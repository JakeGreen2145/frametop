// SPDX-License-Identifier: MIT
// Focused source-model continuation after the unmodified full default Runtime
// bootstrap/collector/current-selection/Adapter recording prelude. The queue,
// stamp, wait, transfer, pool, disposal and reset sequence matches run_chain's
// original tail. Every case traverses it, including unknown caller poisoning.
#include "consumer.private.hpp"
extern "C" void present_snapshot(Runtime&,const char*,const char*,unsigned,unsigned);
static unsigned consumer_skips=0;
static void consumer_snapshot(Chain& f,const char* phase) {
    present_snapshot(f.router,current_mode.c_str(),phase,consumer_skips,f.lookups);
}
static int consumer_chain(Chain& f,Client& client,Request& request,Reply& reply) {
    consumer_skips=0;
    const bool flush=current_mode=="consumer-flush";
    const bool cold=current_mode=="consumer-coldpass";
    const bool unknown=current_mode=="consumer-unknown";
    const bool present=current_mode=="consumer-present";
    const auto d=f.router.diagnostics();
    assert(d.uploads[0]==1 && d.uploads[1]==1 && f.enqueues==2 && !f.lookups && !d.queue_success && !d.submitted_frame);
    consumer_snapshot(f,"recorded");
    if(cold)put(f.cache,0x38,uint32_t(0));
    // Known call lanes derived from verified lane-C native windows by the runner.
    // Objects, SP and all unobserved register lanes remain explicit source models.
    f.s={};f.s.sp=0x900000;f.s.x[0]=addr(f.r);f.s.x[1]=consumer_mode;
    f.s.x[2]=Chain::base+0x2a1e18;
    f.s.x[30]=Chain::base+(unknown?0xdead:flush || cold?consumer_flush_lr:consumer_present_lr);
    std::fprintf(stderr,"CONSUMER stimulus mode=%s lr=0x%llx x0_is_R=%u w1=%u slot=0x%llx\n",current_mode.c_str(),
        (unsigned long long)(f.s.x[30]-Chain::base),f.s.x[0]==addr(f.r),unsigned(f.s.x[1]),(unsigned long long)consumer_slot);
    f.event(0x2a1e18);consumer_snapshot(f,"dispatch");
    // A future authorized repair must be able to turn the functional test GREEN
    // without removing diagnostic assertions or bypassing the cleanup tail.
    assert(present?(f.lookups==0 || f.lookups==2):f.lookups==(flush?2u:0u));
    const bool draws=flush || (present && f.lookups==2);
    for(unsigned eye=0;eye<2;++eye) {
        f.s={};f.s.sp=addr(f.source_stack)+0x2e0;f.s.x[30]=Chain::base+0x2a1c58;
        f.s.x[19]=addr(f.packets)+eye*0x2b0;f.s.x[21]=addr(f.r);f.s.x[22]=addr(f.passes)+eye*0x98;
        f.event(0x2a1c18);
        assert(f.s.pad==(draws?0u:1u));
        if(f.s.pad)consumer_skips|=1u<<eye;
        else {
            f.s.x[0]=addr(f.r);f.s.x[1]=addr(f.packets)+eye*0x2b0;f.s.x[2]=0xa001;f.s.x[3]=37;
            f.event(0x29f1e8);f.event(0x29f200);
            assert(f.s.pad==arena::SkipOwned && f.s.x[0]==addr(f.pipeline_object));
            f.source_sample(eye);f.s.x[30]=Chain::base+0x2a1c58;f.event(0x29f518);
        }
    }
    // Non-owned packet remains native, even when whole graph authority is absent.
    std::array<std::byte,0x2b0> stock{};f.s={};f.s.sp=addr(f.source_stack)+0x2e0;f.s.x[30]=Chain::base+0x2a1c58;
    f.s.x[21]=addr(f.r);f.s.x[19]=addr(stock);f.s.x[22]=addr(f.passes);f.event(0x2a1c18);assert(!f.s.pad);
    consumer_snapshot(f,"packets");
    request.op=Operation::Release;++request.sequence;assert(client.exchange(request,reply) && reply.state==State::Draining);
    consumer_snapshot(f,"off");
    if(draws)f.source_reverse(); // skipped native draws never acquire sampled-source debt
    uint64_t cb=0xc001,sem=0xa004,value=binding::Command::timeline_value({addr(f.sync),f.signal_value.serial,1});
    TimelineSubmit timeline{};SubmitInfo submit{};
    timeline.type=1000207003;timeline.signal_count=1;timeline.signals=uint64_t(&value);
    submit.type=4;submit.next=uint64_t(&timeline);submit.command_count=1;submit.commands=uint64_t(&cb);
    submit.signal_count=1;submit.signals=uint64_t(&sem);
    f.s={};f.s.sp=0x880000;f.s.x[0]=0xbeef;f.s.x[1]=addr(f.executable);f.s.x[30]=Chain::base+FT_BOOT_QUEUE_LR;f.event(0x3018b0);
    f.s.x[19]=addr(f.executable);f.s.x[0]=0xa005;f.s.x[1]=1;f.s.x[2]=uint64_t(&submit);f.s.x[3]=0;f.event(0x3021d4);
    f.s.x[0]=0;f.event(0x3021d8);
    f.s.x[0]=addr(f.executable);f.s.x[1]=addr(f.sync);f.s.x[2]=uint64_t(&f.signal_value);f.event(0x302208);
    for(const auto object:{addr(f.resource),addr(f.executable)}) {
        f.s.sp=0x870000;f.s.x[0]=object;f.s.x[1]=addr(f.sync);f.s.x[2]=uint64_t(&f.signal_value);f.event(0x310ea0);
        std::memcpy(reinterpret_cast<void*>(object+0x10),&f.signal_pointer,8);
        std::memcpy(reinterpret_cast<void*>(object+0x18),&f.signal_value.serial,8);
        std::memcpy(reinterpret_cast<void*>(object+0x20),&f.signal_value.subserial,1);
        f.s.x[19]=object;f.s.sp-=0x30;f.event(0x310f20);
    }
    f.s.sp=0x880000;f.event(0x30220c);consumer_snapshot(f,"queue");
    f.s.sp=0x860000;f.s.x[0]=addr(f.sync);f.s.x[1]=uint64_t(&f.signal_value);f.s.x[30]=Chain::base+0x311104;f.event(0x3160b8);
    f.s.sp-=0x40;f.s.x[0]=0;f.event(0x316110);f.s.sp+=0x40;f.s.x[0]=1;f.event(0x316120);
    consumer_snapshot(f,"wait");
    f.s.sp=0x8fff30;f.s.x[19]=addr(f.r);f.event(0x2a2230);
    put(f.r,0x15a8,addr(f.passes));put(f.r,0x15b8,addr(f.passes));put(f.r,0x15c0,addr(f.passes)+f.passes.size());f.event(0x2a2320);
    put(f.r,0x1380,uint64_t(0));f.s.x[0]=addr(f.command);f.s.x[30]=Chain::base+FT_BOOT_RELEASE_LR;f.event(0x305260);
    put(f.executable,0x50,addr(f.bootstrap_pool));
    f.s.sp-=0x30;f.s.x[0]=addr(f.bootstrap_pool)+0xd0;f.s.x[1]=addr(f.executable);
    f.s.x[19]=addr(f.command);f.s.x[30]=Chain::base+FT_BOOT_POOL_LR;f.event(0x311648);
    std::array<std::byte,0x7b0> next_command{};std::array<std::byte,0x10> next_wrapper{};
    put(next_command,8,addr(f.r)+0x50);put(next_command,0x738,uint64_t(0xc002));put(next_wrapper,8,addr(next_command));
    put(next_command,0x428,uint64_t(&f.signal_pointer));put(next_command,0x430,uint64_t(&f.signal_pointer)+8);
    put(next_command,0x50,addr(f.bootstrap_pool));put(next_command,0x4f0,uint64_t(&f.signal_value));
    f.s.x[0]=f.s.x[20]=addr(next_command);f.s.sp=addr(f.stack)+0x100;f.event(0x301808);
    f.s.sp-=0x40;f.s.x[19]=addr(next_command);f.s.x[30]=Chain::base+0x300ae0;f.event(0x300a08);
    f.s.sp-=0x30;put(f.stack,0x98,uint64_t(Chain::base+0x300ae0));f.s.x[0]=0;f.event(0x300a38);
    f.s.sp=addr(f.stack)+0x100;f.s.x[20]=addr(next_command);f.s.x[0]=1;f.event(0x30180c);
    f.s.sp=0x8fff30;f.s.x[19]=addr(f.r);f.s.x[0]=addr(next_wrapper);f.event(0x2a23f4);put(f.r,0x1380,addr(next_wrapper));
    consumer_snapshot(f,"transfer");
    f.s.x[22]=addr(f.r);f.event(0x2987d8);put(f.r,0x15c0,addr(f.passes));f.event(0x298840);
    consumer_snapshot(f,"disposal");
    f.s.sp=0x840000;f.s.x[0]=addr(f.executable);f.s.x[30]=Chain::base+0x301000;f.event(0x300b20);
    f.s.sp-=0x20;f.s.x[19]=addr(f.executable);f.s.x[0]=0;f.event(0x300c54);
    f.s.sp+=0x20;f.s.x[0]=addr(f.executable);f.event(0x300a08);
    f.s.sp-=0x30;f.s.x[0]=0;f.event(0x300a38);consumer_snapshot(f,"reset");
    ++request.sequence;assert(client.exchange(request,reply));
    std::fprintf(stderr,"CONSUMER settlement mode=%s state=%u pending=%u submitted_frame=%llu disconnects=%u clears=%u\n",
        current_mode.c_str(),unsigned(reply.state),f.router.diagnostics().pending,
        (unsigned long long)reply.submitted_frame,f.native_releases,f.native_clears);
    // Continue ordinary owner stop, OS join, exact native join callbacks and
    // cleanup in scenario(). No trailing assert(false) supplies an abort.
    return 0;
}
