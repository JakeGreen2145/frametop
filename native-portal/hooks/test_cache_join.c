/* Production callback regression. Synthetic native state, never a VR call. */
#include "observer-under-test.c"
#include <assert.h>
static uint64_t virtual_now=1;
int __wrap_clock_gettime(clockid_t id,struct timespec *t) {
    assert(id==CLOCK_MONOTONIC);t->tv_sec=virtual_now/1000000000;t->tv_nsec=virtual_now%1000000000;return 0;
}
static unsigned char *mem;
static const uintptr_t owner=0x40025000,cache=0x4002568c,stack=0x40040000;
static unsigned joins,stored,late_camera,final_release,deferred;
struct fixture_cache_result {uint32_t before,target,after,status;};
static int check_association=1;
static void flush(void) {
    while(tail<head || ctail<chead) {
        struct event *e;
        if(ctail<chead && (tail==head || critical_ring[ctail%CRITICAL_RING_COUNT].seq<ring[tail%RING_COUNT].seq))e=&critical_ring[ctail++%CRITICAL_RING_COUNT];
        else e=&ring[tail++%RING_COUNT];
        if(e->site==CACHE_POSTSTORE) {
            ++joins;struct chunk c;memcpy(&c,e->data,16);
            if(check_association) {assert(c.tag==11 && c.address==cache && c.len==1);assert(!(e->flags&4));}
            if(check_association) {
                struct chunk status_chunk;struct fixture_cache_result result;
                memcpy(&status_chunk,e->data+17,16);memcpy(&result,e->data+33,16);
                assert(status_chunk.tag==41 && status_chunk.len==16);
                assert(result.status==((e->flags&32)?2u:1u));
                assert(result.after==e->data[16] && result.after==result.target);
                assert((result.before!=result.target)==!!(e->flags&32));
            }
            if(e->flags&32)++stored;
        }
        if(e->site==CAMERA_ENQUEUE && e->frame>10000)++late_camera;
        if(e->site==RELEASE_RETURN_SLOW && e->frame>10000)++final_release;
        if(e->site==BACKEND_ENTER && !e->frame)++deferred;
    }
}
static void put64(uintptr_t address,uint64_t value) {memcpy((void*)address,&value,8);}
static void join(struct saved *r,unsigned before,unsigned target) {
    *(unsigned char*)cache=before;r->x[20]=0x40000000;r->x[27]=target;r->x[0]=owner;
    observe(r,ACTIVITY_COMPARE);
    /* ldrb w0 destroys the pointer on the unchanged b.eq arm. */
    r->x[0]=before;
    if(before!=target) {
        r->x[0]=1;observe(r,LAYER_TRANSITION_RESULT);
        r->x[0]=owner;*(unsigned char*)cache=target;
    }
    observe(r,CACHE_POSTSTORE);
}
static void enter(struct saved *r) {r->sp=stack;r->x[0]=0x40050000;observe(r,OUTER_ENTER);}
int main(int argc,char **argv) {
    assert(argc==2 || argc==3);self_pid=getpid();deadline=120000000001ULL;
    lifecycle=1;sample_every=600;window_frames=3;atomic_store(&enabled,1);
    mem=mmap((void*)0x40000000,0x80000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
    assert(mem==(void*)0x40000000);
    struct saved r={0};r.sp=stack;
    if(!strcmp(argv[1],"replay")) {
        assert(argc==3);FILE *f=fopen(argv[2],"r");assert(f);unsigned id,byte;
        while(fscanf(f,"%u %lx %lx %lx %lx %u",&id,&r.x[0],&r.x[20],&r.x[27],&r.sp,&byte)==6) {
            *(unsigned char*)cache=byte;observe(&r,id);flush();
        }
        assert(!ferror(f));fclose(f);
        assert(joins && stored==2 && !atomic_load(&read_failures));
        printf("PASS instruction-emulated cache branch replay: %u joins, %u committed changes\n",joins,stored);
    } else if(!strcmp(argv[1],"unknown")) {
        for(unsigned variant=0;variant<9;variant++) {
            enter(&r);join(&r,0,0);flush();
            r.x[0]=owner;r.x[20]=0x40000000;r.x[27]=1;*(unsigned char*)cache=0;
            if(variant!=0)observe(&r,ACTIVITY_COMPARE);
            if(variant==1)r.sp+=16;
            if(variant==2)r.x[20]+=16;
            if(variant==3)r.x[27]=2;
            if(variant==4)r.x[20]=UINT64_MAX;
            if(variant==5)tls.ambiguous=1;
            if(variant==6)observe(&r,LAYER_TRANSITION_RESULT); /* Store has not executed. */
            if(variant==7) {observe(&r,OUTER_RETURN);enter(&r);} /* Stale comparison. */
            if(variant!=6)*(unsigned char*)cache=1; /* No store-path marker, never infer change. */
            atomic_store(&requested_window,0);atomic_store(&window_open,1);tls.scope[tls.depth-1].selected=1;
            uint64_t c=chead;observe(&r,CACHE_POSTSTORE);
            assert(chead==c && !atomic_load(&requested_window));
            struct event *e=&ring[(head-1)%RING_COUNT];struct fixture_cache_result result;
            assert(e->site==CACHE_POSTSTORE && (e->flags&16));memcpy(&result,e->data+33,16);assert(!result.status);
            if(variant==4)assert(e->flags&4);
            tail=head;ctail=chead;r.sp=stack;tls.ambiguous=0;observe(&r,OUTER_RETURN);flush();
        }
        uint64_t c=chead;
        for(unsigned i=0;i<10800;i++) {
            enter(&r);r.x[20]=UINT64_MAX;r.x[0]=0;observe(&r,CACHE_POSTSTORE);observe(&r,OUTER_RETURN);
            tail=head;ctail=chead;assert(!atomic_load(&requested_window));
        }
        assert(chead==c && head<EVENT_LIMIT && !atomic_load(&dropped));
        printf("PASS unknown/malformed/missing-marker/stale-scope guards, 10800 unknown joins bounded: normal=%lu critical=%lu\n",(unsigned long)head,(unsigned long)chead);
    } else if(!strcmp(argv[1],"association")) {
        for(unsigned i=0;i<4004;i++) {
            enter(&r);unsigned before=i>2000 && i<=4000,target=i>=2000 && i<4000;
            uint64_t c=chead;atomic_store(&requested_window,0);
            join(&r,before,target);flush();
            assert((before!=target) || (chead==c && !atomic_load(&requested_window)));
            observe(&r,OUTER_RETURN);flush();
        }
        assert(stored==2 && !atomic_load(&read_failures));
        printf("PASS steady 0/1 association: %u sampled joins, two committed transitions\n",joins);
    } else {
        assert(!strcmp(argv[1],"schedule"));check_association=0;
        uintptr_t renderer=0x40050000,packet=0x40052000,k=0x40054000,state=0x40056000,resource=0x40058000,wrapper=0x40059000,token=0x4005a000,blend=0x4005b000,name=0x4005c000;
        put64(renderer+0x1380,wrapper);put64(renderer+0x1390,resource);put64(wrapper+8,resource);
        put64(packet+8,blend);put64(packet+0x38,wrapper);put64(packet+0x58,wrapper);
        *(uint32_t*)(packet+0x20)=58;put64(packet+0x160,name);put64(packet+0x168,0);
        for(unsigned frame=0;frame<10800;frame++) {
            virtual_now=1+(uint64_t)frame*120000000000ULL/10800;enter(&r);
            unsigned before=frame>900 && frame<=10500,target=frame>=900 && frame<10500;
            if(frame==900 || frame==10500) {
                r.x[0]=k;observe(&r,frame==900?ACQUIRE_ENTER:RELEASE_ENTER);
                r.x[0]=1;observe(&r,frame==900?ACQUIRE_RETURN_TRUE:RELEASE_RETURN_SLOW);
            }
            join(&r,before,target);join(&r,target,target);
            r.x[19]=state;observe(&r,POLICY_FEEDBACK);
            for(unsigned tap=POLICY_STYLE;tap<SITE_COUNT;tap++)observe(&r,tap);
            r.x[0]=renderer;r.x[1]=packet;r.x[2]=name;r.x[27]=k;
            r.x[19]=0;observe(&r,CAMERA_ENQUEUE);r.x[19]=1;observe(&r,CAMERA_ENQUEUE);
            observe(&r,STEREO_FINISH);observe(&r,OUTER_RETURN);
            /* Work really occurs after outer return, before next outer entry. */
            for(unsigned eye=0;eye<2;eye++) {
                r.x[0]=renderer;r.x[1]=packet;observe(&r,BACKEND_ENTER);r.x[24]=packet;
                observe(&r,PIPELINE_RESULT);observe(&r,BACKEND_RETURN);
            }
            /* Live trial had ~16.5 stamp pairs and ~4 waits per outer invocation. */
            for(unsigned j=0;j<17;j++) {
                r.x[0]=resource;r.x[2]=token;observe(&r,RESOURCE_STAMP);
                r.x[19]=resource;observe(&r,RESOURCE_STAMPED);
            }
            for(unsigned j=0;j<4;j++) {
                r.x[0]=resource;r.x[1]=token;observe(&r,WAIT_ENTER);
                r.x[0]=0;observe(&r,WAIT_RESULT);r.x[0]=1;observe(&r,WAIT_RETURN);
            }
            r.x[22]=renderer;observe(&r,ARENA_RESET);flush();
            assert(head<EVENT_LIMIT && chead<CRITICAL_LIMIT);
        }
        assert(late_camera && final_release==1 && deferred && stored==2);
        assert(!atomic_load(&dropped) && !atomic_load(&read_failures) && !atomic_load(&scope_errors));
        printf("PASS 120s/90Hz, 21600 joins, 183600 stamp pairs: normal=%lu critical=%lu late-camera=%u final-release=%u deferred=%u\n",(unsigned long)head,(unsigned long)chead,late_camera,final_release,deferred);
    }
    assert(!tls.depth && atomic_load(&enabled));return 0;
}
