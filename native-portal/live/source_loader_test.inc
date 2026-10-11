// SPDX-License-Identifier: MIT
// CPU-only mapping/install tests. No dlopen of the ARM DSO on the host.
#include "stock_observer_test.inc"
EXPORT int ft_loader_test_dual(const char *path) {
    int fd=open(path,O_RDONLY|O_CLOEXEC);assert(fd>=0);
    Elf64_Ehdr eh;Elf64_Phdr ph[8];assert(pread(fd,&eh,sizeof eh,0)==sizeof eh);
    assert(eh.e_phnum==8 && pread(fd,ph,sizeof ph,eh.e_phoff)==sizeof ph);
    void *space=mmap(NULL,0x800000,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(space!=MAP_FAILED);
    uintptr_t cb=(uintptr_t)space;
    for(unsigned i=0;i<8;++i)if(ph[i].p_type==PT_LOAD) {
        uintptr_t lo=ph[i].p_vaddr&~4095ULL,off=ph[i].p_offset&~4095ULL;
        size_t len=((ph[i].p_vaddr+ph[i].p_filesz+4095)&~4095ULL)-lo;
        int prot=PROT_READ|((ph[i].p_flags&PF_X)?PROT_EXEC:PROT_WRITE);
        assert(mmap((void*)(cb+lo),len,prot,MAP_PRIVATE|MAP_FIXED,fd,off)==(void*)(cb+lo));
    }
    const Elf64_Phdr *live=(void*)(cb+eh.e_phoff);
    assert(verify_client(fd,cb,live,8));
    assert(!verify_client(fd,cb,live,7));assert(!verify_client(fd,cb+4096,live,8));
    assert(!verify_client(fd,cb,live+1,8));assert(!verify_client(-1,cb,live,8));
    // Full RX comparison rejects changes outside every hook guard.
    assert(!mprotect((void*)(cb+0x1000),4096,PROT_READ|PROT_WRITE));
    *(unsigned char*)(cb+0x1000)^=1;
    assert(!mprotect((void*)(cb+0x1000),4096,PROT_READ|PROT_EXEC));
    assert(!verify_client(fd,cb,live,8));
    assert(!mprotect((void*)(cb+0x1000),4096,PROT_READ|PROT_WRITE));
    *(unsigned char*)(cb+0x1000)^=1;
    assert(!mprotect((void*)(cb+0x1000),4096,PROT_READ|PROT_EXEC));
    assert(verify_client(fd,cb,live,8));
    uint64_t a=0,b=0;assert(!ft_native_portal_source_modules(&a,&b));
    size_t ci_len=0;void *ci=make_module_islands(cb,4096,&ci_len,SITE_COUNT,CLIENT_SITE_COUNT);
    assert(ci!=MAP_FAILED);
    // Every page failure in the second module restores ALL first-module pages,
    // with neither image patched. A missing client guard also writes nothing.
    unsigned cp=0;uintptr_t prior=UINTPTR_MAX;
    for(unsigned i=0;i<CLIENT_SITE_COUNT;++i)if((client_sites[i].offset&~4095ULL)!=prior) {prior=client_sites[i].offset&~4095ULL;++cp;}
    for(unsigned fail=pages()+1;fail<=pages()+cp;++fail) {
        uintptr_t comp=fixture();assert(!mprotect((void*)comp,0x800000,PROT_READ|PROT_EXEC));
        protections=0;fail_at=fail;fail_again=0;
        assert(!install_modules(comp,comp+0x600000,cb,(uintptr_t)ci,4096,protect));
        assert(guards(comp) && client_guards(cb));
        assert(map_range(comp,0x800000,NULL,0,PROT_READ|PROT_EXEC));
        munmap((void*)comp,0x800000);
    }
    uintptr_t comp=fixture();assert(!mprotect((void*)comp,0x800000,PROT_READ|PROT_EXEC));
    protections=fail_at=fail_again=0;
    assert(!install_modules(comp,comp+0x600000,cb,cb+0x10000000,4096,protect) && !protections);
    const uintptr_t bad=cb+client_sites[CLIENT_SITE_COUNT-1].offset;
    assert(!mprotect((void*)(bad&~4095ULL),4096,PROT_READ|PROT_WRITE));
    *(unsigned char*)(bad+15)^=1;
    assert(!mprotect((void*)(bad&~4095ULL),4096,PROT_READ|PROT_EXEC));
    assert(!install_modules(comp,comp+0x600000,cb,(uintptr_t)ci,4096,protect) && !protections);
    assert(guards(comp));
    assert(!mprotect((void*)(bad&~4095ULL),4096,PROT_READ|PROT_WRITE));
    *(unsigned char*)(bad+15)^=1;
    assert(!mprotect((void*)(bad&~4095ULL),4096,PROT_READ|PROT_EXEC));
    // Authenticate selected guards in both modules before any text page
    // becomes writable, including each new observer. Perturb the first word
    // and the tail byte independently; neither may reach the patch phase.
    for(unsigned module=0;module<2;++module) {
        const struct site *table=module?client_sites:sites;
        const unsigned count=module?CLIENT_SITE_COUNT:SITE_COUNT;
        const uintptr_t base=module?cb:comp;
        for(unsigned i=0;i<count;++i)for(unsigned byte=0;byte<16;byte+=15) {
            const uintptr_t at=base+table[i].offset+byte,page=at&~4095ULL;
            assert(!mprotect((void*)page,4096,PROT_READ|PROT_WRITE));
            *(unsigned char*)at^=1;
            assert(!mprotect((void*)page,4096,PROT_READ|PROT_EXEC));
            protections=0;
            assert(!install_modules(comp,comp+0x600000,cb,(uintptr_t)ci,4096,protect) && !protections);
            test_installed_unavailable();
            assert(!mprotect((void*)page,4096,PROT_READ|PROT_WRITE));
            *(unsigned char*)at^=1;
            assert(!mprotect((void*)page,4096,PROT_READ|PROT_EXEC));
            assert(guards(comp) && client_guards(cb));
        }
    }
    pid_t child=fork();assert(child>=0);
    if(!child) {
        protections=0;fail_at=pages()+cp+pages()+1;
        (void)install_modules(comp,comp+0x600000,cb,(uintptr_t)ci,4096,protect);_exit(99);
    }
    int status;assert(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==190);
    assert(guards(comp) && client_guards(cb));
    assert(install_modules(comp,comp+0x600000,cb,(uintptr_t)ci,4096,protect));
    for(unsigned i=0;i<CLIENT_SITE_COUNT;++i) {uint32_t word;
        assert(branch_word(cb+client_sites[i].offset,(uintptr_t)ci+i*STRIDE,&word));
        assert(*(uint32_t*)(cb+client_sites[i].offset)==word);
    }
    test_stock_coverage(comp,cb,(uintptr_t)ci,fd);
    self_pid=getpid();deadline=now_ns()+UINT64_C(1000000000);atomic_store(&enabled,1);
    struct saved s={0};unsigned before=calls;expected_pc=SOURCE_CLIENT_TAG|client_sites[0].offset;
    errno=42;dispatch(&s,SITE_COUNT);assert(calls==before+1 && errno==42);
    expected_pc=0;atomic_store(&enabled,0);
    munmap((void*)comp,0x800000);munmap(ci,ci_len);munmap(space,0x800000);close(fd);
    puts("PASS dual module: complete RX pin, mapped identity, module-tag dispatch, second-image precommit rollback");return 1;
}
