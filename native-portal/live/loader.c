// SPDX-License-Identifier: MIT
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
#include <dlfcn.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/sysmacros.h>
#include <sys/syscall.h>
#include <openssl/evp.h>
#include "loader-pins.h"
#include "loader-build.h"
#include "../hooks/emit.h"
#include "../hooks/identity.h"
#if (!defined(__aarch64__) || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__) && !defined(FT_LOADER_TEST)
#error Production loader requires little-endian AArch64
#endif
#define STRIDE 2048
#define EXPORT __attribute__((visibility("default")))
// Strong production ABI. Only loader unit tests may provide a separate stub.
extern void ft_native_portal_event(uint64_t pc,struct saved *saved);
extern _Bool ft_native_portal_start(uint64_t base,const char *sha,const char *runtime_directory);
extern uint64_t ft_pipeline_generation(void *device);
static pid_t self_pid;
static uintptr_t image_base, image_islands;
#define BYPASS_OFFSET 1536
extern size_t ft_emit_camera_cleanup_bypass(uint32_t*,uint64_t,uint64_t,uint64_t);
EXPORT uint64_t ft_native_portal_original(uint64_t pc) {
    if(getpid()!=self_pid || !image_islands)return 0;
    for(unsigned i=0;i<SITE_COUNT;++i)
        if(sites[i].offset==pc && sites[i].emitter==6)return image_islands+i*STRIDE+BYPASS_OFFSET;
    return 0;
}
static uint64_t deadline;
static _Atomic int enabled;
#include "source_loader.inc"
EXPORT uint64_t ft_native_portal_camera_coverage(void) {
    static const uint64_t pcs[]={0x8dc1c,0x8b020,0x15e1c8,0x15df70,0x162360,0x6d6e0,0x6d6f0,0x1366b0,0x136118,0x13a100,0x15d730,0x15d198,0x15d248,0x15f9e0,0x15fba8,0x15f938,0x15faf8,0x15fcc0,0x15fbb8,0x15fb20};
    if(getpid()!=self_pid || !image_islands || !atomic_load_explicit(&enabled,memory_order_acquire))return 0;
    uint64_t mask=0;
    for(unsigned k=0;k<sizeof(pcs)/sizeof(*pcs);++k)
        for(unsigned i=0;i<SITE_COUNT;++i)if(sites[i].offset==pcs[k] &&
            (k==0 || sites[i].emitter==(k>=18?8:6)))mask|=UINT64_C(1)<<k;
    return mask;
}

