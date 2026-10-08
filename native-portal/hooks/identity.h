/* Launch identity helpers adapted from the reviewed source-over/gate observer. */
static int same_file(const struct stat *a, const struct stat *b) {
    return a->st_dev==b->st_dev && a->st_ino==b->st_ino &&
        a->st_size==b->st_size && a->st_mtim.tv_sec==b->st_mtim.tv_sec &&
        a->st_mtim.tv_nsec==b->st_mtim.tv_nsec && a->st_ctim.tv_sec==b->st_ctim.tv_sec &&
        a->st_ctim.tv_nsec==b->st_ctim.tv_nsec;
}
static int hash_fd(int fd, const char *sha) {
    struct stat a,b;
    if (!sha || strlen(sha)!=64 || fstat(fd,&a) || !S_ISREG(a.st_mode) || a.st_size<0) return 0;
    EVP_MD_CTX *ctx=EVP_MD_CTX_new();
    if (!ctx) return 0;
    int ok=EVP_DigestInit_ex(ctx,EVP_sha256(),NULL)==1;
    unsigned char buf[16384],digest[32]; unsigned int n=0; off_t at=0;
    while (ok && at<a.st_size) {
        ssize_t got=pread(fd,buf,sizeof(buf),at);
        if (got<0 && errno==EINTR) continue;
        if (got<=0) {ok=0;break;}
        ok=EVP_DigestUpdate(ctx,buf,(size_t)got)==1;at+=got;
    }
    if (ok) ok=EVP_DigestFinal_ex(ctx,digest,&n)==1 && n==32;
    EVP_MD_CTX_free(ctx);
    char hex[65];
    if (ok) {
        for (unsigned i=0;i<32;i++) snprintf(hex+2*i,3,"%02x",digest[i]);
        ok=!strcmp(hex,sha) && !fstat(fd,&b) && same_file(&a,&b);
    }
    return ok;
}

static int map_range(uintptr_t at,size_t len,const struct stat *st,off_t offset,int prot) {
    if (!len || at>UINTPTR_MAX-len || offset<0) return 0;
    FILE *f=fopen("/proc/self/maps","re");if (!f) return 0;
    uintptr_t end=at+len;char *line=NULL;size_t cap=0;
    while (at<end && getline(&line,&cap,f)>0) {
        unsigned long lo,hi,off,ino;unsigned maj,min;char perm[5];
        if (sscanf(line,"%lx-%lx %4s %lx %x:%x %lu",&lo,&hi,perm,&off,&maj,&min,&ino)!=7) continue;
        if (at<lo || at>=hi) continue;
        int p=(perm[0]=='r'?PROT_READ:0)|(perm[1]=='w'?PROT_WRITE:0)|(perm[2]=='x'?PROT_EXEC:0);
        if (p!=prot || perm[3]!='p' || (st && (ino!=st->st_ino || makedev(maj,min)!=st->st_dev ||
            off+(at-lo)!=(unsigned long)offset))) break;
        size_t used=(hi<end?hi:end)-at;at+=used;offset+=(off_t)used;
    }
    free(line);fclose(f);return at==end;
}
/* SteamOS /opt overlay stat device differs from its mapped backing device.
 * Derive kernel mapping identity from an owned R-only alias of the pinned fd,
 * not from a path guess or a relaxed device comparison. No map_files privilege. */
static int backing_identity(int fd,struct stat *st) {
    long ps=sysconf(_SC_PAGESIZE);if (ps<=0 || fstat(fd,st)) return 0;
    void *alias=mmap(NULL,(size_t)ps,PROT_READ,MAP_PRIVATE,fd,0);
    if (alias==MAP_FAILED) return 0;
    FILE *f=fopen("/proc/self/maps","re");char *line=NULL;size_t cap=0;int ok=0;
    if (f) {
        while (getline(&line,&cap,f)>0) {
            unsigned long lo,hi,off,ino;unsigned maj,min;char perm[5];
            if (sscanf(line,"%lx-%lx %4s %lx %x:%x %lu",&lo,&hi,perm,&off,&maj,&min,&ino)!=7) continue;
            if (lo==(uintptr_t)alias && hi>=lo+(uintptr_t)ps && !off && ino && !strcmp(perm,"r--p")) {
                st->st_dev=makedev(maj,min);st->st_ino=ino;ok=1;break;
            }
        }
        fclose(f);
    }
    free(line);if (munmap(alias,(size_t)ps)) return 0;
    return ok;
}

