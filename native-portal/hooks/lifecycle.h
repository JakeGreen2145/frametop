/* Included after the fault-safe chunk writer. No native calls or mutations. */
static void copied_chunk(struct event *e,unsigned tag,uint64_t address,const void *p,size_t n,int valid) {
    if(n>sizeof(e->data)-16 || e->used>sizeof(e->data)-16-n) {e->flags|=8;return;}
    struct chunk c={tag|(valid?0:0x80000000u),(uint32_t)n,address};
    memcpy(e->data+e->used,&c,16);
    if(valid)memcpy(e->data+e->used+16,p,n);else {memset(e->data+e->used+16,0,n);e->flags|=4;}
    e->used+=16+n;
}
static uint64_t pointer_chunk(struct event *e,unsigned tag,uint64_t address) {
    unsigned at=e->used;chunk(e,tag,address,8);
    struct chunk c;uint64_t p=0;memcpy(&c,e->data+at,16);
    if(!(c.tag&0x80000000u))memcpy(&p,e->data+at+16,8);return p;
}
static struct call_scope *push_call(struct call_stack *stack,const struct saved *r,uint64_t object,unsigned selected) {
    if(stack->depth==4 || stack->overflow) {++stack->overflow;atomic_fetch_add(&scope_errors,1);return NULL;}
    struct call_scope *c=&stack->item[stack->depth++];
    *c=(struct call_scope){.sp=r->sp,.lr=r->x[30],.object=object,.selected=selected,.valid=1};return c;
}
static struct call_scope *top_call(struct call_stack *stack) {
    return stack->depth && !stack->overflow?&stack->item[stack->depth-1]:NULL;
}
static void pop_call(struct call_stack *stack,const struct saved *r,int check_sp) {
    if(stack->overflow) {--stack->overflow;return;}
    struct call_scope *c=top_call(stack);
    if(!c || (check_sp && (c->sp!=r->sp || c->lr!=r->x[30])))atomic_fetch_add(&scope_errors,1);
    if(c)--stack->depth;
}
static void policy_value(unsigned field,uint64_t value) {
    uint64_t bit=UINT64_C(1)<<field;
    if((tls.policy.seen&bit) && tls.policy.value[field]!=value)tls.policy.conflict|=bit;
    tls.policy.seen|=bit;tls.policy.value[field]=value;
}
static void policy_tap(const struct saved *r,unsigned id) {
    uint32_t scalar;memcpy(&scalar,r->q[0],4);
    switch(id) {
    case POLICY_STYLE:policy_value(0,(uint32_t)r->x[3]);break;
    case POLICY_ENABLE_SPECIAL:policy_value(1,(uint32_t)r->x[1]);policy_value(2,r->x[2]);break;
    case POLICY_SPECIAL_SCALAR:policy_value(3,scalar);break;
    case POLICY_ORDINARY:policy_value(4,0);break;
    case POLICY_SPECIAL:policy_value(4,1);break;
    case POLICY_REQUEST:policy_value(5,(uint32_t)r->x[1]);break;
    case POLICY_CONSTRUCT:policy_value(6,(uint32_t)r->x[0]);break;
    case POLICY_FORCED_INITIAL:policy_value(7,(uint32_t)r->x[0]);break;
    case POLICY_FORCED_INACTIVE:policy_value(8,(uint32_t)r->x[0]);break;
    case POLICY_FORCED_ALPHA:policy_value(9,(uint32_t)r->x[1]);break;
    case POLICY_FORCED_FINAL:policy_value(10,(uint32_t)r->x[23]);break;
    case POLICY_SUPPRESSION:policy_value(11,(uint32_t)r->x[0]);break;
    case POLICY_BASE:policy_value(12,scalar);break;
    default:break;
    }
    /* Repeated forced loads disagreeing across distinct branches are not zero. */
    for(unsigned a=7;a<=10;a++)for(unsigned b=a+1;b<=10;b++)
        if((tls.policy.seen&(UINT64_C(1)<<a)) && (tls.policy.seen&(UINT64_C(1)<<b)) && tls.policy.value[a]!=tls.policy.value[b])
            tls.policy.conflict|=(UINT64_C(1)<<a)|(UINT64_C(1)<<b);
}
static void renderer_metadata(struct event *e,uint64_t r) {
    uint64_t command=pointer_chunk(e,15,r+0x1380);
    chunk(e,16,command+8,8);chunk(e,17,r+0x1388,8);
    uint64_t arena=pointer_chunk(e,18,r+0x1390);
    chunk(e,19,arena+0xb0,16);chunk(e,20,r+0x1410,12);chunk(e,21,r+0x328,8);
    chunk(e,22,r+0x15a0,16);chunk(e,23,arena+0x10,17);
}
static void packet_metadata(struct event *e,uint64_t packet,int recorded) {
    uint64_t blend=pointer_chunk(e,24,packet+8);chunk(e,25,blend+8,32);
    for(unsigned off=0x38;off<=0x58;off+=0x20) {
        uint64_t wrapper=pointer_chunk(e,26,packet+off);
        uint64_t resource=pointer_chunk(e,27,wrapper+8);
        chunk(e,28,wrapper+0x18,4);chunk(e,29,resource+0xb0,16);
    }
    if(recorded) {
        uint64_t name=pointer_chunk(e,13,packet+0x160);
        unsigned at=e->used;chunk(e,31,packet+0x168,8);
        struct chunk c;uint64_t length=0;memcpy(&c,e->data+at,16);
        if(!(c.tag&0x80000000u)) {
            memcpy(&length,e->data+at+16,8);
            if(length)chunk(e,14,name,length<15?length:15);
            else copied_chunk(e,14,name,NULL,0,1);
        }
    }
}
/* 9eebc is a common join, not an unconditional store completion. x0 is the
 * ldrb Boolean on b.eq, or the restored owner on the store arm. x20 remains
 * the base on both. Existing 9eeb4 marks the store arm without moving the
 * post-commit observation or adding a native call. Association is one-use,
 * bound to the current outer scope, layer SP, address and target byte. */
