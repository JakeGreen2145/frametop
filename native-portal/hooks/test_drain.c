/* Real pthread schedules around the production callback/drainer's clock reads.
 * observer-under-test.c is generated from current observer.c by test_drain.py;
 * only unused identity includes and the architecture guard are removed. */
#include "observer-under-test.c"
#include <assert.h>
#include <semaphore.h>

static pthread_t consumer;
static sem_t go,published,entered,resume;
static int inflight,stop_case,consumer_entered;

static void wait_sem(sem_t *s) {
    int result;do {result=sem_wait(s);}while(result && errno==EINTR);
    assert(!result);
}
int __wrap_clock_gettime(clockid_t id,struct timespec *t) {
    assert(id==CLOCK_MONOTONIC);
    if(pthread_equal(pthread_self(),consumer)) {
        if(!inflight && !consumer_entered) {
            /* Old drainer already saw empty and unlocked. The producer must
             * publish before this clock read returns the expired time. */
            consumer_entered=1;assert(!sem_post(&go));wait_sem(&published);
        }
        t->tv_sec=stop_case?0:2;t->tv_nsec=1;
    } else {
        if(inflight) {
            /* observe has passed its first enabled check. Return a still-valid
             * timestamp only after drain has closed admission and finished. */
            assert(!sem_post(&entered));wait_sem(&resume);
        }
        t->tv_sec=0;t->tv_nsec=1;
    }
    return 0;
}
static void *producer(void *unused) {
    (void)unused;
    if(!inflight)wait_sem(&go);
    struct saved r={0};errno=E2BIG;observe(&r,lifecycle?EXTERNAL_RELEASE:STEREO_FINISH);
    assert(errno==E2BIG);assert(!tls.busy);
    assert(!sem_post(&published));return NULL;
}
int main(int argc,char **argv) {
    assert(argc==3 || argc==4);lifecycle=argc==4;
    inflight=strcmp(argv[1],"empty-deadline")!=0;
    stop_case=!strcmp(argv[1],"inflight-stop");
    consumer=pthread_self();self_pid=getpid();deadline=1000000000;
    atomic_store(&enabled,1);
    output_fd=open(argv[2],O_CREAT|O_EXCL|O_RDWR|O_CLOEXEC,0600);assert(output_fd>=0);
    assert(!sem_init(&go,0,0));assert(!sem_init(&published,0,0));
    assert(!sem_init(&entered,0,0));assert(!sem_init(&resume,0,0));
    pthread_t worker;assert(!pthread_create(&worker,NULL,producer,NULL));
    if(inflight)wait_sem(&entered);
    if(stop_case)atomic_store(&stop_drain,1);
    drain(NULL);
    if(inflight)assert(!sem_post(&resume));
    assert(!pthread_join(worker,NULL));
    struct stat st;assert(!fstat(output_fd,&st));
    printf("%s accepted_head=%llu drained_tail=%llu dropped=%llu file_bytes=%lld enabled=%d\n",
        argv[1],(unsigned long long)head,(unsigned long long)tail,
        (unsigned long long)atomic_load(&dropped),(long long)st.st_size,atomic_load(&enabled));
    fflush(stdout);
    uint64_t expected=inflight?0:1;
    assert((lifecycle?chead:head)==expected);assert(tail==head && ctail==chead);assert(!atomic_load(&enabled));
    assert(!atomic_load(&dropped));
    assert(st.st_size==(off_t)((expected+1)*sizeof(struct event)));
    struct event e;
    if(expected) {
        assert(pread(output_fd,&e,sizeof(e),0)==sizeof(e));
        assert(e.magic==(lifecycle?0x3254464e:MAGIC) && e.size==sizeof(e));
        assert(e.site==(lifecycle?EXTERNAL_RELEASE:STEREO_FINISH) && e.seq==1 && e.thread_seq==1);
        assert(e.tid && e.tid!=(uint64_t)syscall(SYS_gettid));assert(e.mono==1);
    }
    assert(pread(output_fd,&e,sizeof(e),(off_t)(expected*sizeof(e)))==sizeof(e));
    assert(e.magic==(lifecycle?0x3254464e:MAGIC) && e.size==sizeof(e) && e.site==UINT32_MAX && e.flags==2);
    assert(!e.x[1]);
    close(output_fd);
    assert(!sem_destroy(&go));assert(!sem_destroy(&published));
    assert(!sem_destroy(&entered));assert(!sem_destroy(&resume));
    return 0;
}
