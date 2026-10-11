// SPDX-License-Identifier: MIT
// Called only AFTER the real CPU dual-image installer succeeds. The client RX
// mapping and file identity are real; this token models the retained dlopen
// reference since an ARM DSO cannot be dlopened on the x86 host.
static void test_installed_unavailable(void) {
    uint64_t out[ALL_SITE_COUNT],prior[ALL_SITE_COUNT];
    memset(out,0xa5,sizeof out);memcpy(prior,out,sizeof out);
    for(unsigned module=0;module<2;++module) {
        assert(!ft_native_portal_installed_sites(FT_INSTALLED_SITES_VERSION,
            module?SOURCE_CLIENT_TAG:0,out,ALL_SITE_COUNT));
        assert(!memcmp(out,prior,sizeof out));
    }
}
static void test_stock_coverage(uintptr_t comp,uintptr_t client,uintptr_t ci,int fd) {
    struct ft_stock_observer_coverage_v1 s,zero={0};
    assert(!ft_native_portal_stock_observer_coverage(1,&s,sizeof s));test_installed_unavailable();
    assert(!memcmp(&s,&zero,sizeof s));
    self_pid=getpid();image_base=comp;client_base=client;
    image_islands=comp+0x600000;client_islands=ci;
    client_handle=&s;client_fd=fd;assert(!fstat(fd,&client_stat));
    atomic_store(&enabled,1);
    // Inventory comes from the selected module table AFTER installation. Check
    // its exact patch destinations as well as count, tag and bounded copy ABI.
    for(unsigned module=0;module<2;++module) {
        const uint64_t tag=module?SOURCE_CLIENT_TAG:0;
        const size_t n=module?CLIENT_SITE_COUNT:SITE_COUNT;
        const struct site *table=module?client_sites:sites;
        uint64_t pcs[ALL_SITE_COUNT+2],prior_pcs[ALL_SITE_COUNT+2];
        memset(pcs,0xa5,sizeof pcs);memcpy(prior_pcs,pcs,sizeof pcs);
        assert(ft_native_portal_installed_sites(1,tag,NULL,0)==n);
        assert(ft_native_portal_installed_sites(1,tag,pcs+1,n-1)==n);
        assert(!memcmp(pcs,prior_pcs,sizeof pcs));
        assert(!ft_native_portal_installed_sites(2,tag,pcs+1,n));
        assert(!ft_native_portal_installed_sites(1,1,pcs+1,n));
        assert(!memcmp(pcs,prior_pcs,sizeof pcs));
        assert(ft_native_portal_installed_sites(1,tag,pcs+1,n)==n);
        assert(pcs[0]==prior_pcs[0] && pcs[n+1]==prior_pcs[n+1]);
        for(size_t i=0;i<n;++i) {
            assert(pcs[i+1]==(table[i].offset|tag));
            uint32_t word;uintptr_t pc=(module?client:comp)+table[i].offset;
            assert(branch_word(pc,(module?ci:image_islands)+i*STRIDE,&word));
            assert(*(uint32_t*)pc==word);
        }
    }
#ifndef FT_TEST_STOCK_MASK
#define FT_TEST_STOCK_MASK FT_STOCK_OBSERVER_REQUIRED
#endif
#ifdef FT_STOCK_OBSERVER_SELECTED_MASK
    assert(ft_native_portal_stock_observer_coverage(1,&s,sizeof s));
    assert(s.version==1 && s.size==sizeof s && s.installed_mask==FT_TEST_STOCK_MASK);
    assert(s.required_mask==FT_STOCK_OBSERVER_REQUIRED && s.compositor_base==comp && s.client_base==client);
    assert(ft_stock_observers_complete(&s)==(FT_TEST_STOCK_MASK==FT_STOCK_OBSERVER_REQUIRED));
#else
    assert(!ft_native_portal_stock_observer_coverage(1,&s,sizeof s));
    assert(!ft_stock_observers_complete(&s));
#endif
    assert(ft_native_portal_camera_coverage()==UINT64_C(0xfffff));
#ifdef FT_TEST_CURRENT_COVERAGE
#include "current-required.h"
    uint64_t current_pcs[ALL_SITE_COUNT];
    const size_t current_count=ft_native_portal_installed_sites(FT_INSTALLED_SITES_VERSION,0,current_pcs,ALL_SITE_COUNT);
    assert(current_count==SITE_COUNT);
    unsigned missing=0;
    for(size_t r=0;r<sizeof current_required/sizeof current_required[0];++r) {
        unsigned found=0;
        for(size_t i=0;i<current_count;++i)found+=current_pcs[i]==current_required[r];
        assert(found<=1);
        if(!found) {++missing;assert(current_required[r]==FT_TEST_CURRENT_OMITTED);}
    }
    assert((missing==0)==FT_TEST_CURRENT_COVERAGE);
    assert(missing==(FT_TEST_CURRENT_COVERAGE?0:1));
    printf("PASS current-render actual installed inventory complete=%u missing=%u; V1/camera intact\n",missing==0,missing);
#endif
#ifdef FT_TEST_ADMISSION_COVERAGE
#include "admission-required.h"
    uint64_t admission_pcs[ALL_SITE_COUNT];
    const size_t comp_count=ft_native_portal_installed_sites(FT_INSTALLED_SITES_VERSION,0,admission_pcs,ALL_SITE_COUNT);
    const size_t client_count=ft_native_portal_installed_sites(FT_INSTALLED_SITES_VERSION,SOURCE_CLIENT_TAG,
        admission_pcs+comp_count,ALL_SITE_COUNT-comp_count);
    assert(comp_count==SITE_COUNT && client_count==CLIENT_SITE_COUNT);
    unsigned admission_missing=0;
    for(size_t r=0;r<sizeof admission_required/sizeof admission_required[0];++r) {
        unsigned found=0;
        for(size_t i=0;i<comp_count+client_count;++i)found+=admission_pcs[i]==admission_required[r];
        assert(found<=1);
        if(!found) {++admission_missing;assert(admission_required[r]==FT_TEST_ADMISSION_OMITTED);}
    }
    assert(admission_missing==(FT_TEST_ADMISSION_COVERAGE?0:1));
    printf("PASS admission actual dual-module installed inventory complete=%u missing=%u; stale V1 full mask intact\n",
        admission_missing==0,admission_missing);
#endif
    memset(&s,0xa5,sizeof s);struct ft_stock_observer_coverage_v1 prior=s;
    assert(!ft_native_portal_stock_observer_coverage(2,&s,sizeof s));
    assert(!memcmp(&s,&prior,sizeof s));
    assert(!ft_native_portal_stock_observer_coverage(1,&s,sizeof s-1));
    assert(!memcmp(&s,&prior,sizeof s));
    assert(!ft_native_portal_stock_observer_coverage(1,NULL,sizeof s));
    pid_t child=fork();assert(child>=0);
    if(!child) {
        assert(!ft_native_portal_stock_observer_coverage(1,&s,sizeof s));test_installed_unavailable();
        assert(!memcmp(&s,&zero,sizeof s));_exit(0);
    }
    int status;assert(waitpid(child,&status,0)==child && status==0);
    atomic_store(&enabled,0);
    assert(!ft_native_portal_stock_observer_coverage(1,&s,sizeof s));test_installed_unavailable();
    assert(!memcmp(&s,&zero,sizeof s));atomic_store(&enabled,1);
    client_handle=NULL;
    assert(!ft_native_portal_stock_observer_coverage(1,&s,sizeof s));test_installed_unavailable();client_handle=&s;
    client_fd=-1;
    assert(!ft_native_portal_stock_observer_coverage(1,&s,sizeof s));test_installed_unavailable();client_fd=fd;
    ++client_stat.st_ino;
    assert(!ft_native_portal_stock_observer_coverage(1,&s,sizeof s));test_installed_unavailable();--client_stat.st_ino;
    client_islands=0;
    assert(!ft_native_portal_stock_observer_coverage(1,&s,sizeof s));test_installed_unavailable();client_islands=ci;
    image_islands=0;
    assert(!ft_native_portal_stock_observer_coverage(1,&s,sizeof s));test_installed_unavailable();
    client_fd=-1;client_handle=NULL;client_islands=client_base=image_base=0;atomic_store(&enabled,0);
    printf("PASS stock observer installed mask=%#llx; legacy camera mask unchanged; version/fork/lifetime gates\n",
           (unsigned long long)FT_TEST_STOCK_MASK);
}
