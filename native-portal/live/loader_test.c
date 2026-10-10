// SPDX-License-Identifier: MIT
// Explicit test-only event/pipeline boundary. Never linked by build.py.
#define FT_LOADER_TEST 1
#define FT_LOADER_NO_CONSTRUCTOR 1
#include "loader.c"
#include <assert.h>
#include <sys/wait.h>
static unsigned calls;
void ft_native_portal_event(uint64_t pc,struct saved *r) {assert(pc==sites[0].offset);assert(r);++calls;errno=123;}
_Bool ft_native_portal_start(uint64_t b,const char *s,const char *d) {(void)b;(void)s;(void)d;return 0;}
uint64_t ft_pipeline_generation(void *d) {(void)d;return 0;}
static unsigned protections,fail_at,fail_again;
static int protect(void *p,size_t n,int mode) {
    assert(mode==(PROT_READ|PROT_WRITE) || mode==(PROT_READ|PROT_EXEC));
    ++protections;if(protections==fail_at || protections==fail_again) {errno=EACCES;return -1;}
    return mprotect(p,n,mode);
}
static uintptr_t fixture(void) {
    void *p=mmap(NULL,0x800000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(p!=MAP_FAILED);
    for(unsigned i=0;i<SITE_COUNT;i++)memcpy((char*)p+sites[i].offset,sites[i].guard,16);
    assert(guards((uintptr_t)p));return (uintptr_t)p;
}
static unsigned pages(void) {
    unsigned n=0;uintptr_t old=UINTPTR_MAX;
    for(unsigned i=0;i<SITE_COUNT;i++)if((sites[i].offset&~4095ULL)!=old) {old=sites[i].offset&~4095ULL;++n;}
    return n;
}
static void expect_fatal(unsigned first,unsigned second) {
    pid_t p=fork();assert(p>=0);
    if(!p) {
        uintptr_t b=fixture();assert(!mprotect((void*)b,0x800000,PROT_READ|PROT_EXEC));
        protections=0;fail_at=first;fail_again=second;
        (void)install_words(b,b+0x600000,4096,protect);_exit(99);
    }
    int s;assert(waitpid(p,&s,0)==p);assert(WIFEXITED(s)&&WEXITSTATUS(s)==190);
}
static void test_file_map(const char *path) {
    int fd=open(path,O_RDONLY|O_CLOEXEC);assert(fd>=0 && hash_fd(fd,EXE_SHA));
    struct stat st;assert(!fstat(fd,&st));
    void *p=mmap(NULL,st.st_size,PROT_READ|PROT_EXEC,MAP_PRIVATE,fd,0);assert(p!=MAP_FAILED);
    const Elf64_Ehdr *eh=p;const Elf64_Phdr *ph=(void*)((char*)p+eh->e_phoff);
    assert(verify_load(fd,(uintptr_t)p,ph,eh->e_phnum));
    assert(!verify_load(fd,(uintptr_t)p,ph,eh->e_phnum-1));
    assert(!verify_load(fd,(uintptr_t)p+4096,ph,eh->e_phnum));
    munmap(p,st.st_size);close(fd);
    fd=open("/proc/self/exe",O_RDONLY|O_CLOEXEC);assert(fd>=0 && !hash_fd(fd,EXE_SHA));close(fd);
    assert(!hash_fd(-1,EXE_SHA));puts("PASS exact ELF file hash and mapped identity, missing/mismatch refusal");
}
#if defined(__aarch64__)
static void native_callback(struct saved *r,unsigned id) {
    assert(sites[id].offset==0x147a48 && r->x[0]==0x123456789);++calls;
}
static void native_execution(void) {
    uintptr_t b=fixture();unsigned id=0;
    while(id<SITE_COUNT && sites[id].offset!=0x147a48)++id;assert(id<SITE_COUNT);
    uintptr_t pc=b+sites[id].offset,at=b+0x600000;
    assert(ft_loader_emit_site((uint32_t*)at,at,b,id,(uintptr_t)native_callback));
    uint32_t word;assert(branch_word(pc,at,&word));*(uint32_t*)pc=word;*(uint32_t*)(pc+4)=0xd65f03c0;
    __builtin___clear_cache((char*)b,(char*)b+0x800000);assert(!mprotect((void*)b,0x800000,PROT_READ|PROT_EXEC));
    unsigned before=calls;uint64_t (*fn)(uint64_t)=(void*)pc;
    assert(fn(0x123456789)==0x123456789 && calls==before+1);munmap((void*)b,0x800000);
    puts("PASS native ARM synthetic island execution, no compositor launched");
}
#endif
int main(int argc,char **argv) {
    if(argc==2)test_file_map(argv[1]);
#if defined(__aarch64__)
    native_execution();calls=0;
#endif
    unsigned seconds;
    unsetenv("FT_NATIVE_PORTAL_ENABLE");assert(!options(&seconds));
    setenv("FT_NATIVE_PORTAL_ENABLE","wrong",1);assert(!options(&seconds));
    setenv("FT_NATIVE_PORTAL_ENABLE",FT_BUILD_ID,1);assert(options(&seconds)&&seconds==60);
    const char *bad[]={"0","121","","-1","+1","1x"," 1"};
    for(unsigned i=0;i<sizeof bad/sizeof bad[0];i++) {setenv("FT_NATIVE_PORTAL_SECONDS",bad[i],1);assert(!options(&seconds));}
    unsetenv("FT_NATIVE_PORTAL_SECONDS");
    assert(!bundle_ok());setenv("FT_NATIVE_PORTAL_BUNDLE_SHA256","wrong",1);assert(!bundle_ok());
    assert(exe_fd()==-1); // inert executable is never the pinned compositor
    struct saved r={0};errno=42;dispatch(&r,0);assert(!calls&&errno==42);
    assert(!ft_native_portal_original(0x6d6e0) && !ft_native_portal_camera_coverage());
    self_pid=getpid();deadline=now_ns()+1000000000;atomic_store(&enabled,1);
    dispatch(&r,0);assert(calls==1&&errno==42);
    pid_t child=fork();assert(child>=0);
    if(!child) {dispatch(&r,0);assert(calls==1);assert(ft_native_portal_image_base()==0);_exit(0);}
    int status;assert(waitpid(child,&status,0)==child&&status==0);
    atomic_store(&enabled,0);
    uintptr_t b=fixture();
    ((unsigned char*)(b+sites[SITE_COUNT-1].offset))[15]^=1;
    assert(!install_words(b,b+0x600000,4096,protect)&&protections==0);
    munmap((void*)b,0x800000);
    b=fixture();assert(!install_words(b,b+0x10000000,4096,protect)&&protections==0);
    munmap((void*)b,0x800000);
    // Every possible precommit page failure must restore all prior RX pages.
    for(unsigned failed=1;failed<=pages();failed++) {
        b=fixture();assert(!mprotect((void*)b,0x800000,PROT_READ|PROT_EXEC));
        protections=0;fail_at=failed;
        assert(!install_words(b,b+0x600000,4096,protect));assert(guards(b));
        assert(map_range(b,0x800000,NULL,0,PROT_READ|PROT_EXEC));munmap((void*)b,0x800000);
    }
    expect_fatal(2,3); // rollback protection failure
    expect_fatal(pages()+1,0); // postpatch RX failure must exit, not return
    b=fixture();assert(!mprotect((void*)b,0x800000,PROT_READ|PROT_EXEC));protections=fail_at=fail_again=0;
    assert(install_words(b,b+0x600000,4096,protect));assert(protections==pages()*2);
    for(unsigned i=0;i<SITE_COUNT;i++) {uint32_t word;assert(branch_word(b+sites[i].offset,b+0x600000+i*STRIDE,&word));assert(*(uint32_t*)(b+sites[i].offset)==word);}
    munmap((void*)b,0x800000);
    b=fixture();size_t len=0;void *islands=make_islands(b,4096,&len);assert(islands!=MAP_FAILED);
    assert(map_range((uintptr_t)islands,len,NULL,0,PROT_READ|PROT_EXEC));
    image_islands=(uintptr_t)islands;atomic_store(&enabled,1);
    assert(ft_native_portal_camera_coverage()==0x3ffff);
    for(unsigned i=0;i<SITE_COUNT;++i)if(sites[i].emitter==6) {
        assert(ft_native_portal_original(sites[i].offset)==image_islands+i*STRIDE+BYPASS_OFFSET);
        assert(*(uint32_t*)ft_native_portal_original(sites[i].offset)==sites[i].word);
    }
    image_islands=0;atomic_store(&enabled,0);munmap(islands,len);munmap((void*)b,0x800000);
    child=fork();assert(child>=0);if(!child) {self_pid=getpid();atomic_store(&enabled,1);deadline=1;dispatch(&r,0);_exit(99);}
    assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==190);
    printf("PASS loader gates/fork/TTL; %u sites, %u text pages; all precommit failures and fatal postpatch/rollback failures\n",SITE_COUNT,pages());
    return 0;
}
