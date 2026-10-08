#define _GNU_SOURCE
#include <errno.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <link.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/sysmacros.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <openssl/evp.h>
#include "pins.h"
#include "emit.h"
#include "identity.h"
#if !defined(__aarch64__) || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error This observer is little-endian AArch64 only.
#endif
#define RING_COUNT 256
#define EVENT_LIMIT 16384
#define CRITICAL_LIMIT 4096
#define CRITICAL_RING_COUNT 128
#define MAGIC 0x3154464e
struct event {
    uint32_t magic,size,site,flags;
    uint64_t seq,frame,thread_seq,tid,mono,owner;
    uint32_t depth,eye,mask,used;
    uint64_t x[31],sp;
    unsigned char data[2048];
};
_Static_assert(sizeof(struct event)==2384,"file format v1");
struct chunk {uint32_t tag,len; uint64_t address;};
struct policy_loads {uint64_t seen,conflict,value[14];};
struct call_scope {uint64_t sp,lr,object,aux; unsigned selected,valid; unsigned char token[16];};
struct call_stack {struct call_scope item[4]; unsigned depth,overflow;};
struct cache_scope {uint64_t address,sp; uint32_t before,target,valid,store_path;};
struct cache_result {uint32_t before,target,after,status;}; /* status: 0 unknown, 1 unchanged, 2 committed change */
struct scope {uint64_t frame,owner,sp; uint32_t eye,mask,selected; struct cache_scope cache;};
static __thread struct {
    struct scope scope[4]; unsigned depth,busy,ambiguous;
    uint64_t seq,tid;
    unsigned pending,previous_selected,policy_valid;
    struct policy_loads policy;
    unsigned char previous_policy[sizeof(struct policy_loads)+38];
    struct call_stack resource,backend,wait,renderer_wait;
} tls __attribute__((tls_model("initial-exec")));
static struct event ring[RING_COUNT],critical_ring[CRITICAL_RING_COUNT];
static int lifecycle;
static unsigned sample_every=120,window_frames=3;
static _Atomic unsigned window_open,requested_window;
static atomic_flag ring_lock=ATOMIC_FLAG_INIT;
/* Terminal diagnostics can run after a failed write while a producer finishes
 * its already-admitted copy. Keep published counts atomic on that error path. */