static int verify_load(int fd,uintptr_t base,const Elf64_Phdr *ph,size_t count) {
    Elf64_Ehdr eh;Elf64_Phdr disk[32];struct stat st;long ps=sysconf(_SC_PAGESIZE);
    if (ps<=0 || (ps&(ps-1)) || base%(uintptr_t)ps || !backing_identity(fd,&st) ||
        pread(fd,&eh,sizeof(eh),0)!=sizeof(eh) || memcmp(eh.e_ident,ELFMAG,SELFMAG) ||
        eh.e_ident[EI_CLASS]!=ELFCLASS64 || eh.e_ident[EI_DATA]!=ELFDATA2LSB ||
        eh.e_machine!=EM_AARCH64 || eh.e_type!=ET_DYN || eh.e_phnum!=count ||
        !count || count>32 || eh.e_phentsize!=sizeof(Elf64_Phdr) || eh.e_phoff!=sizeof(eh) ||
        pread(fd,disk,count*sizeof(*disk),(off_t)eh.e_phoff)!=(ssize_t)(count*sizeof(*disk))) return 0;
    /* The pinned image has a single RX PT_LOAD starting at file/VA zero. */
    if (count!=10 || disk[2].p_type!=PT_LOAD || disk[2].p_offset || disk[2].p_vaddr || disk[2].p_flags!=(PF_R|PF_X)) return 0;
    int found=0;
    for (size_t i=0;i<count;i++) {
        const Elf64_Phdr *p=&disk[i];
        if (p->p_type!=PT_LOAD || !(p->p_flags&PF_X)) continue;
        if (p->p_flags!=(PF_R|PF_X) || p->p_filesz!=p->p_memsz || p->p_vaddr>(uintptr_t)st.st_size ||
            p->p_filesz>(uintptr_t)st.st_size-p->p_vaddr || p->p_offset!=p->p_vaddr ||
            base>UINTPTR_MAX-p->p_vaddr-p->p_filesz-(uintptr_t)ps) return 0;
        uintptr_t start=p->p_vaddr & ~((uintptr_t)ps-1);
        size_t length=((p->p_vaddr+p->p_memsz+(uintptr_t)ps-1)&~((uintptr_t)ps-1))-start;
        if (!map_range(base+start,length,&st,(off_t)start,PROT_READ|PROT_EXEC)) return 0;
        if (p->p_vaddr==0 && p->p_filesz>0x29f520) found++;
    }
    if (found!=1 || (uintptr_t)ph!=base+eh.e_phoff || memcmp((void*)base,&eh,sizeof(eh)) ||
        memcmp(ph,disk,count*sizeof(*disk))) return 0;
    for (unsigned i=0;i<SITE_COUNT;i++)
        if (memcmp((void*)(base+sites[i].offset),sites[i].guard,16)) return 0;
    return 1;
}

struct main_image { uintptr_t base; const Elf64_Phdr *ph; size_t count; int seen; };
static int main_image_cb(struct dl_phdr_info *p,size_t size,void *data) {
    (void)size;
    if (!p->dlpi_name || !*p->dlpi_name) {
        struct main_image *m=data;m->base=p->dlpi_addr;m->ph=p->dlpi_phdr;m->count=p->dlpi_phnum;m->seen++;
    }
    return 0;
}
static int one_thread(void) {
    DIR *d=opendir("/proc/self/task");if (!d) return 0;
    struct dirent *e;int n=0;
    while ((e=readdir(d))) if (e->d_name[0]!='.') n++;
    closedir(d);return n==1;
}
static int exe_fd(void) {
    char path[PATH_MAX],canon[PATH_MAX];
    ssize_t n=readlink("/proc/self/exe",path,sizeof(path)-1);
    if (n<0 || (size_t)n>=sizeof(path)-1) return -1;
    path[n]=0;
    if (strcmp(path,EXE) || !realpath(path,canon) || strcmp(canon,EXE)) return -1;
    int fd=open("/proc/self/exe",O_RDONLY|O_CLOEXEC),installed=open(EXE,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    struct stat a,b;
    int ok=fd>=0 && installed>=0 && !fstat(fd,&a) && !fstat(installed,&b) && same_file(&a,&b) && hash_fd(fd,EXE_SHA);
    if (installed>=0) close(installed);
    if (!ok && fd>=0) {close(fd);fd=-1;}
    return fd;
}