static void cache_tap(const struct saved *r,unsigned id,struct scope *s,
                      struct cache_result *out,uint64_t *address,unsigned char *byte,int *ok) {
    if(id!=ACTIVITY_COMPARE && id!=LAYER_TRANSITION_RESULT && id!=CACHE_POSTSTORE)return;
    uint64_t at=r->x[20]<=UINT64_MAX-0x2568c?r->x[20]+0x2568c:0;
    struct cache_scope *c=s?&s->cache:NULL;
    if(id==ACTIVITY_COMPARE) {
        if(!c)return;
        *c=(struct cache_scope){.address=at,.sp=r->sp,.target=(uint32_t)r->x[27]};
        unsigned char before=0;int read_ok=at && read_meta(&before,at,1);
        if(!read_ok)atomic_fetch_add(&read_failures,1);
        c->before=before;
        c->valid=read_ok && !tls.ambiguous && before<=1 && c->target<=1 && r->x[0]==at-0x68c;
        return;
    }
    int associated=c && c->valid && !tls.ambiguous && c->address==at && c->sp==r->sp && c->target==(uint32_t)r->x[27];
    if(id==LAYER_TRANSITION_RESULT) {
        if(c) {c->store_path=associated;c->valid=associated;}
        return;
    }
    *address=at;*ok=at && read_meta(byte,at,1);
    if(!*ok)atomic_fetch_add(&read_failures,1);
    if(*ok)out->after=*byte;
    if(associated) {
        out->before=c->before;out->target=c->target;
        if(*ok && *byte==c->target) {
            if(c->store_path && c->before!=c->target && r->x[0]==at-0x68c)out->status=2;
            else if(!c->store_path && c->before==c->target && r->x[0]==c->target)out->status=1;
        }
    }
    if(c)c->valid=0; /* A second join or an unmatched frame is unknown, not an edge. */
}
static int resource_edge(unsigned id) {
    return id==EXTERNAL_ACQUIRE || id==EXTERNAL_RELEASE || id==LAYER_TRANSITION_RESULT || id==LAYER_ACQUIRE_RESULT ||
        (id>=ACQUIRE_ENTER && id<=RELEASE_RETURN_SLOW);
}
static int resource_return(unsigned id) {
    return id==ACQUIRE_RETURN_FALSE || id==ACQUIRE_RETURN_TRUE || id==RELEASE_RETURN_FAST || id==RELEASE_RETURN_SLOW;
}
static void call_chunk(struct event *e,unsigned tag,struct call_scope *c,const struct saved *r,int check_sp) {
    int valid=c && c->valid && (!check_sp || (c->sp==r->sp && c->lr==r->x[30]));
    if(!valid)e->flags|=16;
    copied_chunk(e,tag,0,c,sizeof(struct call_scope),valid);
}
