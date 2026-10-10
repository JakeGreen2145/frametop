// SPDX-License-Identifier: MIT
// Included by loader.c. No module-relative PC is interpreted without its image.
#define SOURCE_CLIENT_TAG (UINT64_C(1)<<63)
#define ALL_SITE_COUNT (SITE_COUNT+CLIENT_SITE_COUNT)
static uintptr_t client_base,client_islands;
static void *client_handle; // retained dlopen reference until process exit
static int client_fd=-1;
static struct stat client_stat;
static const struct site *site_at(unsigned id) {
    return id<SITE_COUNT?sites+id:id<ALL_SITE_COUNT?client_sites+id-SITE_COUNT:NULL;
}
static int client_guards(uintptr_t base) {
    for(unsigned i=0;i<CLIENT_SITE_COUNT;++i)
        if(memcmp((void*)(base+client_sites[i].offset),client_sites[i].guard,16))return 0;
    return 1;
}
// Authenticate the actual mapped PT_LOADs against the hashed fd. Do not reuse
// compositor-only phdr assumptions: this DSO has eight headers and a VA/file
// delta in its RW segment. Compare the complete immutable RX file span as well.
static int verify_client(int fd,uintptr_t base,const Elf64_Phdr *ph,size_t count) {
    Elf64_Ehdr eh;Elf64_Phdr disk[32];struct stat st;long page=sysconf(_SC_PAGESIZE);
    if(page<=0 || (page&(page-1)) || !base || base%(uintptr_t)page ||
       !hash_fd(fd,CLIENT_SHA) || !backing_identity(fd,&st) ||
       pread(fd,&eh,sizeof eh,0)!=sizeof eh || memcmp(eh.e_ident,ELFMAG,SELFMAG) ||
       eh.e_ident[EI_CLASS]!=ELFCLASS64 || eh.e_ident[EI_DATA]!=ELFDATA2LSB ||
       eh.e_machine!=EM_AARCH64 || eh.e_type!=ET_DYN || count!=8 || eh.e_phnum!=count ||
       eh.e_phoff!=sizeof eh || eh.e_phentsize!=sizeof *disk ||
       pread(fd,disk,count*sizeof *disk,eh.e_phoff)!=(ssize_t)(count*sizeof *disk) ||
       (uintptr_t)ph!=base+eh.e_phoff || memcmp((void*)base,&eh,sizeof eh) ||
       memcmp(ph,disk,count*sizeof *disk))return 0;
    unsigned rx=0,rw=0;unsigned char bytes[16384];
    const uintptr_t mask=(uintptr_t)page-1;
    for(size_t i=0;i<count;++i) {
        const Elf64_Phdr *p=disk+i;
        if(p->p_type!=PT_LOAD)continue;
        if(!p->p_filesz || p->p_filesz>p->p_memsz || p->p_offset>(uint64_t)st.st_size ||
           p->p_filesz>(uint64_t)st.st_size-p->p_offset ||
           p->p_vaddr>UINTPTR_MAX-base-p->p_memsz-mask ||
           (p->p_offset&mask)!=(p->p_vaddr&mask))return 0;
        uintptr_t lo=p->p_vaddr&~mask,off=p->p_offset&~mask;
        size_t length=((p->p_vaddr+p->p_filesz+mask)&~mask)-lo;
        if(p->p_flags==(PF_R|PF_X)) {
            if(rx++ || lo || off || p->p_filesz!=p->p_memsz ||
               !map_range(base+lo,length,&st,off,PROT_READ|PROT_EXEC))return 0;
            for(uint64_t at=0;at<p->p_filesz;) {
                size_t n=p->p_filesz-at<sizeof bytes?p->p_filesz-at:sizeof bytes;
                if(pread(fd,bytes,n,p->p_offset+at)!=(ssize_t)n ||
                   memcmp((void*)(base+p->p_vaddr+at),bytes,n))return 0;
                at+=n;
            }
        } else if(p->p_flags==(PF_R|PF_W)) {
            if(rw++)return 0;
            // RELRO may split R and RW pages. Each page must still be the same
            // exact file offset/inode, private, readable and non-executable.
            for(size_t at=0;at<length;at+=page)
                if(!map_range(base+lo+at,page,&st,off+at,PROT_READ) &&
                   !map_range(base+lo+at,page,&st,off+at,PROT_READ|PROT_WRITE))return 0;
        } else return 0;
    }
    return rx==1 && rw==1 && client_guards(base);
}
struct client_image {uintptr_t base;const Elf64_Phdr *ph;size_t count;unsigned seen;};
static int client_image_cb(struct dl_phdr_info *p,size_t n,void *arg) {
    (void)n;struct client_image *out=arg;
    if((uintptr_t)p->dlpi_addr==out->base) {out->ph=p->dlpi_phdr;out->count=p->dlpi_phnum;++out->seen;}
    return 0;
}
static int pin_client(void) {
    char path[PATH_MAX];
    if(!realpath(CLIENT_PATH,path) || strcmp(path,CLIENT_PATH))return 0;
    int fd=open(path,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    struct stat before,after;
    if(fd<0)return 0;
    if(fstat(fd,&before) || !hash_fd(fd,CLIENT_SHA)) {close(fd);return 0;}
    // Load only the pinned canonical module, while bootstrap is single threaded.
    // Hold our reference permanently: other clients' dlclose cannot unmap hooks.
    void *handle=dlopen(path,RTLD_NOW|RTLD_LOCAL);
    struct link_map *map=NULL;struct client_image image={0};
    int current=open(path,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    int ok=handle && current>=0 && !dlinfo(handle,RTLD_DI_LINKMAP,&map) && map &&
        map->l_name && !strcmp(map->l_name,path) && !fstat(current,&after) &&
        same_file(&before,&after) && !fstat(fd,&after) && same_file(&before,&after) && one_thread();
    if(current>=0)close(current);
    if(ok) {
        image.base=map->l_addr;dl_iterate_phdr(client_image_cb,&image);
        ok=image.seen==1 && verify_client(fd,image.base,image.ph,image.count);
    }
    if(!ok) {if(handle)dlclose(handle);close(fd);return 0;}
    client_handle=handle;client_fd=fd;client_stat=before;client_base=image.base;return 1;
}
EXPORT _Bool ft_native_portal_source_modules(uint64_t *comp,uint64_t *client) {
    struct stat current;
    if(!comp || !client || getpid()!=self_pid || !atomic_load_explicit(&enabled,memory_order_acquire) ||
       !image_islands || !client_islands || !client_handle || client_fd<0 ||
       fstat(client_fd,&current) || !same_file(&client_stat,&current))return 0;
    *comp=image_base;*client=client_base;return 1;
}