static _Atomic uint64_t head,chead;
static uint64_t tail,ctail;
static _Atomic uint64_t critical_dropped,critical_budget_drops,normal_budget_drops,sampled_out,write_failures;
static _Atomic uint64_t frame_seq,dropped,read_failures,filtered,reentrant,scope_errors;
static _Atomic int enabled,stop_drain;
static uint64_t deadline;
static pid_t self_pid;
static int output_fd=-1;
static pthread_t drainer;
static int drainer_started;
static uintptr_t image_base;
static uint64_t now_ns(void) {
    struct timespec t;
    if(clock_gettime(CLOCK_MONOTONIC,&t))return 0;
    return (uint64_t)t.tv_sec*1000000000u+t.tv_nsec;
}
static int fresh_log(const char *path) {
    if(!path || path[0]!='/' || strlen(path)>=PATH_MAX)return -1;
    char parent[PATH_MAX],real[PATH_MAX];strcpy(parent,path);
    char *slash=strrchr(parent,'/');
    if(!slash || !slash[1] || !strcmp(slash+1,".") || !strcmp(slash+1,".."))return -1;
    *slash=0;
    if(!*parent || !realpath(parent,real) || strcmp(parent,real))return -1;
    struct stat st;
    if(stat(parent,&st) || st.st_uid!=geteuid() || (st.st_mode&0777)!=0700)return -1;
    int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK,0600);
    if(fd<0)return -1;
    if(fstat(fd,&st) || !S_ISREG(st.st_mode) || st.st_uid!=geteuid() ||
       (st.st_mode&0777)!=0600 || st.st_nlink!=1 || st.st_size!=0) {close(fd);return -1;}
    return fd;
}
static int write_all(const void *p,size_t size) {
    /* Only setup/drainer calls this. A regular private fd, never a pipe/socket. */
    const unsigned char *b=p;
    while(size) {ssize_t n=write(output_fd,b,size);if(n<0 && errno==EINTR)continue;
        if(n<=0)return 0;b+=n;size-=n;}
    return 1;
}
static int control_event(unsigned code) {
    struct event e={.magic=MAGIC,.size=sizeof(e),.site=UINT32_MAX,.flags=code,.mono=now_ns(),.owner=image_base};
    e.x[0]=self_pid;e.x[1]=atomic_load(&dropped);e.x[2]=atomic_load(&read_failures);
    e.x[3]=atomic_load(&filtered);e.x[4]=atomic_load(&reentrant);e.x[5]=atomic_load(&scope_errors);
    if(lifecycle) {e.magic=0x3254464e;e.x[6]=atomic_load(&critical_dropped);
        e.x[7]=atomic_load(&critical_budget_drops);e.x[8]=atomic_load(&normal_budget_drops);
        e.x[9]=atomic_load(&sampled_out);e.x[10]=atomic_load(&write_failures);
        e.x[11]=head;e.x[12]=chead;e.x[13]=sample_every;e.x[14]=window_frames;
        e.x[15]=EVENT_LIMIT;e.x[16]=CRITICAL_LIMIT;e.x[17]=(EVENT_LIMIT+CRITICAL_LIMIT+3ULL)*sizeof(e);}
    memcpy(e.data,EXE_SHA,64);e.used=64;
    return write_all(&e,sizeof(e));
}
static _Noreturn void fatal(unsigned why) {
    (void)control_event(why);syscall(SYS_exit_group,190);__builtin_trap();
}
static int read_meta(void *dst,uintptr_t src,size_t n) {
    if(!src || !n || n>2048 || src>UINTPTR_MAX-n)return 0;
    struct iovec local={dst,n},remote={(void*)src,n};
    return syscall(SYS_process_vm_readv,self_pid,&local,1,&remote,1,0)==(ssize_t)n;
}
static void chunk(struct event *e,unsigned tag,uintptr_t address,size_t n) {
    if(n>sizeof(e->data)-sizeof(struct chunk) || e->used>sizeof(e->data)-n-sizeof(struct chunk)) {e->flags|=8;return;}
    struct chunk c={tag,(uint32_t)n,address};
    memcpy(e->data+e->used,&c,sizeof(c));
    if(!read_meta(e->data+e->used+sizeof(c),address,n)) {
        memset(e->data+e->used+sizeof(c),0,n);e->flags|=4;
        atomic_fetch_add_explicit(&read_failures,1,memory_order_relaxed);
        c.tag|=0x80000000u;memcpy(e->data+e->used,&c,sizeof(c));
    }
    e->used+=sizeof(c)+n;
}
#include "lifecycle.h"
/* Never called by native code directly: emit_island saves every live machine register. */
static void observe(const struct saved *r,unsigned id) {
    int saved_errno=errno;
    if(!atomic_load_explicit(&enabled,memory_order_relaxed) || id>=SITE_COUNT || (!lifecycle && id>OUTER_RETURN))goto done;
    if((pid_t)syscall(SYS_getpid)!=self_pid) {atomic_store(&enabled,0);goto done;}
    if(tls.busy) {atomic_fetch_add_explicit(&reentrant,1,memory_order_relaxed);goto done;}
    tls.busy=1;
    uint64_t time=now_ns();
    if(!time || time>=deadline) {atomic_store(&enabled,0);goto finish;}
    if(!tls.tid)tls.tid=(uint64_t)syscall(SYS_gettid);
    ++tls.seq;
    /* No normal-return observation after unwind: do not reuse a stale frame on a new outer entry. */
    if(id==OUTER_ENTER) {
        if(tls.depth && r->sp>=tls.scope[tls.depth-1].sp) {tls.depth=0;tls.ambiguous=1;atomic_fetch_add(&scope_errors,1);}
        if(tls.depth==4) {tls.ambiguous=1;atomic_fetch_add(&scope_errors,1);atomic_store(&enabled,0);goto finish;}
        uint64_t frame=atomic_fetch_add(&frame_seq,1)+1;
        unsigned selected=1;
        if(lifecycle) {
            unsigned requested=atomic_exchange(&requested_window,0);
            if(tls.pending<requested)tls.pending=requested;
            if((frame-1)%sample_every==0 && tls.pending<window_frames)tls.pending=window_frames;
            selected=tls.pending!=0;if(tls.pending)--tls.pending;
            memset(&tls.policy,0,sizeof(tls.policy));
            atomic_store(&window_open,selected);
        }
        tls.scope[tls.depth++]=(struct scope){.frame=frame,.owner=r->x[0],.sp=r->sp,.eye=UINT32_MAX,.selected=selected};
    }
    struct scope *s=tls.depth?&tls.scope[tls.depth-1]:NULL;
    if(s && id>=LAYER_LEFT_EARLY && id<=LAYER_RIGHT_LATE)s->eye=(uint32_t)r->x[1];
    if(s && id==CAMERA_ENQUEUE && r->x[19]<2)s->mask|=1u<<r->x[19];
    unsigned selected=!lifecycle || (s?s->selected:atomic_load(&window_open));
    unsigned critical=lifecycle && resource_edge(id);
    struct cache_result cache_result={UINT32_MAX,UINT32_MAX,UINT32_MAX,0};
    uint64_t cache_address=0;unsigned char cache_byte=0;int cache_ok=0;
    unsigned char policy_snapshot[sizeof(struct policy_loads)+38]={0};int policy_ok=1;
    struct call_scope *call=NULL;
    if(lifecycle) {
        cache_tap(r,id,s,&cache_result,&cache_address,&cache_byte,&cache_ok);
        if(id==CACHE_POSTSTORE && cache_result.status==2)critical=1;
        policy_tap(r,id);
        if(id==POLICY_FEEDBACK) {
            memcpy(policy_snapshot,&tls.policy,sizeof(tls.policy));
            policy_ok=read_meta(policy_snapshot+sizeof(tls.policy),r->x[19]+0x62c,36) &&
                read_meta(policy_snapshot+sizeof(tls.policy)+36,r->x[19]+0x1e0,2);
            if(!policy_ok)atomic_fetch_add(&read_failures,1);
            if(!policy_ok || !tls.policy_valid || memcmp(policy_snapshot,tls.previous_policy,sizeof(policy_snapshot))) {
                critical=1;tls.policy_valid=policy_ok;memcpy(tls.previous_policy,policy_snapshot,sizeof(policy_snapshot));
            }
        }
        if(critical)atomic_store(&requested_window,window_frames);
        if(id==ACQUIRE_ENTER || id==RELEASE_ENTER)push_call(&tls.resource,r,r->x[0],1);
        if(resource_return(id))call=top_call(&tls.resource);
        if(id==BACKEND_ENTER)push_call(&tls.backend,r,r->x[1],selected);
        if(id==PIPELINE_RESULT || id==BACKEND_RETURN || id==BACKEND_RETURN_CLEANUP) {
            call=top_call(&tls.backend);selected=call?call->selected:0;
        }
        if(id==WAIT_ENTER) {
            call=push_call(&tls.wait,r,r->x[0],selected);
            if(call) {call->aux=r->x[2];call->valid=read_meta(call->token,r->x[1],9);
                if(!call->valid)atomic_fetch_add(&read_failures,1);}
        }
        if(id==WAIT_RESULT || id==WAIT_RETURN || id==WAIT_RETURN_ERROR) {
            call=top_call(&tls.wait);selected=call?call->selected:0;
        }
        if(id==RENDERER_WAIT)call=push_call(&tls.renderer_wait,r,r->x[0],selected);
        if(id==RENDERER_WAIT_RETURN) {call=top_call(&tls.renderer_wait);selected=call?call->selected:0;}
    }
    if(id==BACKEND_ENTER || id==PIPELINE_RESULT) {
        uint32_t vs=0;uintptr_t p=id==BACKEND_ENTER?r->x[1]:r->x[24];
        int ok=read_meta(&vs,p+0x20,4);
        if(!ok)atomic_fetch_add(&read_failures,1);
        if(!ok || (vs!=57 && vs!=58)) {
            if(lifecycle && id==BACKEND_ENTER && top_call(&tls.backend))top_call(&tls.backend)->selected=0;
            atomic_fetch_add(&filtered,1);goto cleanup;
        }
    }
    if(lifecycle && id==RECORD_COMPLETE && selected) {
        uint64_t source=0;uint32_t vs=0;
        if(!read_meta(&source,r->sp+0x98,8) || !read_meta(&vs,source+0x20,4)) {
            atomic_fetch_add(&read_failures,1);goto cleanup;
        }
        if(vs!=57 && vs!=58) {atomic_fetch_add(&filtered,1);goto cleanup;}
    }
    if(lifecycle && !critical && !selected) {atomic_fetch_add(&sampled_out,1);goto cleanup;}
    if(atomic_flag_test_and_set_explicit(&ring_lock,memory_order_acquire)) {
        atomic_fetch_add(&dropped,1);if(critical)atomic_fetch_add(&critical_dropped,1);goto cleanup;
    }
    /* The drainer closes admission under this lock before testing emptiness.
     * A callback may have passed the initial enabled check before that close. */
    if(!atomic_load_explicit(&enabled,memory_order_relaxed)) {atomic_flag_clear_explicit(&ring_lock,memory_order_release);goto cleanup;}
    if((critical?chead:head)>=(critical?CRITICAL_LIMIT:EVENT_LIMIT)) {
        if(!lifecycle)atomic_store(&enabled,0);
        else {atomic_fetch_add(&dropped,1);atomic_fetch_add(critical?&critical_budget_drops:&normal_budget_drops,1);}
        atomic_flag_clear_explicit(&ring_lock,memory_order_release);goto cleanup;
    }
    if((critical?chead-ctail:head-tail)==(critical?CRITICAL_RING_COUNT:RING_COUNT)) {
        atomic_fetch_add(&dropped,1);if(critical)atomic_fetch_add(&critical_dropped,1);
        atomic_flag_clear_explicit(&ring_lock,memory_order_release);goto cleanup;
    }
    struct event *e=critical?&critical_ring[chead%CRITICAL_RING_COUNT]:&ring[head%RING_COUNT];memset(e,0,sizeof(*e));
    e->magic=lifecycle?0x3254464e:MAGIC;e->size=sizeof(*e);e->site=id;e->seq=head+chead+1;e->thread_seq=tls.seq;
    e->tid=tls.tid;e->mono=time;e->depth=tls.depth;e->flags=(!s?1:0)|(tls.ambiguous?2:0)|(critical?32:0)|(lifecycle && selected?64:0);
    e->frame=s?s->frame:0;e->owner=s?s->owner:0;e->eye=s?s->eye:UINT32_MAX;e->mask=s?s->mask:0;
    memcpy(e->x,r->x,sizeof(e->x));e->sp=r->sp;
    switch(id) {
    case CAMERA_ENQUEUE:
        e->eye=(uint32_t)r->x[19];
        chunk(e,1,r->x[27]+0xc0,528); /* native constants only, no resource/pixel storage */
        chunk(e,2,r->sp+0x170,48); /* SP unchanged from camera prologue through enqueue */
        chunk(e,3,r->x[1],0x160);
        if(lifecycle) {renderer_metadata(e,r->x[0]);packet_metadata(e,r->x[1],0);
            unsigned char name[15];unsigned n=0;int ok=1;
            for(;n<15;n++) {if(!read_meta(name+n,r->x[2]+n,1)) {ok=0;atomic_fetch_add(&read_failures,1);break;}if(!name[n])break;}
            copied_chunk(e,14,r->x[2],name,n,ok);
        }break;
    case BACKEND_ENTER: case PIPELINE_RESULT: {
        uintptr_t p=id==BACKEND_ENTER?r->x[1]:r->x[24];
        chunk(e,3,p,0x160);chunk(e,4,p+0x1c8,9*24);if(lifecycle)packet_metadata(e,p,1);break;
    }
    case TRACKING_INPUT:chunk(e,5,r->x[3],0x50);break; /* HMD pose, original array pointer remains x3 */
    case POLICY_FEEDBACK:
        if(lifecycle) {
            copied_chunk(e,30,0,&tls.policy,sizeof(tls.policy),1);
            copied_chunk(e,32,r->x[19]+0x62c,policy_snapshot+sizeof(tls.policy),36,policy_ok);
            copied_chunk(e,33,r->x[19]+0x1e0,policy_snapshot+sizeof(tls.policy)+36,2,policy_ok);
            break;
        }
        chunk(e,6,r->x[19],0x660); /* original completed pre-feedback S */
        chunk(e,7,r->x[0]+0xb40,0x18); /* H+0x40000 in x0, diagnostic reread, not original load results */
        chunk(e,8,r->x[20]+0x1572,2);break;
    case POLICY_COMPLETE:chunk(e,6,r->x[28],0x660);break;
    case POLICY_SEAL:
        chunk(e,6,r->sp+0x610,0x660);chunk(e,9,r->sp+0xc70,8);
        chunk(e,10,r->sp+0xc70+0xc38,64);break;
    case ACTIVITY_COMPARE:chunk(e,11,r->x[0]+0x68c,1);break;
    case ACQUIRE_ENTER:case RELEASE_ENTER:chunk(e,12,r->x[0]+0x340,0x28);break;
    case CACHE_POSTSTORE:
        copied_chunk(e,11,cache_address,&cache_byte,1,cache_ok);
        copied_chunk(e,41,0,&cache_result,sizeof(cache_result),1);
        if(!cache_result.status)e->flags|=16;
        break;
    case ACQUIRE_RETURN_FALSE:case ACQUIRE_RETURN_TRUE:case RELEASE_RETURN_FAST:case RELEASE_RETURN_SLOW:
        if(!lifecycle)break;
        call_chunk(e,34,call,r,1);
        if(call && call->sp==r->sp && call->lr==r->x[30])chunk(e,12,call->object+0x340,40);
        break;
    case RECORD_COMPLETE: {
        uint64_t src=pointer_chunk(e,35,r->sp+0x98),base=pointer_chunk(e,36,r->sp+0x60);
        (void)src;uint64_t packet=base+0x150;
        chunk(e,3,packet,0x160);chunk(e,4,packet+0x1c8,216);packet_metadata(e,packet,1);
        renderer_metadata(e,r->x[21]);break;
    }
    case BACKEND_RETURN:case BACKEND_RETURN_CLEANUP:call_chunk(e,34,call,r,1);break;
    case ARENA_RESET:renderer_metadata(e,r->x[22]);break;
    case SUBMIT_CALL:case SUBMIT_RESULT:case SUBMIT_STAMPED:
        chunk(e,23,r->x[19]+0x10,17);chunk(e,37,r->x[19]+0x428,8);chunk(e,38,r->x[19]+0x4f0,8);break;
    case SUBMIT_STAMP:case RESOURCE_STAMP:
        chunk(e,39,r->x[2],9);chunk(e,23,r->x[0]+0x10,17);break;
    case RESOURCE_STAMPED:chunk(e,23,r->x[19]+0x10,17);break;
    case WAIT_ENTER:
        call_chunk(e,34,call,r,0);chunk(e,40,r->x[0]+0x58,8);break;
    case WAIT_RESULT:case WAIT_RETURN:case WAIT_RETURN_ERROR:call_chunk(e,34,call,r,id!=WAIT_RESULT);break;
    case RENDERER_WAIT:
        call_chunk(e,34,call,r,0);renderer_metadata(e,r->x[0]);break;
    case RENDERER_WAIT_RETURN:call_chunk(e,34,call,r,0);break;
    default:
        if(lifecycle && id>=POLICY_STYLE)copied_chunk(e,30,0,&tls.policy,sizeof(tls.policy),1);
        break;
    }
    if(critical)++chead;else ++head;atomic_flag_clear_explicit(&ring_lock,memory_order_release);
cleanup:
    if(lifecycle) {
        if(resource_return(id))pop_call(&tls.resource,r,1);
        if(id==BACKEND_RETURN || id==BACKEND_RETURN_CLEANUP)pop_call(&tls.backend,r,1);
        if(id==WAIT_RETURN || id==WAIT_RETURN_ERROR)pop_call(&tls.wait,r,1);
        if(id==RENDERER_WAIT_RETURN && top_call(&tls.renderer_wait))pop_call(&tls.renderer_wait,r,0);
    }
    if(id==OUTER_RETURN && tls.depth) {
        if(s && s->sp!=r->sp) {tls.ambiguous=1;atomic_fetch_add(&scope_errors,1);}
        --tls.depth;
    }
finish:tls.busy=0;
done:errno=saved_errno;
}
static void *drain(void *unused) {
    (void)unused;
    const struct timespec pause={0,10000000};
    for(;;) {
        struct event e;int have=0,finished=0;
        int closing=now_ns()>=deadline || atomic_load(&stop_drain);
        if(!atomic_flag_test_and_set_explicit(&ring_lock,memory_order_acquire)) {
            if(closing)atomic_store(&enabled,0);
            /* Completion requires closed admission and an empty ring in the
             * same critical section, never a cached pre-close empty result. */
            if(ctail<chead && (tail==head || critical_ring[ctail%CRITICAL_RING_COUNT].seq<ring[tail%RING_COUNT].seq)) {
                e=critical_ring[ctail%CRITICAL_RING_COUNT];++ctail;have=1;
            }
            else if(tail<head) {e=ring[tail%RING_COUNT];++tail;have=1;}
            else finished=!atomic_load(&enabled);
            atomic_flag_clear_explicit(&ring_lock,memory_order_release);
        }
        if(have && !write_all(&e,sizeof(e))) {atomic_fetch_add(&write_failures,1);atomic_store(&enabled,0);break;}
        if(finished)break;
        if(!have)nanosleep(&pause,NULL);
    }
    atomic_store(&enabled,0);(void)control_event(atomic_load(&write_failures)?21:2);fsync(output_fd);
    return NULL;
}
static int guards(uintptr_t base) {
    for(unsigned i=0;i<SITE_COUNT;i++)if(memcmp((void*)(base+sites[i].offset),sites[i].guard,16))return 0;
    return 1;
}
/* Caller proves single-thread launch and blocks signals. No callback can run during install. */
static int install_words(uintptr_t base,uintptr_t islands,size_t ps,int (*protect)(void*,size_t,int)) {
    uintptr_t pages[SITE_COUNT];uint32_t words[SITE_COUNT];unsigned count=0;
    if(!ps || (ps&(ps-1)) || !guards(base))return 0;
    for(unsigned i=0;i<SITE_COUNT;i++) {
        uintptr_t page=(base+sites[i].offset)&~(ps-1);unsigned j;
        if(!branch_word(base+sites[i].offset,islands+i*ISLAND_STRIDE,&words[i]))return 0;
        for(j=0;j<count && pages[j]!=page;j++);
        if(j==count)pages[count++]=page;
    }
    unsigned rw=0;
    for(;rw<count;rw++)if(protect((void*)pages[rw],ps,PROT_READ|PROT_WRITE))break;
    if(rw!=count) {
        for(unsigned j=0;j<rw;j++)if(protect((void*)pages[j],ps,PROT_READ|PROT_EXEC))fatal(91);
        return 0;
    }
    for(unsigned i=0;i<SITE_COUNT;i++)__atomic_store_n((uint32_t*)(base+sites[i].offset),words[i],__ATOMIC_RELAXED);
    for(unsigned i=0;i<count;i++)__builtin___clear_cache((char*)pages[i],(char*)pages[i]+ps);
    for(unsigned i=0;i<count;i++)if(protect((void*)pages[i],ps,PROT_READ|PROT_EXEC))fatal(92);
    for(unsigned i=0;i<SITE_COUNT;i++)if(*(uint32_t*)(base+sites[i].offset)!=words[i])fatal(93);
    for(unsigned i=0;i<count;i++)if(!map_range(pages[i],ps,NULL,0,PROT_READ|PROT_EXEC))fatal(94);
    return 1;
}
static void *make_islands(uintptr_t base,size_t ps,size_t *length) {
    *length=(SITE_COUNT*ISLAND_STRIDE+ps-1)&~(ps-1);
    /* At most 120 non-replacing probes, outside this pinned image's low text/data. */
    for(uintptr_t delta=0x800000;delta<0x8000000;delta+=0x100000) {
        uintptr_t at=(base+delta+ps-1)&~(ps-1);
        void *p=mmap((void*)at,*length,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
        if(p==MAP_FAILED)continue;
        int ok=(uintptr_t)p==at;
        for(unsigned i=0;ok && i<SITE_COUNT;i++) {
            size_t n=emit_island((uint32_t*)((char*)p+i*ISLAND_STRIDE),at+i*ISLAND_STRIDE,base,i,(uintptr_t)observe);
            uint32_t word;ok=n && n<ISLAND_STRIDE && branch_word(base+sites[i].offset,at+i*ISLAND_STRIDE,&word);
        }
        if(ok) {__builtin___clear_cache(p,(char*)p+*length);ok=!mprotect(p,*length,PROT_READ|PROT_EXEC);}
        if(ok && map_range(at,*length,NULL,0,PROT_READ|PROT_EXEC))return p;
        munmap(p,*length);
    }
    return MAP_FAILED;
}
#ifndef HOOK_TEST
__attribute__((constructor)) static void launch(void) {
    const char *opt=getenv("FT_NATIVE_HOOKS_ENABLE");
    if(!opt || strcmp(opt,"metadata-v1-9b1bd5fe"))return;
    /* Service-wide preload reaches helpers first. Never reserve their shared log.
     * Keep the full identity check below so intended-target failures still report. */
    char path[PATH_MAX];ssize_t n=readlink("/proc/self/exe",path,sizeof(path)-1);
    if(n<0 || (size_t)n>=sizeof(path)-1)return;
    path[n]=0;if(strcmp(path,EXE))return;
    self_pid=getpid();output_fd=fresh_log(getenv("FT_NATIVE_HOOKS_LOG"));if(output_fd<0)return;
    int fd=exe_fd();unsigned refusal=10;
    if(fd<0)goto refuse;
    /* No sibling mutator may make this metadata-only claim. Stock loader discovery remains untouched. */
    if(getenv("FT_AURORA_GATE_ENABLE") || getenv("FT_NATIVE_CLIP_DIR") || getenv("FT_CAMERA_BLEND_MODE")) {refusal=11;goto refuse;}
    struct main_image m={0};dl_iterate_phdr(main_image_cb,&m);image_base=m.base;
    if(m.seen!=1 || !verify_load(fd,m.base,m.ph,m.count) || !one_thread()) {refusal=12;goto refuse;}
    unsigned seconds=60;const char *limit=getenv("FT_NATIVE_HOOKS_SECONDS");
    if(limit) {char *end;unsigned long n=strtoul(limit,&end,10);if(!*limit || *end || n<1 || n>120) {refusal=13;goto refuse;}seconds=(unsigned)n;}
    const char *profile=getenv("FT_NATIVE_HOOKS_PROFILE");
    if(profile) {if(strcmp(profile,"lifecycle-v2")) {refusal=20;goto refuse;}lifecycle=1;}
    const char *every=getenv("FT_NATIVE_HOOKS_SAMPLE_EVERY"),*window=getenv("FT_NATIVE_HOOKS_WINDOW_FRAMES");
    if(every || window) {
        if(!lifecycle) {refusal=20;goto refuse;}
        const char *values[2]={every,window};unsigned *dest[2]={&sample_every,&window_frames};
        for(unsigned i=0;i<2;i++)if(values[i]) {
            char *end;unsigned long v=strtoul(values[i],&end,10);
            if(values[i][0]<'0' || values[i][0]>'9' || *end || v<1 || v>(i?8:10000)) {refusal=20;goto refuse;}
            *dest[i]=(unsigned)v;
        }
    }
    /* Warm libc/syscall/TLS and self-read before touching executable pages. */
    unsigned char probe[8];if(!read_meta(probe,(uintptr_t)&m,8)) {refusal=14;goto refuse;}
    deadline=now_ns()+(uint64_t)seconds*1000000000;
    size_t ps=(size_t)sysconf(_SC_PAGESIZE),length=0;
    if(ps!=4096 && ps!=16384 && ps!=65536) {refusal=15;goto refuse;}
    void *islands=make_islands(m.base,ps,&length);
    if(islands==MAP_FAILED) {refusal=16;goto refuse;}
    sigset_t all,old;sigfillset(&all);
    if(sigprocmask(SIG_BLOCK,&all,&old)) {munmap(islands,length);refusal=17;goto refuse;}
    if(!one_thread() || !guards(m.base) || !control_event(0)) {
        munmap(islands,length);if(sigprocmask(SIG_SETMASK,&old,NULL))fatal(95);refusal=18;goto refuse;
    }
    if(!install_words(m.base,(uintptr_t)islands,ps,mprotect)) {
        munmap(islands,length);if(sigprocmask(SIG_SETMASK,&old,NULL))fatal(95);refusal=19;goto refuse;
    }
    /* Once installed, no failure may return with a partially initialized observer. */
    if(!control_event(1))fatal(96);
    atomic_store(&enabled,1);
    if(pthread_create(&drainer,NULL,drain,NULL))fatal(97);
    drainer_started=1;
    if(sigprocmask(SIG_SETMASK,&old,NULL))fatal(95);
    close(fd);return;
refuse:
    (void)control_event(refusal);if(fd>=0)close(fd);close(output_fd);output_fd=-1;
}
__attribute__((destructor)) static void shutdown_observer(void) {
    if(self_pid && getpid()!=self_pid) {if(output_fd>=0)close(output_fd);return;}
    atomic_store(&enabled,0);atomic_store(&stop_drain,1);
    if(drainer_started)pthread_join(drainer,NULL);
    if(output_fd>=0)close(output_fd);
    /* Never unpatch or free executable islands while other threads may be running. Process exit owns cleanup. */
}
#endif
