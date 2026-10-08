#define HOOK_TEST
#include "observer.c"
#include <assert.h>
#include <sys/wait.h>
static unsigned protect_calls,fail_call;
static int protect_test(void *p,size_t n,int prot) {
    assert(prot!=(PROT_READ|PROT_WRITE|PROT_EXEC));
    if(++protect_calls==fail_call) {errno=EACCES;return -1;}
    return mprotect(p,n,prot);
}
static void test_transaction(void) {
    size_t len=0x800000,ps=(size_t)sysconf(_SC_PAGESIZE);
    unsigned char *p=mmap(NULL,len,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(p!=MAP_FAILED);
    for(unsigned i=0;i<SITE_COUNT;i++)memcpy(p+sites[i].offset,sites[i].guard,16);
    p[sites[0].offset]^=1;
    assert(!install_words((uintptr_t)p,(uintptr_t)p+0x400000,ps,protect_test));
    assert(protect_calls==0);p[sites[0].offset]^=1;
    assert(!mprotect(p,len,PROT_READ|PROT_EXEC));
    fail_call=2;protect_calls=0;
    assert(!install_words((uintptr_t)p,(uintptr_t)p+0x400000,ps,protect_test));
    assert(guards((uintptr_t)p));assert(map_range((uintptr_t)p,len,NULL,0,PROT_READ|PROT_EXEC));
    unsigned page_count=0;
    for(unsigned i=0;i<SITE_COUNT;i++) {
        unsigned j;for(j=0;j<i;j++)if((sites[i].offset&~(ps-1))==(sites[j].offset&~(ps-1)))break;
        if(j==i)++page_count;
    }
    pid_t child=fork();assert(child>=0);
    if(!child) {output_fd=-1;fail_call=page_count+1;protect_calls=0;
        (void)install_words((uintptr_t)p,(uintptr_t)p+0x400000,ps,protect_test);_exit(1);}
    int status;assert(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==190);
    assert(guards((uintptr_t)p));
    fail_call=0;protect_calls=0;
    assert(install_words((uintptr_t)p,(uintptr_t)p+0x400000,ps,protect_test));
    for(unsigned i=0;i<SITE_COUNT;i++) {
        uint32_t word;assert(branch_word((uintptr_t)p+sites[i].offset,(uintptr_t)p+0x400000+i*ISLAND_STRIDE,&word));
        assert(*(uint32_t*)(p+sites[i].offset)==word);
        for(unsigned j=4;j<16;j++) {
            int patched=0;
            for(unsigned k=0;k<SITE_COUNT;k++)if(sites[i].offset+j>=sites[k].offset && sites[i].offset+j<sites[k].offset+4)patched=1;
            if(!patched)assert(p[sites[i].offset+j]==sites[i].guard[j]);
        }
    }
    munmap(p,len);puts("PASS transaction-and-rollback");
}
static void test_capture(void) {
    self_pid=getpid();deadline=now_ns()+1000000000u;atomic_store(&enabled,1);
    struct saved r={0};unsigned char k[0x400],stack[0x400],packet[0x160];
    pid_t child=fork();assert(child>=0);
    if(!child) {observe(&r,OUTER_ENTER);_exit(head==0 && !atomic_load(&enabled)?0:1);}
    int status;assert(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    memset(k,0x31,sizeof(k));memset(stack,0x62,sizeof(stack));memset(packet,0x93,sizeof(packet));
    r.sp=(uintptr_t)stack;r.x[0]=123;errno=E2BIG;observe(&r,OUTER_ENTER);assert(errno==E2BIG);
    assert(head==1 && ring[0].frame==1 && ring[0].depth==1);
    r.x[19]=0;r.x[27]=(uintptr_t)k;r.x[1]=(uintptr_t)packet;
    struct saved original=r;observe(&r,CAMERA_ENQUEUE);assert(!memcmp(&r,&original,sizeof(r)));
    struct event *e=&ring[1];assert(e->mask==1 && e->eye==0 && e->used==976 && !(e->flags&4));
    assert(!memcmp(e->data+16,k+0xc0,528));assert(!memcmp(e->data+560,stack+0x170,48));
    assert(!memcmp(e->data+624,packet,0x160));
    r.x[19]=1;observe(&r,CAMERA_ENQUEUE);assert(ring[2].mask==3);
    int fixture=open("build/metadata-fixture.bin",O_CREAT|O_TRUNC|O_WRONLY|O_CLOEXEC,0600);assert(fixture>=0);
    assert(write(fixture,&ring[1],2*sizeof(struct event))==2*sizeof(struct event));assert(!close(fixture));
    r.x[27]=1;observe(&r,CAMERA_ENQUEUE);assert(ring[3].flags&4);assert(atomic_load(&read_failures)==1);
    observe(&r,OUTER_RETURN);assert(!tls.depth);
    atomic_flag_test_and_set(&ring_lock);observe(&r,STEREO_FINISH);assert(atomic_load(&dropped)==1);atomic_flag_clear(&ring_lock);
    for(unsigned i=0;i<RING_COUNT;i++)observe(&r,STEREO_FINISH);
    assert(head==RING_COUNT && atomic_load(&dropped)>1);
    tls.busy=1;observe(&r,OUTER_ENTER);assert(atomic_load(&reentrant)==1);tls.busy=0;
    puts("PASS ring-and-metadata");
}
static void test_native_bridge(void) {
    size_t length;size_t ps=(size_t)sysconf(_SC_PAGESIZE);
    void *base=mmap(NULL,0x400000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(base!=MAP_FAILED);
    void *islands=make_islands((uintptr_t)base,ps,&length);assert(islands!=MAP_FAILED);
    uint32_t word;uintptr_t pc=(uintptr_t)base+sites[OUTER_ENTER].offset;
    assert(branch_word(pc,(uintptr_t)islands,&word));*(uint32_t*)pc=word;
    *(uint32_t*)(pc+4)=0xd65f03c0; /* Synthetic native continuation returns, never executes Valve code. */
    __builtin___clear_cache(base,(char*)base+0x400000);assert(!mprotect(base,0x400000,PROT_READ|PROT_EXEC));
    head=tail=0;deadline=now_ns()+1000000000u;atomic_store(&enabled,1);tls.depth=0;
    uint64_t (*fn)(uint64_t)=(void*)pc;
    errno=EBUSY;assert(fn(0x123456789)==0x123456789 && errno==EBUSY);
    assert(head==1 && ring[0].site==OUTER_ENTER && ring[0].x[0]==0x123456789);
    munmap(base,0x400000);munmap(islands,length);puts("PASS native-bridge");
}
static void test_file_map(void) {
    int fd=open(EXE,O_RDONLY|O_CLOEXEC);assert(fd>=0 && hash_fd(fd,EXE_SHA));
    struct stat st;assert(!fstat(fd,&st));
    void *p=mmap(NULL,st.st_size,PROT_READ|PROT_EXEC,MAP_PRIVATE,fd,0);assert(p!=MAP_FAILED);
    const Elf64_Ehdr *eh=p;const Elf64_Phdr *ph=(void*)((char*)p+eh->e_phoff);
    assert(verify_load(fd,(uintptr_t)p,ph,eh->e_phnum));
    assert(!verify_load(fd,(uintptr_t)p,ph,eh->e_phnum-1));
    assert(!verify_load(fd,(uintptr_t)p+4096,ph,eh->e_phnum));
    munmap(p,st.st_size);close(fd);puts("PASS pinned-file-map");
}
int main(void) {
    self_pid=getpid();
    char dir[]="build/test-log-XXXXXX";assert(mkdtemp(dir));assert(!chmod(dir,0700));
    char absolute[PATH_MAX],file[PATH_MAX];assert(realpath(dir,absolute));
    assert(snprintf(file,sizeof(file),"%s/events.bin",absolute)>0);
    output_fd=fresh_log(file);assert(output_fd>=0);assert(fresh_log(file)<0);assert(fresh_log("relative")<0);
    struct stat st;assert(!fstat(output_fd,&st) && (st.st_mode&0777)==0600);
    puts("PASS fresh-private-file");
    assert(exe_fd()<0);int fd=open("/proc/self/exe",O_RDONLY);assert(fd>=0 && !hash_fd(fd,EXE_SHA));close(fd);
    assert(one_thread());puts("PASS identity-refusal");
    test_file_map();test_transaction();test_capture();test_native_bridge();
    deadline=now_ns()+20000000;atomic_store(&enabled,1);
    assert(!pthread_create(&drainer,NULL,drain,NULL));assert(!pthread_join(drainer,NULL));
    assert(!atomic_load(&enabled));assert(tail==head);close(output_fd);output_fd=-1;
    assert(!stat(file,&st) && st.st_size==(off_t)((head+1)*sizeof(struct event)));
    puts("PASS deadline-drain");return 0;
}
