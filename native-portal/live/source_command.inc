// SPDX-License-Identifier: MIT
// Native source observations only. Command owns queue acceptance/GPU truth;
// Evidence/Lifecycle owns CPU disposal. Addresses here grant NO read permission.
    struct SourceRead {
        uint64_t texture=0,image=0,vk_image=0,frame=0;
        bool acquired=false,released=false;
        uint32_t aspect=0,levels=0;
    };
    struct SourceCommand {
        std::array<SourceRead,32> reads{};
        std::array<uint16_t,2> slots{};
        std::array<uint16_t,2> expected{};
        std::array<uint64_t,2> packets{},tickets{};
        binding::Owner owner{};
        uint64_t watermark=0,sample_sp=0,sample_texture=0,barrier_sp=0;
        unsigned count=0,sample_eye=0,sample_slot=0,barrier_index=0,barrier_phase=0;
        bool bad=false,sample=false,barrier=false,reverse=false,sealed=false,attempted=false;
    } source_command;
    bool source_command_live() noexcept {
        CameraBinding held;
        auto reader=evidence.pass_reader();
        return camera_owner.retained_binding(held) && held==camera_binding &&
            reader.live && reader.live(reader.user,{binding.identity,command_owner.recording});
    }
    bool source_command_identity() noexcept {
        binding::Owner now{};
        return evidence.command_owner(now) && now.command==source_command.owner.command &&
            now.vk_command==source_command.owner.vk_command && now.recording==source_command.owner.recording &&
            now.generation==source_command.owner.generation && same(now.identity,source_command.owner.identity);
    }
    bool source_vector(uint64_t texture,uint64_t element=0) noexcept {
        uint64_t begin=0,end=0,capacity=0;
        const auto renderer=source_command.owner.identity.renderer;
        if(!get(renderer+0x1550,begin) || !get(renderer+0x1558,end) || !get(renderer+0x1560,capacity) ||
           !begin || end<begin || capacity<end || (begin|end|capacity)&7 || end-begin>256*8)return false;
        if(element && (element<begin || element>=end || (element-begin)%8))return false;
        unsigned matches=0;
        for(uint64_t p=begin;p<end;p+=8) {uint64_t t=0;if(!get(p,t))return false;if(t==texture)++matches;}
        uint64_t selected=0;
        return matches==1 && (!element || (get(element,selected) && selected==texture));
    }
    bool source_barrier(const arena::Saved& s,bool reverse,unsigned index) noexcept {
        auto& c=source_command;const auto& read=c.reads[index];
        uint32_t src=0,dst=0,level=0,layers=0,native_family=0,external_family=0;
        uint64_t marker=0,frame=0,device=0,vk=0;
        const auto renderer=c.owner.identity.renderer;
        if(c.barrier || !source_command_identity() || s.x[19]!=renderer ||
           s.x[0]!=read.image || s.x[1]!=c.owner.command ||
           uint32_t(s.x[2])!=(reverse?5u:1u) || uint32_t(s.x[3])!=(reverse?1u:5u) ||
           uint32_t(s.x[4])!=1 || uint32_t(s.x[5]) || uint32_t(s.x[6])!=8 || uint32_t(s.x[7])!=0x20 ||
           !get(s.sp,level) || level || !get(s.sp+8,layers) || layers!=1 ||
           !get(s.sp+16,src) || !get(s.sp+24,dst) ||
           !get(renderer+0x768,native_family) || !get(renderer+0xb20,external_family) ||
           (external_family!=uint32_t(-3) && external_family!=uint32_t(-2)) ||
           src!=(reverse?native_family:external_family) || dst!=(reverse?external_family:native_family) ||
           !get(read.image+8,device) || device!=c.owner.identity.renderer+0x50 ||
           !get(read.image+0xb0,vk) || vk!=read.vk_image ||
           !get(read.image+0xd8,marker) || marker!=(reverse?UINT64_MAX:read.frame) ||
           !get(renderer+0x13f8,frame) || frame!=read.frame)return false;
        c.barrier=true;c.reverse=reverse;c.barrier_index=index;c.barrier_sp=s.sp;c.barrier_phase=0;return true;
    }
    void source_command_event(uint64_t pc,const arena::Saved& s) noexcept {
        auto& c=source_command;
        const bool sample_pc=pc==0x29f294 || pc==0x29f298;
        const bool acquire_pc=pc==0x29ac80 || pc==0x29ac84;
        const bool release_pc=pc==0x2a255c || pc==0x2a2560;
        const bool inner_pc=pc==0x2fe72c || pc==0x2fe730 || pc==0x2fe738 || pc==0x2fe73c;
        if(!sample_pc && !acquire_pc && !release_pc && !inner_pc)return;
        if(!c.watermark || c.attempted || c.bad)return;
        if(sample_pc && (!backend_open || !backend_admitted))return; // unrelated stock encoder
        if(acquire_pc && !c.sample)return;
        if(inner_pc && !c.barrier)return;
        if(!source_command_live()) {c.bad=true;return;} // retain BEFORE any native pointer read
        if(pc==0x29f294) {
            const auto& e=prepared.eyes[backend_preparation];
            if(c.sample || c.barrier || backend_eye>1 || s.x[0]!=binding.identity.renderer || s.x[21]!=s.x[0] ||
               s.x[24]!=e.lookup.x[1] || s.sp+0x90!=backend_scope.sp ||
               s.x[20]<s.x[24]+0x80 || s.x[20]>=s.x[24]+0x100 || (s.x[20]-s.x[24])%8 ||
               uint32_t(s.x[2])!=8 || uint32_t(s.x[3])!=0x20 || !source_command_identity()) {c.bad=true;return;}
            const unsigned slot=unsigned((s.x[20]-s.x[24]-0x80)/8);
            uint64_t expected=0;std::memcpy(&expected,e.record.prefix.data()+0x80+slot*8,8);
            uint64_t actual=0,shared=0,image=0,vk=0,frame=0,device=0;
            if(!expected || s.x[1]!=expected || (c.slots[backend_eye]&(1u<<slot)) ||
               !get(s.x[20],actual) || actual!=expected || !get(expected+0x28,shared) || !shared ||
               !get(expected+0x48,image) || !image || !get(image+8,device) || device!=c.owner.identity.renderer+0x50 ||
               !get(image+0xb0,vk) || !vk || !get(binding.identity.renderer+0x13f8,frame)) {c.bad=true;return;}
            unsigned index=0;while(index<c.count && c.reads[index].texture!=expected)++index;
            if(index==c.count) {
                if(c.count==c.reads.size()){c.bad=true;return;}
                c.reads[c.count++]={expected,image,vk,frame,false,false};
            } else if(c.reads[index].image!=image || c.reads[index].vk_image!=vk ||
                      c.reads[index].frame!=frame || c.reads[index].released) {c.bad=true;return;}
            c.sample=true;c.sample_sp=s.sp;c.sample_texture=expected;c.sample_eye=backend_eye;c.sample_slot=slot;
        } else if(pc==0x29ac80) {
            unsigned index=0;while(index<c.count && c.reads[index].texture!=c.sample_texture)++index;
            if(index==c.count || c.reads[index].acquired || s.sp+0x50!=c.sample_sp ||
               !source_barrier(s,false,index))c.bad=true;
        } else if(pc==0x29ac84 || pc==0x2a2560) {
            if(!c.barrier)return; // unrelated stock helper; no owned completion
            if(c.reverse!=(pc==0x2a2560) || c.barrier_phase!=4 || s.sp!=c.barrier_sp ||
               s.x[19]!=binding.identity.renderer) {c.bad=true;return;}
            if(c.reverse)c.reads[c.barrier_index].released=true;
            else c.reads[c.barrier_index].acquired=true;
            c.barrier=false;
        } else if(pc==0x29f298) {
            unsigned index=0;while(index<c.count && c.reads[index].texture!=c.sample_texture)++index;
            if(!c.sample || c.barrier || s.sp!=c.sample_sp || s.x[24]!=c.packets[c.sample_eye] ||
               index==c.count || !c.reads[index].acquired || !source_vector(c.sample_texture)) {c.bad=true;return;}
            c.slots[c.sample_eye]|=uint16_t(1u<<c.sample_slot);c.sample=false;
        } else if(pc==0x2a255c) {
            unsigned index=0;while(index<c.count && c.reads[index].image!=s.x[0])++index;
            if(index==c.count)return; // stock source release, unchanged
            if(c.sample || !c.reads[index].acquired || c.reads[index].released ||
               !source_vector(c.reads[index].texture,s.x[20]) || !source_barrier(s,true,index))c.bad=true;
        } else {
            auto& read=c.reads[c.barrier_index];
            const unsigned phase=pc==0x2fe72c?0:pc==0x2fe730?1:pc==0x2fe738?2:3;
            if(phase!=c.barrier_phase || s.sp+0xc0!=c.barrier_sp ||
               s.x[19]!=read.image || s.x[20]!=c.owner.command) {c.bad=true;return;}
            if(pc==0x2fe72c) {
                // Actual vkCmdPipelineBarrier payload, not merely helper entry.
                struct ImageBarrier {uint32_t type,pad;uint64_t next;uint32_t src_access,dst_access,old_layout,new_layout,
                    src_family,dst_family;uint64_t image;uint32_t aspect,mip,levels,layer,layers,pad2;} b{};
                uint64_t pointer=0;uint32_t count=0,src=0,dst=0;
                if(s.x[0]!=c.owner.vk_command || s.x[1]!=1 || s.x[2]!=8 || s.x[3] || s.x[4] || s.x[6] ||
                   !get(s.sp,count) || count!=1 || !get(s.sp+8,pointer) || pointer!=s.sp+0x70 || !get(pointer,b) ||
                   !get(c.barrier_sp+16,src) || !get(c.barrier_sp+24,dst) || b.type!=45 || b.next ||
                   b.src_access || b.dst_access!=0x20 || b.old_layout!=(c.reverse?5u:1u) ||
                   b.new_layout!=(c.reverse?1u:5u) || b.src_family!=src || b.dst_family!=dst ||
                   b.image!=read.vk_image || !b.aspect || b.mip || !b.levels || b.layer || b.layers!=1) {c.bad=true;return;}
                if(c.reverse && (b.aspect!=read.aspect || b.levels!=read.levels)) {c.bad=true;return;}
                if(!c.reverse){read.aspect=b.aspect;read.levels=b.levels;}
            }
            if(pc==0x2fe738 && (s.x[0]!=c.owner.command || s.x[1]!=read.image)){c.bad=true;return;}
            ++c.barrier_phase;
        }
    }
    bool source_command_seal() noexcept {
        auto& c=source_command;
        if(c.attempted)return false;
        c.attempted=true;
        if(c.bad || c.sample || c.barrier || !c.watermark || !source_command_live() || !source_command_identity())return false;
        if(!c.tickets[0] || !c.tickets[1] || c.tickets[0]==c.tickets[1] ||
           provenance[0].ticket!=c.tickets[0] || provenance[1].ticket!=c.tickets[1])return false;
        // The existing authenticated two-skip transaction encoded no source
        // read. Its uploads still need the SAME queue/wait/disposal/reset proof.
        if(rejected_pair && !c.count && !c.slots[0] && !c.slots[1] &&
           !encoded_returns[0] && !encoded_returns[1]) {c.sealed=true;return true;}
        for(unsigned eye=0;eye<2;++eye) {
            if(!c.expected[eye] || c.expected[eye]!=c.slots[eye] || !encoded_returns[eye] ||
               !provenance[eye].ticket || provenance[eye].ticket!=c.tickets[eye])return false;
        }
        if(!c.count)return false;
        for(unsigned i=0;i<c.count;++i)if(!c.reads[i].acquired || !c.reads[i].released)return false;
        c.sealed=true;return true;
    }
    bool source_command_settled(uint64_t watermark) noexcept {
        // No private-pointer reads here. Exact accepted Command and Lifecycle
        // certificates remain the only authorities for GPU/CPU retirement.
        Completion token{};
        return source_command.sealed && !source_command.bad && source_command.watermark==watermark && command &&
            command->submission(provenance[0],provenance[1],token) && command->completed(binding,token) &&
            evidence.settled(binding,provenance[0],provenance[1],token);
    }
