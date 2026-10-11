// SPDX-License-Identifier: MIT
// Fixed original Update read/release path. These facts do not prove publication,
// waiter drain, immutable generations, registry ref success or source settlement.
bool ClientSourceBinding::metadata_event(Module module,uint64_t pc,const arena::Saved& s) noexcept {
    auto& m=metadata_;
    if(module==Module::Compositor) {
        unsigned q=0;bool start=false,releasing=false;uint64_t ret=0;
        switch(pc) {
        case 0x15fee4:q=1;start=true;ret=0x15fee8;break;
        case 0x160184:start=true;ret=0x160188;break;
        case 0x160164:q=1;start=true;releasing=true;ret=0x160168;break;
        case 0x160380:start=true;releasing=true;ret=0x160384;break;
        case 0x1605c4:start=true;releasing=true;ret=0x1605c8;break;
        case 0x15fee8:case 0x160188:case 0x160168:case 0x160384:case 0x1605c8:break;
        default:return refuse();
        }
        if(s.x[19]!=binding_.camera || !s.sp)return refuse();
        if(start) {
            if(m.pending || s.sp>UINT64_MAX-0x98)return refuse();
            uint64_t id=0,vptr=0,target=0;
            const uint64_t expected=client_base_+(releasing?0x327640:0x3280a8);
            if(native_inputs_) {
                if(!selected_ || selected_->queue!=q || selected_->value!=s.x[1])return refuse();
                id=selected_->value;
            } else if(!read(binding_.camera+0x350+q*8,&id,8))return refuse();
            if(!id || s.x[1]!=id ||
               (provider_ && s.x[0]!=provider_) || !read(s.x[0],&vptr,8) || vptr!=client_base_+0x60f108 ||
               !read(vptr+(releasing?0x38:0x30),&target,8) || target!=expected ||
               s.x[releasing?3:5]!=expected)return refuse();
            auto& r=observation_.reads[q];
            if(releasing) {
                if(!r.acquire_observed || r.acquire_result || r.release_observed || s.x[2]!=r.metadata || r.id!=id)return refuse();
            } else if(r.acquire_observed || s.x[2]!=s.sp+0x90 || s.x[3]!=s.sp+0x98 || uint32_t(s.x[4]))return refuse();
            provider_=s.x[0];observation_.provider=provider_;
            m={};m.pending=true;m.releasing=releasing;m.queue=q;m.sp=s.sp;m.return_pc=ret;m.id=id;
            m.metadata_slot=s.sp+0x90;m.data_slot=s.sp+0x98;m.metadata=releasing?s.x[2]:0;
            return true;
        }
        // This join also follows failed layout acquisition without a release.
        if(pc==0x160168 && !m.pending)return true;
        if(!m.pending || pc!=m.return_pc || s.sp!=m.sp || !m.entered)return refuse();
        auto& r=observation_.reads[m.queue];const int32_t result=static_cast<int32_t>(s.x[0]);
        if(m.inner && (!m.inner_returned || result!=m.inner_result))return refuse();
        if(m.releasing) {r.release_observed=true;r.release_result=result;}
        if(!result) {
            if(!m.inner_entered || !m.inner_returned || !m.ref_after || !m.property_returned || !m.header)return refuse();
            if(m.releasing) {
                r.returned=true;r.decrement=true;r.release_result=result;
                r.metadata_return_observed=true;r.metadata_return=m.property_return;
            } else {
                uint64_t metadata=0,data=0;
                if(!read(m.metadata_slot,&metadata,8) || metadata!=m.metadata || !metadata ||
                   !read(m.data_slot,&data,8) || !data)return refuse();
                r.acquire_observed=true;r.increment=true;r.acquire_result=result;r.disposition=AcquireDisposition::Granted;
                r.metadata=metadata;r.data=data;r.id=m.id;r.queue=m.object;r.header=m.header;r.ref_address=m.ref_address;
            }
        } else if(m.releasing)r.release_result=result;
        else {
            r.acquire_observed=true;r.acquire_result=result;r.id=m.id;
            if(native_inputs_ && result==3 && m.inner_entered && m.inner_returned && m.empty_path && m.header &&
               !m.property && !m.property_returned && !m.ref_before && !m.ref_after) {
                r.disposition=AcquireDisposition::RecognizedNoGrant;r.queue=m.object;r.header=m.header;
            }
        }
        m={};return true;
    }
    if(!m.pending)return refuse();
    const uint64_t manager_entry=m.releasing?0x327640:0x3280a8;
    const uint64_t inner_call=m.releasing?0x3276c4:0x32813c;
    const uint64_t inner_entry=m.releasing?0x3274d8:0x327e18;
    const uint64_t manager_frame=m.releasing?0x40:0x50,inner_frame=m.releasing?0x60:0x70;
    if(m.sp<manager_frame+inner_frame)return refuse();
    if(pc==manager_entry) {
        if(m.entered || s.sp!=m.sp || s.x[0]!=provider_ || s.x[1]!=m.id ||
           s.x[30]!=compositor_base_+m.return_pc ||
           (m.releasing?s.x[2]!=m.metadata:(s.x[2]!=m.metadata_slot || s.x[3]!=m.data_slot || uint32_t(s.x[4]))))return refuse();
        m.entered=true;return true;
    }
    if(!m.entered)return refuse();
    if(pc==inner_call) {
        if(m.inner || s.sp!=m.sp-manager_frame || s.x[19]!=m.id || s.x[20]!=provider_+0x70 ||
           !s.x[0] || (m.releasing?s.x[1]!=m.metadata:(s.x[1]!=m.metadata_slot || s.x[2]!=m.data_slot || uint32_t(s.x[3]))))return refuse();
        if(m.releasing && s.x[0]!=observation_.reads[m.queue].queue)return refuse();
        m.inner=true;m.object=s.x[0];return true;
    }
    if(pc==inner_entry) {
        if(!m.inner || m.inner_entered || s.sp!=m.sp-manager_frame || s.x[0]!=m.object ||
           s.x[30]!=client_base_+inner_call+4 ||
           (m.releasing?s.x[1]!=m.metadata:(s.x[1]!=m.metadata_slot || s.x[2]!=m.data_slot || uint32_t(s.x[3]))))return refuse();
        m.inner_entered=true;return true;
    }
    if(pc==inner_call+4) {
        if(!m.inner_entered || m.inner_returned || s.sp!=m.sp-manager_frame)return refuse();
        m.inner_returned=true;m.inner_result=static_cast<int32_t>(s.x[0]);return true;
    }
    if(!m.inner_entered || s.sp!=m.sp-manager_frame-inner_frame ||
       s.x[m.releasing?21:20]!=m.object)return refuse();
    if(pc==(m.releasing?0x32750c:0x327e54)) {
        uint64_t id=0;
        if(m.header || !s.x[0] || s.x[0]>UINT64_MAX-0x470)return refuse();
        // Acquire has not succeeded yet. Its native output and header may be
        // stale. Associate the by-value pointer with this exact selected call.
        if(m.releasing && (!read(s.x[0],&id,8) || id!=m.id))return refuse();
        m.header=s.x[0];return true;
    }
    if(pc==0x327e60) {
        if(m.releasing || !m.header || m.latest_observed || m.property || m.ref_before)return refuse();
        m.latest_observed=true;
        m.empty_path=uint32_t(s.x[19])==0x80 && (s.nzcv&0xf0000000)==0x60000000;
        if((uint32_t(s.x[19])==0x80)!=bool(s.nzcv&0x40000000))return refuse();
        return true;
    }
    if(m.empty_path)return refuse(); // no property/ref work exists on this path
    if(pc==(m.releasing?0x3275a0:0x327f2c)) {
        if(m.property || (m.releasing?s.x[1]!=m.metadata:s.x[1]!=m.metadata_slot))return refuse();
        m.property=true;return true;
    }
    if(pc==(m.releasing?0x3275a4:0x327f30)) {
        if(!m.property || m.property_returned)return refuse();
        m.property_returned=true;m.property_return=s.x[0];
        if(!m.releasing && uint32_t(s.x[0])==0) {
            uint64_t id=0;
            // Needed only for a real grant, with actual manager/shared locks.
            if(!m.header || !read(m.header,&id,8) || id!=m.id)return refuse();
            if(!read(m.metadata_slot,&m.metadata,8) || !m.metadata)return refuse();
        }
        return true;
    }
    const uint64_t store=m.releasing?0x32758c:0x327f70;
    if(pc==store) {
        if(m.ref_before || !m.header)return refuse();
        uint64_t address=0;
        if(m.releasing) {
            if(s.x[1]>UINT64_MAX-s.x[24])return refuse();
            address=s.x[1]+s.x[24];
            const auto& r=observation_.reads[m.queue];
            if(address!=r.ref_address || m.header!=r.header)return refuse();
        } else {
            if(s.x[19]>UINT64_MAX-0x68)return refuse();
            address=s.x[19]+0x68;
            if(address<m.header+0x68 || address>=m.header+0x468 || (address-m.header-0x68)%8)return refuse();
        }
        uint32_t before=0,after=uint32_t(s.x[0]);
        if(!read(address,&before,4) || (m.releasing?(!before || before-1!=after):(before==UINT32_MAX || before+1!=after)))return refuse();
        m.ref_before=true;m.ref_address=address;m.ref_value=after;return true;
    }
    if(pc==store+4) {
        uint32_t actual=0;
        if(!m.ref_before || m.ref_after || !read(m.ref_address,&actual,4) || actual!=m.ref_value)return refuse();
        m.ref_after=true;return true;
    }
    return refuse();
}
