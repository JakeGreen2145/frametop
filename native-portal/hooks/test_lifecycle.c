#include "observer-under-test.c"
#include <assert.h>
_Static_assert(_Generic(&head,_Atomic uint64_t*:1,default:0),"terminal error-path reads require atomic publication count");
_Static_assert(_Generic(&chead,_Atomic uint64_t*:1,default:0),"critical terminal error-path reads require atomic publication count");
static uint64_t virtual_now;
int __real_clock_gettime(clockid_t,struct timespec*);
int __wrap_clock_gettime(clockid_t id,struct timespec *t) {
    if(!virtual_now)return __real_clock_gettime(id,t);
    assert(id==CLOCK_MONOTONIC);t->tv_sec=virtual_now/1000000000;t->tv_nsec=virtual_now%1000000000;return 0;
}
static void put64(unsigned char *p,unsigned off,uint64_t v) {memcpy(p+off,&v,8);}
static void flush(void) {
    while(tail<head || ctail<chead) {
        struct event *e;
        if(ctail<chead && (tail==head || critical_ring[ctail%CRITICAL_RING_COUNT].seq<ring[tail%RING_COUNT].seq))
            e=&critical_ring[ctail++%CRITICAL_RING_COUNT];
        else e=&ring[tail++%RING_COUNT];
        assert(write_all(e,sizeof(*e)));
    }
}
static void cache_transition(struct saved *r,unsigned before,unsigned target) {
    static unsigned char layer[0x26000];
    layer[0x2568c]=before;r->x[20]=(uintptr_t)layer;r->x[27]=target;r->x[0]=(uintptr_t)layer+0x25000;
    observe(r,ACTIVITY_COMPARE);r->x[0]=1;observe(r,LAYER_TRANSITION_RESULT);
    r->x[0]=(uintptr_t)layer+0x25000;layer[0x2568c]=target;observe(r,CACHE_POSTSTORE);
}
static void *external_thread(void *unused) {
    (void)unused;struct saved r={0};observe(&r,EXTERNAL_RELEASE);return NULL;
}
int main(int argc,char **argv) {
    assert(argc==2 || argc==3);self_pid=getpid();deadline=now_ns()+10000000000ULL;
    atomic_store(&enabled,1);struct saved legacy={0};observe(&legacy,ACQUIRE_RETURN_TRUE);
    assert(head==1 && ring[0].magic==MAGIC && ring[0].used==0 && !(ring[0].flags&16));
    head=tail=0;memset(&tls,0,sizeof(tls));
    lifecycle=1;sample_every=100;window_frames=2;atomic_store(&enabled,1);
    output_fd=open(argv[1],O_CREAT|O_EXCL|O_WRONLY,0600);assert(output_fd>=0);
    static unsigned char renderer[0x1700],packet[0x2b0],k[0x400],stack[0x2000],state[0x660],wrapper[32],resource[0xc0],command[0x800],blend[40];
    memset(k+0x340,0x42,40);put64(renderer,0x1380,(uintptr_t)wrapper);
    put64(wrapper,8,(uintptr_t)command);put64(renderer,0x1390,(uintptr_t)resource);
    put64(renderer,0x328,0x100000040ULL);
    put64(resource,0xb0,0x1234);put64(resource,0xb8,0x80000);
    put64(packet,8,(uintptr_t)blend);put64(packet,0x38,(uintptr_t)wrapper);put64(packet,0x58,(uintptr_t)wrapper);
    uint32_t vs=58;memcpy(packet+0x20,&vs,4);
    const char name[]="Cam: Room View";
    put64(packet,0x160,(uintptr_t)name);put64(packet,0x168,strlen(name));
    struct saved r={0};r.sp=(uintptr_t)stack;r.x[0]=(uintptr_t)renderer;
    if(argc==3) {
        virtual_now=1;deadline=120000000001ULL;
        uint64_t final_frame=0;
        for(unsigned frame=0;frame<10800;frame++) {
            virtual_now=1+(uint64_t)frame*120000000000ULL/10800;
            r.x[0]=(uintptr_t)renderer;observe(&r,OUTER_ENTER);
            if(frame==100 || frame==9000) {
                r.x[0]=(uintptr_t)k;observe(&r,frame==100?ACQUIRE_ENTER:RELEASE_ENTER);
                r.x[0]=1;observe(&r,frame==100?ACQUIRE_RETURN_TRUE:RELEASE_RETURN_SLOW);
                cache_transition(&r,frame==100?0:1,frame==100?1:0);
                final_frame=frame+1;
            }
            r.x[19]=(uintptr_t)state;observe(&r,POLICY_FEEDBACK);
            r.x[0]=(uintptr_t)renderer;r.x[1]=(uintptr_t)packet;r.x[2]=(uintptr_t)name;r.x[27]=(uintptr_t)k;
            r.x[19]=0;observe(&r,CAMERA_ENQUEUE);r.x[19]=1;observe(&r,CAMERA_ENQUEUE);
            observe(&r,STEREO_FINISH);observe(&r,OUTER_RETURN);
            for(unsigned eye=0;eye<2;eye++) {observe(&r,BACKEND_ENTER);r.x[24]=(uintptr_t)packet;observe(&r,PIPELINE_RESULT);observe(&r,BACKEND_RETURN);}
            flush();
        }
        assert(final_frame==9001 && atomic_load(&enabled) && atomic_load(&frame_seq)==10800 && !tls.depth);
        assert(!atomic_load(&dropped) && !atomic_load(&read_failures) && !atomic_load(&scope_errors));
        virtual_now=deadline;drain(NULL);
        assert(!atomic_load(&enabled) && head==tail && chead==ctail);
        close(output_fd);puts("PASS synthetic 120-second off/on/off, final critical release retained, coherent stereo windows");return 0;
    }
    observe(&r,OUTER_ENTER);
    assert(ring[0].flags&64);
    r.x[3]=1;observe(&r,POLICY_STYLE);r.x[3]=2;observe(&r,POLICY_STYLE);
    r.x[19]=(uintptr_t)state;r.x[20]=(uintptr_t)stack;r.x[0]=(uintptr_t)stack;
    observe(&r,POLICY_FEEDBACK);
    r.x[0]=(uintptr_t)renderer;r.x[1]=(uintptr_t)packet;r.x[2]=(uintptr_t)name;r.x[27]=(uintptr_t)k;
    r.x[19]=0;observe(&r,CAMERA_ENQUEUE);r.x[19]=1;observe(&r,CAMERA_ENQUEUE);
    observe(&r,OUTER_RETURN);assert(tls.depth==0);
    observe(&r,BACKEND_ENTER);r.x[24]=(uintptr_t)packet;
    atomic_store(&window_open,0);uint64_t before_pipeline=head;observe(&r,PIPELINE_RESULT);
    assert(head==before_pipeline+1);observe(&r,BACKEND_RETURN);atomic_store(&window_open,1);
    flush();
    /* Recorder epilogue across native snapshots; no proprietary code execution. */
    put64(stack,0x98,(uintptr_t)packet);put64(stack,0x60,(uintptr_t)packet-0x150);
    r.x[21]=(uintptr_t)renderer;observe(&r,RECORD_COMPLETE);
    /* Exact submitted command + token + result, then captured wait token at RET. */
    unsigned char token[16]={0};put64(token,0,0x1122334455667788ULL);token[8]=3;
    r.x[19]=(uintptr_t)command;r.x[0]=0;observe(&r,SUBMIT_CALL);observe(&r,SUBMIT_RESULT);
    r.x[0]=(uintptr_t)command;r.x[1]=(uintptr_t)resource;r.x[2]=(uintptr_t)token;observe(&r,SUBMIT_STAMP);
    observe(&r,RESOURCE_STAMP);r.x[19]=(uintptr_t)resource;observe(&r,RESOURCE_STAMPED);
    r.x[19]=(uintptr_t)command;observe(&r,SUBMIT_STAMPED);
    r.x[0]=(uintptr_t)resource;r.x[1]=(uintptr_t)token;r.x[2]=0;observe(&r,WAIT_ENTER);
    memset(token,0xff,sizeof(token));r.x[0]=0;observe(&r,WAIT_RESULT);r.x[0]=1;observe(&r,WAIT_RETURN);
    r.x[0]=(uintptr_t)resource;r.x[1]=(uintptr_t)token;observe(&r,WAIT_ENTER);
    r.x[0]=(uint32_t)-4;observe(&r,WAIT_RESULT);r.x[0]=0;observe(&r,WAIT_RETURN_ERROR);
    r.x[0]=(uintptr_t)renderer;observe(&r,RENDERER_WAIT);r.x[0]=1;observe(&r,RENDERER_WAIT_RETURN);
    r.x[22]=(uintptr_t)renderer;observe(&r,ARENA_RESET);
    r.x[1]=(uintptr_t)packet;r.x[0]=(uintptr_t)renderer;observe(&r,BACKEND_ENTER);observe(&r,BACKEND_RETURN_CLEANUP);
    r.x[0]=(uintptr_t)renderer;observe(&r,OUTER_ENTER);cache_transition(&r,0,1);observe(&r,OUTER_RETURN);
    for(unsigned id=POLICY_STYLE;id<SITE_COUNT;id++)observe(&r,id);
    flush();
    /* RET x19 belongs to the caller, not entry K. */
    r.x[0]=(uintptr_t)k;observe(&r,ACQUIRE_ENTER);r.x[0]=1;r.x[19]=1;
    observe(&r,ACQUIRE_RETURN_TRUE);flush();
    /* Every unrecorded outer invocation still advances and closes its scope. */
    for(unsigned i=0;i<8;i++) {r.x[0]=(uintptr_t)renderer;observe(&r,OUTER_ENTER);observe(&r,OUTER_RETURN);flush();}
    assert(atomic_load(&frame_seq)==10 && tls.depth==0 && atomic_load(&sampled_out)>0);
    /* A resource edge on a different callback thread requests the owner's next whole window. */
    pthread_t external;assert(!pthread_create(&external,NULL,external_thread,NULL));assert(!pthread_join(external,NULL));
    flush();uint64_t before_window=head;r.x[0]=(uintptr_t)renderer;observe(&r,OUTER_ENTER);observe(&r,OUTER_RETURN);
    assert(head==before_window+2);flush();
    /* Normal budget exhaustion must not close critical admission. */
    head=tail=EVENT_LIMIT;uint64_t before=chead;
    r.x[0]=(uintptr_t)k;observe(&r,RELEASE_ENTER);r.x[0]=1;observe(&r,RELEASE_RETURN_SLOW);
    assert(chead==before+2 && atomic_load(&enabled));flush();
    /* Independent queue reserve, explicit overflow and budget accounting. */
    for(unsigned i=0;i<CRITICAL_RING_COUNT;i++)observe(&r,EXTERNAL_RELEASE);
    assert(chead-ctail==CRITICAL_RING_COUNT);observe(&r,EXTERNAL_RELEASE);
    assert(atomic_load(&critical_dropped)==1);flush();
    chead=ctail=CRITICAL_LIMIT;observe(&r,EXTERNAL_RELEASE);
    assert(atomic_load(&critical_budget_drops)==1 && atomic_load(&enabled));
    assert(control_event(2));close(output_fd);
    output_fd=-1;head=tail=chead=ctail=0;observe(&r,EXTERNAL_RELEASE);
    assert(chead==1);atomic_store(&stop_drain,1);drain(NULL);
    assert(atomic_load(&write_failures)==1 && !atomic_load(&enabled));
    puts("PASS lifecycle coherent windows, postcall K, independent budgets/queues, scope accounting and explicit loss");
}