EXPORT uint64_t ft_native_portal_image_base(void) { return getpid()==self_pid?image_base:0; }
static uint64_t now_ns(void) {
    struct timespec t;if(clock_gettime(CLOCK_BOOTTIME,&t))return 0;
    return (uint64_t)t.tv_sec*1000000000u+t.tv_nsec;
}
static _Noreturn void fatal(unsigned reason) {
    (void)reason;syscall(SYS_exit_group,190);__builtin_trap();
}
static void dispatch(struct saved *saved,unsigned id) {
    int e=errno;
    // Fork cannot reach any router state, TLS, allocation, control server or log.
    if(!atomic_load_explicit(&enabled,memory_order_acquire) || getpid()!=self_pid || id>=ALL_SITE_COUNT)goto out;
    // Removing routing at TTL could forward a queued owned packet as stock.
    // End this explicitly bounded compositor trial instead. Never unpatch live.
    uint64_t now=now_ns();if(!now || now>=deadline)fatal(80);
    const struct site *s=site_at(id);
    ft_native_portal_event(s->offset|(id>=SITE_COUNT?SOURCE_CLIENT_TAG:0),saved);
 out:errno=e;
}
typedef size_t (*emitter)(uint32_t*,uintptr_t,uintptr_t,uint64_t,uint32_t,unsigned,unsigned,uintptr_t);
#define DECL(N) extern size_t ft_loader_emit_##N(uint32_t*,uintptr_t,uintptr_t,uint64_t,uint32_t,unsigned,unsigned,uintptr_t)
DECL(0);DECL(1);DECL(2);DECL(3);
static const emitter emitters[]={ft_loader_emit_0,ft_loader_emit_1,ft_loader_emit_2,ft_loader_emit_3};
extern size_t ft_emit_runtime_island(uint32_t*,uint64_t,uint64_t,uint64_t,uint64_t);
#define RUNTIME_CAPACITY 1024
_Static_assert(RUNTIME_CAPACITY+40<=STRIDE,"runtime island plus callback thunk must fit");
EXPORT size_t ft_loader_emit_site(uint32_t *out,uintptr_t at,uintptr_t base,unsigned id,uintptr_t callback) {
    if(!out || !callback || id>=ALL_SITE_COUNT || ((at|base)&3) || at>UINTPTR_MAX-STRIDE)return 0;
    const struct site *s=site_at(id);
    if(s->emitter>=5 && s->emitter<=8) {
        // Keep the runtime emitter's (relative_pc,Saved*) ABI untouched. A
        // bounded per-island tail thunk enters the SAME errno/fork/TTL dispatcher
        // as every donor emitter, using its immutable deduplicated site ID.
        size_t bytes=ft_emit_runtime_island(out,at,base,s->offset,at+RUNTIME_CAPACITY);
        if(!bytes || bytes>RUNTIME_CAPACITY || (bytes&3))return 0;
        memset((char*)out+bytes,0,RUNTIME_CAPACITY-bytes);
        size_t n=RUNTIME_CAPACITY/4;
        out[n++]=0xaa0103e0; // mov x0,x1 (Saved*)
        imm64(out,&n,1,id);
        imm64(out,&n,16,callback);
        out[n++]=0xd61f0200; // br x16; preserve the emitter's callback return LR
        return n*4;
    }
    unsigned group=s->emitter==4?1:s->emitter;
    if(group>=4)return 0;
    unsigned local=group?s->index:0;
    size_t n=emitters[group](out,at,base,s->offset,s->word,s->kind,local,callback);
    if(!n || n>STRIDE)return 0;
    // Generators pass a group-local ID. The loader passes one deduplicated ID.
    // Fail if the proven callback-argument sequence changes in a donor emitter.
    unsigned found=0;
    for(size_t k=0;k+1<n/4;k++)if(out[k]==0x910003e0 && out[k+1]==(0xd2800001u|(local<<5))) {
        out[k+1]=0xd2800001u|(id<<5);++found;
    }
    return found==1?n:0;
}
static int guards(uintptr_t base) {
    for(unsigned i=0;i<SITE_COUNT;i++)if(memcmp((void*)(base+sites[i].offset),sites[i].guard,16))return 0;
    return 1;
}
// All branch range and guard checks precede the first writable text page.
static int install_modules(uintptr_t base,uintptr_t islands,uintptr_t cb,uintptr_t ci,size_t ps,int (*protect)(void*,size_t,int)) {
    uintptr_t pages[ALL_SITE_COUNT],addresses[ALL_SITE_COUNT];uint32_t words[ALL_SITE_COUNT];unsigned count=0;
    const unsigned total=cb?ALL_SITE_COUNT:SITE_COUNT;
    if(!ps || (ps&(ps-1)) || !guards(base) || (cb && (!ci || !client_guards(cb))))return 0;
    for(unsigned i=0;i<total;i++) {
        const struct site *s=site_at(i);const unsigned local=i<SITE_COUNT?i:i-SITE_COUNT;
        addresses[i]=(i<SITE_COUNT?base:cb)+s->offset;
        uintptr_t page=addresses[i]&~(ps-1);unsigned j;
        if(!branch_word(addresses[i],(i<SITE_COUNT?islands:ci)+local*STRIDE,&words[i]))return 0;
        for(j=0;j<count && pages[j]!=page;j++);
        if(j==count)pages[count++]=page;
    }
    unsigned rw=0;
    for(;rw<count;rw++)if(protect((void*)pages[rw],ps,PROT_READ|PROT_WRITE))break;
    if(rw!=count) {
        for(unsigned j=0;j<rw;j++)if(protect((void*)pages[j],ps,PROT_READ|PROT_EXEC))fatal(91);
        return 0;
    }
    for(unsigned i=0;i<total;i++)__atomic_store_n((uint32_t*)addresses[i],words[i],__ATOMIC_RELAXED);
    for(unsigned i=0;i<count;i++)__builtin___clear_cache((char*)pages[i],(char*)pages[i]+ps);
    for(unsigned i=0;i<count;i++)if(protect((void*)pages[i],ps,PROT_READ|PROT_EXEC))fatal(92);
    for(unsigned i=0;i<total;i++)if(*(uint32_t*)addresses[i]!=words[i])fatal(93);
    for(unsigned i=0;i<count;i++)if(!map_range(pages[i],ps,NULL,0,PROT_READ|PROT_EXEC))fatal(94);
    return 1;
}
static int install_words(uintptr_t base,uintptr_t islands,size_t ps,int (*protect)(void*,size_t,int)) {
    return install_modules(base,islands,0,0,ps,protect);
}
static void *make_module_islands(uintptr_t base,size_t ps,size_t *length,unsigned first,unsigned count) {
    *length=(count*STRIDE+ps-1)&~(ps-1);
    for(uintptr_t delta=0x800000;delta<0x8000000;delta+=0x100000) {
        uintptr_t at=(base+delta+ps-1)&~(ps-1);
        void *p=mmap((void*)at,*length,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
        if(p==MAP_FAILED)continue;
        int ok=(uintptr_t)p==at;
        for(unsigned i=0;ok && i<count;i++) {
            const struct site *s=site_at(first+i);
            size_t n=ft_loader_emit_site((uint32_t*)((char*)p+i*STRIDE),at+i*STRIDE,base,first+i,(uintptr_t)dispatch);
            uint32_t word;ok=n && n<=BYPASS_OFFSET && branch_word(base+s->offset,at+i*STRIDE,&word);
            if(ok && s->emitter==6)
                ok=ft_emit_camera_cleanup_bypass((uint32_t*)((char*)p+i*STRIDE+BYPASS_OFFSET),
                    at+i*STRIDE+BYPASS_OFFSET,base,s->offset)==8;
        }
        if(ok) {__builtin___clear_cache(p,(char*)p+*length);ok=!mprotect(p,*length,PROT_READ|PROT_EXEC);}
        if(ok && map_range(at,*length,NULL,0,PROT_READ|PROT_EXEC))return p;
        munmap(p,*length);
    }
    return MAP_FAILED;
}
static void *make_islands(uintptr_t base,size_t ps,size_t *length) {
    return make_module_islands(base,ps,length,0,SITE_COUNT);
}
static int immutable_file(const char *path,const char *sha) {
    struct stat s;int fd=open(path,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    int ok=fd>=0 && !fstat(fd,&s) && S_ISREG(s.st_mode) && s.st_uid==geteuid() &&
        (s.st_mode&0777)==0400 && s.st_nlink==1 && hash_fd(fd,sha);
    if(fd>=0)close(fd);return ok;
}
static int bundle_ok(void) {
    const char *pin=getenv("FT_NATIVE_PORTAL_BUNDLE_SHA256");
    if(!pin || strlen(pin)!=64)return 0;
    Dl_info owner={0},layer={0};char dir[PATH_MAX],path[PATH_MAX],canonical[PATH_MAX];
    if(!dladdr((void*)ft_native_portal_image_base,&owner) || !owner.dli_fname ||
       !realpath(owner.dli_fname,canonical) || strlen(canonical)>=sizeof dir)return 0;
    strcpy(dir,canonical);char *slash=strrchr(dir,'/');if(!slash)return 0;*slash=0;
    struct stat s;if(lstat(dir,&s) || !S_ISDIR(s.st_mode) || s.st_uid!=geteuid() || (s.st_mode&0777)!=0500)return 0;
    if(snprintf(path,sizeof path,"%s/bundle.pin",dir)>=(int)sizeof path || !immutable_file(path,pin))return 0;
    int fd=open(path,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)return 0;
    char text[512]={0};ssize_t n=read(fd,text,sizeof text-1);close(fd);
    char owner_sha[65]={0},expected[512];
    const size_t prefix=strlen("schema=1\nbuild=" FT_BUILD_ID "\nowner=");
    if(n!=(ssize_t)(prefix+64+strlen("\npipeline=" FT_PIPELINE_SHA "\n")) ||
       memcmp(text,"schema=1\nbuild=" FT_BUILD_ID "\nowner=",prefix))return 0;
    memcpy(owner_sha,text+prefix,64);
    snprintf(expected,sizeof expected,"schema=1\nbuild=%s\nowner=%s\npipeline=%s\n",FT_BUILD_ID,owner_sha,FT_PIPELINE_SHA);
    if(strcmp(text,expected) || !immutable_file(canonical,owner_sha))return 0;
    if(!dladdr((void*)ft_pipeline_generation,&layer) || !layer.dli_fname ||
       !realpath(layer.dli_fname,canonical))return 0;
    if(snprintf(path,sizeof path,"%s/libft_portal_pipeline.so.1",dir)>=(int)sizeof path ||
       strcmp(path,canonical) || !immutable_file(path,FT_PIPELINE_SHA))return 0;
    return 1;
}
#ifdef FT_LOADER_TEST
EXPORT int ft_loader_test_bundle_ok(void) {return bundle_ok();}
#endif
static int options(unsigned *seconds) {
    const char *opt=getenv("FT_NATIVE_PORTAL_ENABLE");
    if(!opt || strcmp(opt,FT_BUILD_ID))return 0;
    const char *limit=getenv("FT_NATIVE_PORTAL_SECONDS");*seconds=60;
    if(limit) {char *end;unsigned long n=strtoul(limit,&end,10);
        if(*limit<'0' || *limit>'9' || *end || n<1 || n>120)return 0;*seconds=(unsigned)n;}
    if(getenv("FT_NATIVE_HOOKS_ENABLE") || getenv("FT_AURORA_GATE_ENABLE") ||
       getenv("FT_NATIVE_CLIP_DIR") || getenv("FT_CAMERA_BLEND_MODE"))return 0;
    return 1;
}
#ifndef FT_LOADER_NO_CONSTRUCTOR
__attribute__((constructor)) static void launch(void) {
    unsigned seconds;if(!options(&seconds))return;
    // The exact executable gate precedes file inspection, logs or router calls.
    char path[PATH_MAX];ssize_t n=readlink("/proc/self/exe",path,sizeof(path)-1);
    if(n<0 || (size_t)n>=sizeof(path)-1)return;
    path[n]=0;if(strcmp(path,EXE))return;
    int fd=exe_fd();if(fd<0)return;
    struct main_image m={0};dl_iterate_phdr(main_image_cb,&m);
    if(m.seen!=1 || !bundle_ok() || !verify_load(fd,m.base,m.ph,m.count) || !one_thread() || !pin_client()) {close(fd);return;}
    uint64_t now=now_ns();size_t ps=(size_t)sysconf(_SC_PAGESIZE),length=0;
    if(!now || (ps!=4096 && ps!=16384 && ps!=65536)) {close(fd);return;}
    self_pid=getpid();image_base=m.base;deadline=now+(uint64_t)seconds*1000000000;
    void *islands=make_islands(m.base,ps,&length);
    if(islands==MAP_FAILED) {close(fd);return;}
    size_t client_length=0;
    void *ci=make_module_islands(client_base,ps,&client_length,SITE_COUNT,CLIENT_SITE_COUNT);
    if(ci==MAP_FAILED) {munmap(islands,length);close(fd);return;}
    sigset_t all,old;sigfillset(&all);
    if(sigprocmask(SIG_BLOCK,&all,&old)) {munmap(ci,client_length);munmap(islands,length);close(fd);return;}
    if(!one_thread() || !guards(m.base) || !install_modules(m.base,(uintptr_t)islands,client_base,(uintptr_t)ci,ps,mprotect)) {
        munmap(ci,client_length);munmap(islands,length);close(fd);if(sigprocmask(SIG_SETMASK,&old,NULL))fatal(95);return;
    }
    image_islands=(uintptr_t)islands;client_islands=(uintptr_t)ci;
    atomic_store_explicit(&enabled,1,memory_order_release);
    // No native thread exists yet. Server creation is deferred until the complete
    // hook group is installed; failure from here cannot return into native code.
    if(!ft_native_portal_start(m.base,EXE_SHA,getenv("XDG_RUNTIME_DIR")))fatal(96);
    if(sigprocmask(SIG_SETMASK,&old,NULL))fatal(95);
    close(fd);
    // No constructors in the router should start it before this first event.
    // No unpatch/destructor: process exit owns island and native-state lifetime.
}
#endif
