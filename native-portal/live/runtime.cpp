// SPDX-License-Identifier: MIT
#include "runtime.hpp"
#include "diagnostics.hpp"
#include "command_trace.hpp"
#include <chrono>
#include <fcntl.h>
#include <sys/socket.h>
#include <signal.h>
#include "../pipeline/ft_pipeline.h"
#include <atomic>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <optional>
#include <sys/syscall.h>
#include <unistd.h>
#pragma weak ft_pipeline_generation
#pragma weak ft_pipeline_begin
#pragma weak ft_pipeline_end
#pragma weak ft_pipeline_approved
#if !defined(__aarch64__) && !defined(FT_NATIVE_PORTAL_TEST_CALL)
extern "C" bool ft_native_portal_call(uint64_t,uint64_t *,uint64_t) noexcept { return false; }
#endif
namespace native_portal::live {
using namespace render;
namespace {
template<class T> bool get(uint64_t p,T &out) noexcept {return runtime::read_self(p,&out,sizeof out);}
uint64_t tid() noexcept {return uint64_t(syscall(SYS_gettid));}
bool same(const Identity &a,const Identity &b) noexcept {
    return a.process_instance==b.process_instance && a.renderer==b.renderer &&
        a.renderer_generation==b.renderer_generation && a.device==b.device && a.device_generation==b.device_generation;
}
bool rigid(const std::array<float,12>& m,ft::portal::Mat4 &out) noexcept {
    for(float v:m)if(!std::isfinite(v))return false;
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j) {
        double dot=0;for(unsigned k=0;k<3;++k)dot+=double(m[i*4+k])*m[j*4+k];
        if(std::abs(dot-(i==j?1.:0.))>0.0001)return false;
    }
    const double det=m[0]*(m[5]*m[10]-m[6]*m[9])-m[1]*(m[4]*m[10]-m[6]*m[8])+m[2]*(m[4]*m[9]-m[5]*m[8]);
    if(std::abs(det-1)>0.0001)return false;
    out={};for(unsigned i=0;i<12;++i)out.v[i]=m[i];out.v[15]=1;return true;
}
}
bool read_submit(const arena::Saved &r,uint64_t device,binding::SubmitSnapshot &out) noexcept {
    out={};SubmitInfo s{};TimelineSubmit t{};
    if(!device || !r.x[0] || r.x[1]!=1 || r.x[3] || !get(r.x[2],s) || s.type!=4 ||
       s.command_count!=1 || s.signal_count!=1 || s.wait_count>128 || !get(s.next,t) ||
       t.type!=1000207003 || t.next || t.signal_count!=1 || t.wait_count!=s.wait_count) return false;
    binding::SubmitSnapshot q{};
    if(!get(s.commands,q.command) || !get(s.signals,q.semaphore) || !get(t.signals,q.value))return false;
    // Validate all arrays, including stock waits. Never omit mixed native work.
    std::array<uint64_t,128> words{};std::array<uint32_t,128> stages{};
    if(s.wait_count && (!runtime::read_self(s.waits,words.data(),8*s.wait_count) ||
       !runtime::read_self(s.stages,stages.data(),4*s.wait_count) ||
       !runtime::read_self(t.waits,words.data(),8*s.wait_count)))return false;
    q.readable=q.supported_chain=true;q.submit_count=q.command_count=q.signal_count=q.timeline_signal_count=1;
    q.queue=r.x[0];q.device=device;out=q;return true;
}
struct Runtime::Impl {
    const pid_t pid=getpid();const uint64_t bootstrap_tid=tid();
    std::atomic<uint64_t> owner_tid{bootstrap_tid};uint64_t base=0;
    uint64_t worker=0,context=0,native_handle=0,wait_sp=0;
    bool shutdown_wait=false,join_attempt=false,join_ok=false;
    std::atomic<bool> runtime_owned{false},joined{false},handoff_wait{false};
    std::array<binding::Scope,2> encoded_scopes{};
    std::array<bool,2> encoded_returns{};
    arena::Saved native_queue_entry{};
    bool rejected_pair=false;
    unsigned backend_eye=0;
    uint64_t camera=0,layer_frame=0,layer_policy=0;
    std::atomic<uint64_t> external_camera{0};std::atomic<bool> external_pending{false};
    bool available=false,stock_active=false,native_portal_policy=false;
    bool transition_handled=false;uint64_t transition_result=0;
    bool layer_effective=false;
    bool external_call=false,policy_ready=false,claimed_layer=false;
    std::array<uint64_t,8> external_args{};
    uint64_t external_target=0;

    Backend backend;runtime::Evidence evidence;arena::Bridge bridge{evidence.bridge_memory()};
    std::unique_ptr<Server> server;
    std::optional<FrameGate> gate;std::optional<DemandAdapter> demand;
    std::optional<Adapter> adapter;std::optional<binding::Command> command;
    std::optional<binding::PassGate> pass;
    Binding binding{};binding::Owner command_owner{};
    std::array<Provenance,2> provenance{};
    PairPreparation prepared{};
    binding::Scope backend_scope{},queue_scope{},stamp_scope{},resource_scope{},wait_scope{};
    binding::Signal queue_signal{},wait_signal{};
    bool backend_admitted=false,backend_open=false,queue_open=false,stamp_open=false,resource_open=false,wait_open=false;
    uint64_t sequence=0,wrapper=0,marker=0,invocation=0,outer_sp=0,outer_owner=0,outer_lr=0;
    uint64_t origin_epoch=0,origin_token=0,frame_number=0;
    ft::portal::Mat4 origin{};Frame frame{};EyeInput eye{};
    PolicyInput policy{};bool tracking=false,outer=false,owned=false,pinned=false;
    std::array<unsigned char,0x660> effective{};
    std::array<uint64_t,14> policy_values{};uint64_t policy_seen=0;bool policy_conflict=false;
    std::atomic<bool> stopped{false};Diagnostics diag{};Mailbox<Diagnostics> published;
    LiveDiagnostics live{};Mailbox<LiveDiagnostics> live_published;
    CommandTrace command_trace;
    // Owner callbacks only write live. Foreign/handoff observations use atomics,
    // never a second writer to the owner's non-atomic snapshot.
    std::atomic<uint64_t> owner_events{0},first_owner_rejection{0},first_stop{0};
    void owner_rejected(uint64_t pc,uint16_t predicate) noexcept {
        uint64_t empty=0;first_owner_rejection.compare_exchange_strong(empty,(pc<<16)|predicate);
    }
    void stop_at(uint64_t pc,const arena::Saved &s) noexcept {
        // One atomic record even when bootstrap and Run race. No native reads.
        // At the shutdown store 1426d4, x20 is the authenticated context.
        const uint64_t flags=uint64_t(bool(context)) | uint64_t(context && s.x[0]==context)<<1 |
            uint64_t(context && s.x[20]==context)<<2 | uint64_t(context && s.x[21]==context)<<3 |
            uint64_t(tid()==bootstrap_tid)<<4 | uint64_t(handoff_wait.load())<<5 |
            uint64_t(runtime_owned.load())<<6;
        const uint64_t lr=s.x[30]>=base && s.x[30]-base<0x600000?s.x[30]-base:0;
        uint64_t empty=0;first_stop.compare_exchange_strong(empty,(lr<<32)|(flags<<24)|pc);stopped=true;
    }
    std::atomic<bool> collector_stop{false};std::thread collector;
    Impl(){published.publish(diag);live_published.publish(live);}
    ~Impl(){collector_stop=true;if(collector.joinable())collector.join();}
    void collect() {
        collector=std::thread([this] {
            DiagnosticLog log;char line[1536];
            size_t trace_next=0;uint64_t trace_lost=0,trace_last=UINT64_MAX;unsigned trace_summaries=0;
            // Reopen pipes/files with an independent nonblocking file description.
            // Journal stderr is a socket: send(MSG_DONTWAIT) leaves its flags alone.
            sigset_t blocked;sigemptyset(&blocked);sigaddset(&blocked,SIGPIPE);
            if(pthread_sigmask(SIG_BLOCK,&blocked,nullptr))return;
            const int fd=open("/proc/self/fd/2",O_WRONLY|O_APPEND|O_NONBLOCK|O_CLOEXEC);
            auto emit=[&](size_t n) {
                const auto written=fd>=0?write(fd,line,n):send(STDERR_FILENO,line,n,MSG_DONTWAIT|MSG_NOSIGNAL);
                return written==static_cast<ssize_t>(n);
            };
            for(;;) {
                LiveDiagnostics snapshot{};
                if(live_published.read(snapshot)) {
                    snapshot.first_owner_rejection=first_owner_rejection.load();
                    snapshot.first_evidence_pc=evidence.first_failure_pc();
                    const auto stop=first_stop.load();
                    snapshot.first_stop_pc=stop&0xffffff;snapshot.stop_flags=(stop>>24)&255;snapshot.stop_lr=stop>>32;
                    const size_t n=log.format(snapshot,owner_events.load(),line,sizeof line);
                    if(n) {
                        if(fd>=0)(void)write(fd,line,n);
                        else (void)send(STDERR_FILENO,line,n,MSG_DONTWAIT|MSG_NOSIGNAL);
                    }
                }
                CommandTrace::Row row{};
                // Keep the existing capability/refusal line ahead of a trace
                // burst, especially on small journal sockets. Preserve startup
                // slots until a seal, invalidation, or collector shutdown.
                const bool trace_flush=snapshot.seal || evidence.first_failure_pc() || collector_stop.load();
                while(trace_flush && command_trace.read(trace_next,row)) {
                    const auto n=CommandTrace::format(trace_next,row,line,sizeof line);
                    if(!n || !emit(n))++trace_lost;
                    ++trace_next;
                }
                const auto count=command_trace.count();
                if(trace_summaries<64 && (count!=trace_last || collector_stop.load())) {
                    const auto n=std::snprintf(line,sizeof line,
                        "ft-native-command-summary attempted=%llu consumed=%zu capacity=%zu overflow=%llu identity_overflow=%llu sink_lost=%llu\n",
                        (unsigned long long)count,trace_next,CommandTrace::capacity,
                        (unsigned long long)(count>CommandTrace::capacity?count-CommandTrace::capacity:0),
                        (unsigned long long)command_trace.identity_overflow(),(unsigned long long)trace_lost);
                    if(n>0 && size_t(n)<sizeof line)(void)emit(size_t(n));
                    trace_last=count;++trace_summaries;
                }
                if(collector_stop.load())break;
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
            if(fd>=0)close(fd);
        });
    }
    void refuse(Refusal why) noexcept {
        if(live.first_refusal==Refusal::None){live.first_refusal=why;live.first_pc=live.pc;}
        diag.refusal=why;diag.capable=false;
    }
    void publish() noexcept {
        if(adapter)diag.pending=adapter->state().pending;
        BackendStatus status{};if(backend.status(status))diag.submitted_frame=status.submitted_frame;
        diag.origin=origin_epoch;diag.stopped=stopped.load();published.publish(diag);
        live.events=diag.events;live.frames=diag.frames;live.refusal=diag.refusal;
        live.capable=diag.capable;live.pending=diag.pending;live.stopped=diag.stopped;
        live.initialized=evidence.initialized();live.publication=evidence.publication_diagnostic();
        for(unsigned i=0;i<2;++i){live.uploads[i]=diag.uploads[i];live.encoded[i]=diag.encoded[i];}
        live.queue_success=diag.queue_success;live.submitted_frame=diag.submitted_frame;
        live_published.publish(live);
    }
    bool call(Registers &r,uint64_t indirect=0) noexcept {
        if(!pinned || getpid()!=pid || tid()!=owner_tid || !r.target)return false;
        return ft_native_portal_call(r.target,r.x.data(),indirect);
    }
    static bool validate(void *u,const Binding &b,Phase phase) noexcept {
        auto &p=*static_cast<Impl*>(u);
        return phase!=Phase::Retire && p.evidence.initialized() && same(b.identity,p.evidence.identity()) && tid()==p.owner_tid;
    }
    static bool native(void *u,Registers &r) noexcept {
        auto &p=*static_cast<Impl*>(u);const bool create=r.target==p.base+0x296c00 || r.target==p.base+0x294358;
        if(create && !p.evidence.call_enter(r))return false;
        if(!p.call(r))return false;
        if(create && !p.evidence.call_return(r))return false;
        if(r.target==p.base+0x296c00)p.wrapper=r.x[0];
        if(r.target==p.base+0x294358)p.marker=r.x[0];
        return true;
    }
    static bool observe(void *u,const Binding &b,uint64_t w,uint64_t m,Observation &o) noexcept {
        return runtime::Evidence::observe(&static_cast<Impl*>(u)->evidence,b,w,m,o);
    }
    static bool stock(void *u,const Binding &b,uint64_t blend) noexcept {
        auto &p=*static_cast<Impl*>(u);std::array<uint32_t,8> words{};
        // Read only in this exact suspended camera enqueue invocation.
        return p.owned && p.invocation && same(b.identity,p.binding.identity) && blend &&
            runtime::read_self(blend+8,words.data(),sizeof words) && words==std::array<uint32_t,8>{1,1,7,0,1,7,0,15};
    }
    static bool pipeline(void *u,const Provenance &v,Registers &r) noexcept {
        auto &p=*static_cast<Impl*>(u);auto reader=p.evidence.pass_reader();
        Context context{v.identity,v.recording_generation};
        if(!reader.live || !reader.live(reader.user,context) || !ft_pipeline_begin || !ft_pipeline_end || !ft_pipeline_approved || v.eye>1)return false;
        const auto device=reinterpret_cast<VkDevice>(v.identity.device);
        const auto token=ft_pipeline_begin(device,v.identity.device_generation,v.vs,v.ps);if(!token)return false;
        bool ok=p.call(r);const bool ended=ft_pipeline_end(token)!=0;
        uint64_t handle=0;FtPipelineRecord record{};
        // Native pipeline object stores VkPipeline at +0x60, separately from layout +0x68.
        ok=ok && ended && r.x[0] && get(r.x[0]+0x60,handle) && handle &&
            ft_pipeline_approved(device,v.identity.device_generation,reinterpret_cast<VkPipeline>(handle),v.vs,v.ps,&record) &&
            record.abi==FT_PIPELINE_ABI && record.generation==v.identity.device_generation &&
            record.vertex_id==v.vs && record.fragment_id==v.ps && reader.live(reader.user,context);
        if(ok)p.provenance[v.eye]=v;
        return ok;
    }
    static bool submission(void *u,const Provenance &a,const Provenance &b,Completion &c) noexcept {
        auto &p=*static_cast<Impl*>(u);return p.command && p.command->submission(a,b,c);
    }
    static bool completed(void *u,const Binding &b,const Completion &c) noexcept {
        auto &p=*static_cast<Impl*>(u);return p.command && p.command->completed(b,c);
    }
    static bool settled(void *u,const Binding &b,const Provenance &a,const Provenance &c,const Completion &t) noexcept {
        return static_cast<Impl*>(u)->evidence.settled(b,a,c,t);
    }
    binding::Scope scope(const arena::Saved &r,uint64_t object) noexcept {
        if(sequence==UINT64_MAX)return {};
        return {command_owner,++sequence,owner_tid,r.sp,r.x[30],object};
    }
    static bool rejected(void *u,Address c,const Provenance &a,const Provenance &b) noexcept {
        auto &p=*static_cast<Impl*>(u);
        if(!p.backend_scope.sp || c!=p.command_owner.command || p.rejected_pair)return false;
        p.provenance={a,b};p.rejected_pair=true;
        return true; // bounded exact-skip ledger, consumed at this command's queue

    }
    bool origin_sample() noexcept {
        Registers r{};r.target=base+0x4b1a0;if(!call(r) || !uint32_t(r.x[0]))return false;
        const uint32_t token=uint32_t(r.x[0]);int32_t error=-1;
        const char version[]="IVRSystem_026";r={};r.target=base+0x4c450;
        r.x[0]=reinterpret_cast<uint64_t>(version);r.x[1]=reinterpret_cast<uint64_t>(&error);
        if(!call(r) || error || !r.x[0])return false;
        uint64_t vtable=0,target=0;if(!get(r.x[0],vtable) || !get(vtable+14*8,target) || !target)return false;
        std::array<float,12> matrix{};r.target=target;
        if(!call(r,reinterpret_cast<uint64_t>(matrix.data())))return false;
        ft::portal::Mat4 next{};if(!rigid(matrix,next))return false;
        r={};r.target=base+0x4b1a0;if(!call(r) || uint32_t(r.x[0])!=token)return false;
        if(!origin_epoch || origin.v!=next.v || origin_token!=token) {
            if(origin_epoch==UINT64_MAX)return false;
            ++origin_epoch;origin=next;origin_token=token;
            backend.invalidate(Reason::Origin);
        }
        return true;
    }
    void policy_tap(uint64_t pc,const arena::Saved &s) noexcept {
        auto add=[&](unsigned i,uint64_t v){const auto bit=uint64_t(1)<<i;if((policy_seen&bit)&&policy_values[i]!=v)policy_conflict=true;policy_seen|=bit;policy_values[i]=v;};
        uint32_t scalar=0;std::memcpy(&scalar,s.q[0],4);
        switch(pc) {
        case 0x112ab4:add(0,uint32_t(s.x[3]));break;
        case 0x112aec:add(1,uint32_t(s.x[1]));add(2,s.x[2]);break;
        case 0x112af4:add(3,scalar);break;
        case 0x112afc:add(4,0);break;case 0x112c70:add(4,1);break;
        case 0x112c10:add(5,uint32_t(s.x[1]));break;case 0x112c78:add(6,uint32_t(s.x[0]));break;
        case 0x112ac4:add(7,uint32_t(s.x[0]));break;case 0x112b10:add(8,uint32_t(s.x[0]));break;
        case 0x112c50:add(9,uint32_t(s.x[1]));break;case 0x112d14:add(10,uint32_t(s.x[23]));break;
        case 0x112cfc:add(11,uint32_t(s.x[0]));break;case 0x112d2c:add(12,scalar);break;
        case 0x112d90:{uint32_t v=0;std::memcpy(&v,s.q[1],4);add(13,v);break;}
        default:break;
        }
    }
    bool attach() noexcept {
        const auto id=evidence.identity();diag.initialized=evidence.initialized();diag.renderer=id.renderer;
        if(!id.device_generation || !ft_pipeline_begin || !ft_pipeline_end || !ft_pipeline_approved) {refuse(Refusal::PipelineAbi);return false;}
        // Initial attachment is real. Whole-dispatch ownership is an independent
        // obligation: no capability may be published simply because memory reads.
        if(!evidence.graph_capable()) {refuse(Refusal::GraphOwnership);return false;}
        // Native ownership alone does not authorize demand. Publish initial
        // capability only after the cold origin and real camera-idle checks.
        if(!origin_epoch) {refuse(Refusal::Origin);return false;}
        if(!gate)gate.emplace(backend);
        if(!demand)demand.emplace(&backend);
        binding.identity=id;binding.upstream=&bridge;
        binding.targets={base+0x296c00,base+0x2993f8,base+0x2988a8,base+0x29b278,base+0x2966a0,base+0x294358};
        Callbacks cb{};cb.clock=boottime;cb.user=this;cb.validate=validate;cb.call=native;cb.observe=observe;cb.stock_blend=stock;
        cb.pipeline=pipeline;cb.submission=submission;cb.completed=completed;cb.settled=settled;cb.rejected=rejected;
        adapter.emplace(backend,*gate,binding,cb,2);
        if(!adapter->initialize()){refuse(Refusal::Adapter);return false;}
        runtime_owned=true;diag.capable=true;diag.refusal=Refusal::None;return true;
    }
    bool in_native_demand=false;
    static bool acquire(void *u) noexcept {return static_cast<Impl*>(u)->camera_transition(true);}
    static bool release(void *u) noexcept {return static_cast<Impl*>(u)->camera_transition(false);}
    bool camera_transition(bool acquiring) noexcept {
        Registers r{};r.target=base+(acquiring?0x15f9e0:0x15fba8);
        r.x={camera,2,0,0,0,0,0,0};
        if(external_call) {r.target=external_target;r.x=external_args;}
        in_native_demand=true;const bool called=call(r);in_native_demand=false;
        transition_result=r.x[0];
        uint8_t active=1,state=1;
        if(!called || !get(camera+0x34a,active) || !get(camera+0x360,state))return false;
        return acquiring?(uint8_t(r.x[0])!=0 && active!=0):(active==0 && state==0);
    }
    bool signal(uint64_t resource,binding::Signal &s) noexcept {
        s={};return get(resource+0x10,s.synchronization) && get(resource+0x18,s.serial) && get(resource+0x20,s.subserial);
    }
    bool token(uint64_t synchronization,uint64_t pointer,binding::Signal &s) noexcept {
        s={};s.synchronization=synchronization;return get(pointer,s.serial) && get(pointer+8,s.subserial);
    }
    void queue_event(uint64_t pc,const arena::Saved &s) noexcept {
        if(pc==0x3021d4 && adapter && adapter->state().pending && !command && native_queue_entry.x[1] &&
           (rejected_pair || (encoded_returns[0] && encoded_returns[1]))) {
            binding::SubmitSnapshot submit{};binding::Owner owner{};uint64_t signals=0,signals_end=0,token_pointer=0;
            if(!evidence.command_owner(owner) || native_queue_entry.x[1]!=owner.command || s.x[19]!=owner.command ||
               !read_submit(s,owner.identity.device,submit) || submit.command!=owner.vk_command ||
               !get(owner.command+0x428,signals) || !get(owner.command+0x430,signals_end) || signals_end!=signals+8 ||
               !get(signals,owner.synchronization) || !get(owner.synchronization+0x58,owner.semaphore) ||
               owner.semaphore!=submit.semaphore || !get(owner.command+0x4f0,token_pointer) ||
               !token(owner.synchronization,token_pointer,queue_signal)) {refuse(Refusal::Command);return;}
            owner.queue=submit.queue;command_owner=owner;command.emplace();
            if(!command->bind(owner)) {refuse(Refusal::Command);return;}
            bool registered=true;
            if(rejected_pair) {
                auto entry=backend_scope;entry.owner=owner;entry.object=owner.command;
                registered=command->rejected(entry,owner.command,provenance[0],provenance[1]);
            } else for(unsigned i=0;i<2;++i) {
                auto entry=encoded_scopes[i];entry.owner=owner;
                registered=registered && encoded_returns[i] && command->backend_enter(entry,provenance[i]) && command->backend_return(entry,true);
            }
            if(!registered || !evidence.bind_lifecycle(*command,owner) || !evidence.event(0x3018b0,native_queue_entry)) {refuse(Refusal::Command);return;}
        }
        if(!command)return;
        switch(pc) {
        case 0x3021d4: {
            if(s.x[19]!=command_owner.command)return;
            binding::SubmitSnapshot submit{};
            if(!read_submit(s,command_owner.identity.device,submit) || !queue_signal.synchronization) {refuse(Refusal::Command);return;}
            queue_scope=scope(s,command_owner.command);queue_open=true;
            if(!command->queue_enter(queue_scope,submit,queue_signal))refuse(Refusal::Command);
            break;
        }
        case 0x3021d8:
            if(!queue_open || s.sp!=queue_scope.sp || s.x[19]!=command_owner.command)return;
            if(uint32_t(s.x[0])==0)++diag.queue_success;
            if(!command->queue_result(queue_scope,int32_t(s.x[0])))refuse(Refusal::Command);
            break;
        case 0x302208: {
            if(!queue_open || s.x[0]!=command_owner.command)return;
            binding::Signal actual{};if(!token(s.x[1],s.x[2],actual)){refuse(Refusal::NativeRead);return;}
            stamp_scope=scope(s,s.x[0]);stamp_open=true;
            if(!command->stamp_enter(queue_scope,stamp_scope,actual))refuse(Refusal::Command);
            break;
        }
        case 0x310ea0:
            if(!stamp_open || (s.x[0]!=command_owner.command && s.x[0]!=command_owner.arena))return;
            {binding::Signal actual{};if(!token(s.x[1],s.x[2],actual))return;
             resource_scope=scope(s,s.x[0]);resource_open=true;
             if(!command->resource_enter(stamp_scope,resource_scope,actual))refuse(Refusal::Command);}
            break;
        case 0x310f20:
            if(!resource_open || s.x[19]!=resource_scope.object || s.sp+0x30!=resource_scope.sp)return;
            {binding::Signal actual{};if(!signal(s.x[19],actual) || !command->resource_post(resource_scope,s.x[19],actual))refuse(Refusal::Command);resource_open=false;}
            break;
        case 0x30220c:
            if(!stamp_open || s.x[19]!=command_owner.command || s.sp!=stamp_scope.sp)return;
            {binding::Signal actual{};if(!signal(s.x[19],actual) || !command->stamp_post(stamp_scope,s.x[19],actual))refuse(Refusal::Command);
             stamp_open=queue_open=false;if(adapter)adapter->submitted();}
            break;
        case 0x3160b8:
            if(s.x[0]!=command_owner.synchronization)return;
            {uint64_t semaphore=0;if(!token(s.x[0],s.x[1],wait_signal) || !get(s.x[0]+0x58,semaphore))return;
             wait_scope=scope(s,s.x[0]);wait_open=true;if(!command->wait_enter(wait_scope,wait_signal,semaphore))refuse(Refusal::Command);}
            break;
        case 0x316110:
            if(wait_open && s.sp+0x40==wait_scope.sp && !command->wait_result(wait_scope,wait_signal,int32_t(s.x[0])))refuse(Refusal::Command);
            break;
        case 0x316120:case 0x316150:
            if(!wait_open || s.sp!=wait_scope.sp || s.x[30]!=wait_scope.lr)return;
            if(!command->wait_return(wait_scope,wait_signal,uint32_t(s.x[0])))refuse(Refusal::Command);
            wait_open=false;if(adapter)adapter->poll_completion();break;
        default:break;
        }
    }
    void event(uint64_t pc,arena::Saved &s) noexcept {
        if(!pinned || getpid()!=pid)return;
        // Shutdown guard is owned by the original initializer thread. It pairs
        // the real timed wait and actual pthread_join result before cleanup.
        const bool shutdown_store=pc==0x1426d4 && tid()==bootstrap_tid && context &&
            s.x[20]==context && uint32_t(s.x[1])==1;
        if(tid()==bootstrap_tid) {
            if(shutdown_store) {stop_at(pc,s);backend.invalidate(Reason::Off);}
            if(pc==0x142704 && worker && s.x[0]==worker && s.x[20]==context) {
                stop_at(pc,s);backend.invalidate(Reason::Off);wait_sp=s.sp;shutdown_wait=true;join_attempt=join_ok=false;
                get(worker+8,native_handle);
            }
            if(pc==0x183f78 && shutdown_wait && s.x[20]==worker && s.sp+0x20==wait_sp && s.x[0]==native_handle && native_handle)join_attempt=true;
            if(pc==0x183f7c && join_attempt && s.x[20]==worker && s.sp+0x20==wait_sp)join_ok=uint32_t(s.x[0])==0;
            if(pc==0x142708 && shutdown_wait && s.sp==wait_sp) {
                joined=join_ok && uint32_t(s.x[0])==1;shutdown_wait=false;
            }
            // Explicit bounded-trial fail-stop, not a fabricated join, silent
            // queue cancellation or a speculative blocking wait inside Run locks.
            if(pc==0x1381b8 && runtime_owned.load() && !joined.load())std::abort();
        }
        if(pc==0x14099c || pc==0x140dbc || pc==0x14e5a8) {
            command_trace.push(pc,s,base,tid(),owner_tid.load(),CommandTrace::OwnerEvent);
            const unsigned owner_bit=pc==0x14099c?0:pc==0x140dbc?2:4;
            owner_events.fetch_or(1ull<<owner_bit);
            if(pc!=0x14e5a8 && (handoff_wait || owner_tid!=bootstrap_tid)) {owner_rejected(pc,253);return;}
            uint16_t predicate=0;
            if(evidence.owner_event(pc,s,&predicate)) {
                owner_events.fetch_or(1ull<<(owner_bit+1));
                if(pc==0x14099c){worker=s.x[19];context=s.x[21];}
                if(pc==0x140dbc)handoff_wait=true;
                if(pc==0x14e5a8){owner_tid=tid();handoff_wait=false;}
            } else owner_rejected(pc,predicate);
            return;
        }
        if(handoff_wait && tid()==bootstrap_tid) {
            command_trace.push(pc,s,base,tid(),owner_tid.load(),CommandTrace::HandoffWait);return;
        }
        if(tid()!=owner_tid) {
            command_trace.push(pc,s,base,tid(),owner_tid.load(),CommandTrace::Foreign);
            // Native external methods run on RPC/main threads too. They retain
            // the exact original continuation and result. Revoke immediately;
            // hand debt ownership off on the owner, never replay the call.
            if((pc==0x15f9e0 || pc==0x15fba8) && s.x[0] && s.x[0]==external_camera.load()) {
                backend.invalidate(Reason::External);external_pending=true;
            }
            if(pc==0x112e50 && uint8_t(s.x[1]))backend.invalidate(Reason::External);
            return;
        }
        if(external_pending.exchange(false) && demand) {demand->external_observed();owned=false;}
        ++diag.events;live.pc=pc;
        constexpr uint64_t phase_pcs[]={0x7fda0,0x7fdbc,0x147a48,0x112b58,0x63920,0x1480b8,
            0x161d64,0x161d68,0x161ed8,0x2a1e18,0x29f1e8,0x3021d4,0x3021d8,0x316120,0x300a38};
        for(unsigned i=0;i<sizeof(phase_pcs)/sizeof(*phase_pcs);++i)if(pc==phase_pcs[i])live.phases|=1ull<<i;
        // Native queue instructions always run, including mixed stock submissions.
        queue_event(pc,s);
        if(pc==0x7fda0 || pc==0x29a14c || pc==0x29a150 || pc==0x7fdbc) {
            if(!evidence.route(bridge,pc,s))refuse(Refusal::NativeInitialization);
            if(pc==0x7fdbc && evidence.initialized())attach();
            publish();return;
        }
        const bool graph_boundary=pc==0x2a1e18 || pc==0x2a2230 || pc==0x2a2320 || pc==0x2987d8 || pc==0x298840;
        if(graph_boundary)evidence.graph_event(pc,s);
        evidence.event(pc,s);
        if(runtime::command_trace_pc(pc)) {
            const auto diagnostic=evidence.command_diagnostic();
            command_trace.push(pc,s,base,tid(),owner_tid.load(),CommandTrace::Owner,&diagnostic);
        }
        if(graph_boundary && pc!=0x2a1e18)evidence.graph_event(0,s);
        if(pc==0x3018b0 && adapter)native_queue_entry=s;
        unsigned arena_site=arena::SiteCount;
        if(pc==0x299110)arena_site=arena::Admission;
        if(pc==0x2988b0)arena_site=arena::UboClassify;
        if(pc==0x29f358)arena_site=arena::GeometryClassify;
        if(pc==0x2987d4)arena_site=arena::Reset;
        if(arena_site<arena::SiteCount) {
            evidence.arena_event(bridge,arena_site,s);
        }
        if(shutdown_store || pc==0x29bd18 || pc==0x29c238) {
            stop_at(pc,s);backend.invalidate(Reason::Off);if(adapter)adapter->off();refuse(Refusal::Stopped);
        }
        if(pc==0x112e50 && uint8_t(s.x[1])) {
            backend.invalidate(Reason::External);owned=false;
        }
        if((pc==0x15f9e0 || pc==0x15fba8) && demand && s.x[0]==camera && !in_native_demand) {
            // Raw methods include all native callers, not merely the public
            // RoomView wrappers. Reenter the original through one guarded call,
            // then the site-specific FUNCTION-RETURN island restores native LR.
            external_call=true;external_target=base+pc;
            for(unsigned i=0;i<8;++i)external_args[i]=s.x[i];
            const bool ok=demand->external(pc==0x15f9e0,pc==0x15f9e0?acquire:release,this);
            external_call=false;owned=false;s.x[0]=transition_result;s.pad=arena::SkipOwned;
            if(!ok)refuse(Refusal::Policy);
        }
        if(pc==0x9ee5c && outer)available=uint8_t(s.x[0])!=0;
        if(pc==0x9ee98 && outer && demand && (owned || demand->owned_debt())) {
            uint8_t f5d=0,f5a=0;
            const bool read=get(layer_frame+0xd5d,f5d) && get(layer_frame+0xd5a,f5a);
            const bool stock_activity=read && activity(available,f5d,frame.policy.stock.construct,f5a,frame.policy.stock.alpha);
            const bool effective_activity=read && activity(available,f5d,frame.policy.effective.construct,f5a,frame.policy.effective.alpha);
            if(!read || uint32_t(s.x[27])!=unsigned(effective_activity)) {
                backend.invalidate(Reason::NativeFault);owned=false;refuse(Refusal::Policy);
            } else {
                transition_handled=true;
                if(!demand->layer(stock_activity,owned && effective_activity,acquire,release,this)) {
                    owned=false;refuse(Refusal::Policy);
                }
                layer_effective=effective_activity;stock_active=stock_activity;
            }
        }
        if((pc==0x9eeb0 || pc==0xa050c) && transition_handled) {
            // Both are BL sites; their own call-replacement islands skip only BL.
            // x0 is the actual native result if forwarded, never success invented.
            s.x[0]=transition_result;s.x[30]=base+pc+4;s.pad=arena::SkipOwned;
        }
        if(pc==0x9f404 && owned) {
            const bool expected=background(layer_effective,native_portal_policy,frame.policy.effective.alpha);
            if(bool(s.x[23])!=expected) {backend.invalidate(Reason::NativeFault);owned=false;refuse(Refusal::Policy);}
            else if(background_permission(bool(s.x[23]),true,stock_active,frame.policy.stock.alpha,
                                          frame.policy.effective.alpha,native_portal_policy,layer_effective))s.x[23]=1;
        }
        if(pc==0x147a48) {
            if(outer || frame_number==UINT64_MAX) {refuse(Refusal::Policy);return;}
            outer=true;outer_sp=s.sp;outer_owner=s.x[0];owned=false;tracking=false;policy_seen=0;policy_conflict=false;policy_ready=false;
            outer_lr=s.x[30]>=base && s.x[30]-base<0x600000?s.x[30]-base:0;
            ++frame_number;++diag.frames;
        }
        if(outer)policy_tap(pc,s);
        if(outer && pc==0x112b58) {
            policy={};policy.style=uint32_t(policy_values[0]);policy.enabled=policy_values[1]==1;
            policy.special=policy_values[4]!=0;policy.request=policy_values[5]!=0;policy.construct=policy_values[6]!=0;
            bool forced_seen=false;uint64_t forced=0;
            for(unsigned k=7;k<=10;++k)if(policy_seen&(uint64_t(1)<<k)) {
                if(forced_seen && forced!=policy_values[k])policy_conflict=true;
                forced_seen=true;forced=policy_values[k];
            }
            policy.forced=forced!=0;policy.suppression=policy_values[11]!=0;
            uint32_t bits=uint32_t(policy_values[12]);std::memcpy(&policy.base,&bits,4);
            bits=uint32_t(policy_values[13]);std::memcpy(&policy.gain,&bits,4);
            uint8_t p0=0,p1=0,construct=0;uint32_t style=0;float alpha=0;
            const bool read=get(s.x[19]+0x1e0,p0) && get(s.x[19]+0x1e1,p1) &&
                get(s.x[19]+0x62c,construct) && get(s.x[19]+0x630,style) && get(s.x[19]+0x648,alpha);
            policy.p0=p0!=0;policy.p1=p1!=0;const auto expected=pure_policy(policy,false);
            const uint64_t required=(1ull<<0)|(1ull<<1)|(1ull<<4)|(1ull<<5);
            // Unsupported branches remain entirely stock. These fields are real
            // load results and the original function's unmodified outputs.
            policy_ready=read && !policy_conflict && forced_seen && (policy_seen&required)==required &&
                !policy.special && !policy.request && !policy.forced && p0<=1 && p1<=1 &&
                expected.construct==bool(construct) && expected.demand==bool(s.x[23]) &&
                expected.style==style && expected.alpha==alpha;
        }
        if(outer && pc==0x63920) {
            struct Pose {unsigned char transform[72];uint32_t result;uint8_t valid,connected;uint8_t padding[2];} pose{};
            tracking=get(s.x[3],pose) && pose.result==200 && pose.valid==1 && pose.connected==1;
        }
        if(outer && pc==0x1480b8) {
            // Origin sampling uses existing OpenVR initialization and x8 return.
            // It does not VR_Init, set camera state, or establish graph authority.
            const bool origin_ok=origin_sample();
            if(!origin_ok) {backend.invalidate(Reason::Origin);refuse(Refusal::Origin);}
            if(!tracking)refuse(Refusal::Tracking);
            uint64_t native_context=0,application=0;uint8_t cache=1;
            // Record only the existing short-circuit reads. Failed checks do
            // not inspect any downstream pointer or change camera admission.
            live.assoc_checked=live.assoc_passed=live.assoc_failed=0;
            live.assoc_owner=outer_owner==worker;live.assoc_lr=outer_lr;live.assoc_frame=frame_number;
            auto association_check=[&](unsigned id,bool ok) {
                live.assoc_checked|=uint64_t(1)<<id;
                if(ok)live.assoc_passed|=uint64_t(1)<<id;
                else if(!live.assoc_failed)live.assoc_failed=id;
                return ok;
            };
            const bool associated=association_check(1,get(outer_owner+0x48,native_context)) &&
                association_check(2,native_context==context) && association_check(3,get(context+0x1a8,application)) &&
                association_check(4,application!=0) && association_check(5,get(application+0x25320,camera)) &&
                association_check(6,camera!=0) && association_check(7,get(application+0x2568c,cache));
            if(associated)external_camera=camera;
            std::array<unsigned char,40> camera_state{};
            bool idle=associated && cache==0 && runtime::read_self(camera+0x340,camera_state.data(),camera_state.size());
            for(unsigned k:{9u,10u,32u})idle=idle && camera_state[k]==0;
            uint64_t h0=0,h1=0;std::memcpy(&h0,camera_state.data()+16,8);std::memcpy(&h1,camera_state.data()+24,8);
            idle=idle && !h0 && !h1;
            if(!demand && evidence.graph_capable())demand.emplace(&backend);
            if(demand && idle && !backend.render_pending())demand->establish(true,true);
            BackendStatus status{};backend.status(status);
            status.origin=origin_epoch;status.serialized=evidence.graph_capable();
            const bool pipeline_abi=ft_pipeline_begin && ft_pipeline_end && ft_pipeline_approved && evidence.identity().device_generation;
            status.capable=status.serialized && pipeline_abi && origin_ok && origin_epoch && demand && (demand->can_acquire() || demand->owned_debt());
            if(!pipeline_abi)refuse(Refusal::PipelineAbi);
            else if(!associated)refuse(Refusal::NativeRead);
            status.external_clear=policy_ready && !policy.request && !policy.forced && !policy.special;
            if(stopped)status.capable=false;
            backend.publish(status);
            if(!adapter && status.capable)attach();
            if(!adapter || !adapter->state().initialized) {status.capable=false;backend.publish(status);}
            diag.capable=status.capable;
            // Capture exactly the existing seal predicates, after adapter attachment.
            // This is evidence only: no new admission, native read or gate changes.
            live.seal=true;++live.seals;live.graph=status.serialized;live.origin_ok=origin_ok && origin_epoch;
            live.associated=associated;live.idle=idle;live.demand=bool(demand);
            live.can_acquire=demand && demand->can_acquire();live.owned_debt=demand && demand->owned_debt();
            live.pipeline=pipeline_abi;live.adapter=adapter && adapter->state().initialized;
            live.tracking=tracking;live.policy=policy_ready;
            if(adapter && adapter->state().initialized && policy_ready && associated && tracking && !stopped &&
               runtime::read_self(s.sp+0x610,effective.data(),effective.size())) {
                if(!gate)gate.emplace(backend);
                frame=gate->begin(frame_number,policy,true,true,false,boottime());
                owned=frame.policy.portal && demand->bind({frame.lease.epoch,frame.lease.origin,frame.id}) && adapter->latch(frame);
                if(owned) {
                    diag.capable=true;diag.refusal=Refusal::None;
                    if(!evidence.command_owner(command_owner)) {owned=false;adapter->off();refuse(Refusal::Command);}
                    else diag.recording=command_owner.recording;
                    const uint8_t construct=frame.policy.effective.construct;
                    std::memcpy(effective.data()+0x62c,&construct,1);
                    std::memcpy(effective.data()+0x630,&frame.policy.effective.style,4);
                    std::memcpy(effective.data()+0x648,&frame.policy.effective.alpha,4);
                    if(!evidence.release_lifecycle()) {owned=false;adapter->off();refuse(Refusal::Command);}
                    else {encoded_returns={};encoded_scopes={};provenance={};rejected_pair=false;command.reset();}
                }
            }
        }
        if(outer && (pc==0x148244 || pc==0x1482a8 || pc==0x14834c || pc==0x14840c)) {
            eye={};eye.eye=unsigned(s.x[1]);eye.observed_origin=origin_epoch;eye.raw_to_standing=origin;
            layer_frame=s.x[2];layer_policy=s.x[3];available=false;transition_handled=false;
            uint8_t f5d=0,f5a=0,np=0;get(layer_frame+0xd5d,f5d);get(layer_frame+0xd5a,f5a);get(layer_policy+0x62e,np);
            native_portal_policy=np!=0;stock_active=f5d || (f5a && frame.policy.stock.alpha>0);
            if(owned && stock_active){owned=false;adapter->off();}
            // Stock native calls are unchanged until the complete frame-policy
            // and demand transition has been admitted, not just a lease bit.
            claimed_layer=owned;
            if(claimed_layer)s.x[3]=reinterpret_cast<uint64_t>(effective.data());
        }
        if(claimed_layer && adapter && (pc==0x161d64 || pc==0x161d68 || pc==0x161ed8 || pc==0x161f00)) {
            std::array<float,12> actual{};ft::portal::Mat4 standing{};
            if(!runtime::read_self(s.sp+0x170,actual.data(),48) || !rigid(actual,standing) || standing.v!=origin.v) {
                adapter->off();refuse(Refusal::Origin);
                // Site-specific camera BLR islands have a SkipOwned route.
                if(pc==0x161d64 || pc==0x161ed8) {s.pad=arena::SkipOwned;s.x[30]=base+pc+4;}
            } else {
                const auto ticket=evidence.stage(*adapter,pc,s,eye,pc==0x161d64?0:invocation);
                if(pc==0x161d64)invocation=ticket;
                if(pc==0x161d68 && ticket && eye.eye<2)++diag.uploads[eye.eye];
                if(pc==0x161ed8 || pc==0x161f00)invocation=0;
                if(!ticket)refuse(Refusal::Adapter);
            }
        }
        if(pc==0x148de0 && outer && s.sp==outer_sp) {if(adapter && frame.id==frame_number)adapter->outer_return();outer=false;owned=false;claimed_layer=false;}
        if(adapter && pc==0x2a1e18 && s.x[0]==binding.identity.renderer && !uint32_t(s.x[1])) {
            Observation observation{};
            if(evidence.snapshot(wrapper,marker,observation)) {
                binding::PassRequest request{{binding.identity,observation.arena.recording_generation},marker,base,0};
                auto reader=evidence.pass_reader();prepared=binding::extract_passes(reader,request).pair;
                pass.emplace(*adapter,reader,request);backend_scope=scope(s,observation.arena.command);
                binding::PassRegisters regs{};std::memcpy(&regs,&s,sizeof s);pass->callback(regs,0);std::memcpy(&s,&regs,sizeof s);
                if(adapter->state().pending && !adapter->state().committed)refuse(Refusal::Adapter);
            }
        }
        if(pass && pc==0x2a1c18) {
            backend_scope=scope(s,command_owner.command);
            binding::PassRegisters regs{};std::memcpy(&regs,&s,sizeof s);pass->callback(regs,1);std::memcpy(&s,&regs,sizeof s);
        }
        if(adapter && pc==0x2a1c18 && s.x[21]==binding.identity.renderer) {
            uint64_t actual_marker=0;
            // Classification-only borrow from THIS suspended packet callback.
            // No labels, descriptors, pipeline authority or generic graph reads
            // are granted when whole-dispatch borrowing was refused.
            if(s.x[19] && s.x[19]<=UINT64_MAX-16 && get(s.x[19]+8,actual_marker) && actual_marker==marker) {
                const auto reader=evidence.pass_reader();
                const bool graph_live=reader.live && reader.live(reader.user,{binding.identity,command_owner.recording});
                if(!graph_live || !adapter->state().committed) {
                    if(!s.pad)adapter->suppressed({},0); // no fabricated disposal certificate
                    s.pad=1; // exact existing PASS packet-skip route, not arena SkipOwned
                    if(!graph_live)refuse(Refusal::GraphOwnership);
                }
            }
        }
        if(adapter && pc==0x29f1e8) {
            for(unsigned i=0;i<2;++i)if(prepared.eyes[i].lookup.x[1]==s.x[1]) {
                const auto &e=prepared.eyes[i];Registers lookup=e.lookup;
                for(unsigned k=0;k<4;++k)lookup.x[k]=s.x[k];
                backend_scope=scope(s,s.x[0]);backend_open=true;
                uint64_t ticket=0;decode_ticket(e.record.label,e.record.label_size,ticket);
                unsigned eye_index=provenance[0].ticket==ticket?0:1;
                backend_eye=eye_index;
                auto result=adapter->backend(e.record,e.context,lookup);backend_admitted=result.decision==Decision::Portal;
                if(backend_admitted) {++diag.encoded[eye_index];encoded_scopes[eye_index]=backend_scope;}
                // The pipeline-result island receives the cached native object.
                backend_pipeline=result.decision==Decision::Portal?result.pipeline:0;
                break;
            }
        }
        if(pc==0x29f200 && backend_open) {
            s.x[0]=backend_pipeline;s.x[30]=base+0x29f204;s.pad=arena::SkipOwned;
        }
        if((pc==0x29f518 || pc==0x29f550) && backend_open && s.sp==backend_scope.sp && s.x[30]==backend_scope.lr) {
            encoded_returns[backend_eye]=backend_admitted;
            backend_open=false;
        }
        if(adapter && (pc==0x300c54 || pc==0x300a38)) {
            adapter->reclaim();
            if(command && !adapter->state().pending && evidence.release_lifecycle())command.reset();
        }
        publish();
    }
    uint64_t backend_pipeline=0;
};
Runtime::Runtime():p_(new Impl){}
Runtime::~Runtime()=default;
bool Runtime::pin(uint64_t b,const char *sha) noexcept {
    if(!p_->evidence.pin(b,sha))return false;
    p_->base=b;p_->pinned=true;return true;
}
bool Runtime::start_server(const char *path) {
    if(!p_->pinned || p_->server || !path || path[0]!='/')return false;
    p_->server=std::make_unique<Server>(path,p_->backend);
    if(!p_->server->start()){p_->server.reset();return false;}
    // Optional evidence must not become another runtime-start gate.
    try {p_->collect();} catch(...) {}
    return true;
}
void Runtime::event(uint64_t pc,arena::Saved &r) noexcept {p_->event(pc,r);}
Diagnostics Runtime::diagnostics() const noexcept {Diagnostics d{};p_->published.read(d);return d;}
void Runtime::stop() noexcept {p_->stopped=true;p_->backend.invalidate(Reason::Off);}
}
namespace {std::atomic<native_portal::live::Runtime*> live_runtime{nullptr};}
extern "C" bool ft_native_portal_start(uint64_t base,const char *sha,const char *directory) noexcept {
    if(live_runtime.load())return false;
    try {
        auto runtime=std::make_unique<native_portal::live::Runtime>();
        if(!runtime->pin(base,sha) || !runtime->start_server(directory))return false;
        auto *expected=static_cast<native_portal::live::Runtime*>(nullptr);
        if(!live_runtime.compare_exchange_strong(expected,runtime.get()))return false;
        runtime.release();return true; // retained through process exit, never free pending native debt
    } catch(...) {return false;}
}
extern "C" void ft_native_portal_event(uint64_t pc,native_portal::arena::Saved *saved) noexcept {
    auto *runtime=live_runtime.load();if(runtime && saved)runtime->event(pc,*saved);
}
extern "C" bool ft_native_portal_diagnostics(native_portal::live::Diagnostics *out,size_t size) noexcept {
    auto *runtime=live_runtime.load();if(!runtime || !out || size!=sizeof(*out))return false;
    *out=runtime->diagnostics();return true;
}
extern "C" void ft_native_portal_stop() noexcept {auto *runtime=live_runtime.load();if(runtime)runtime->stop();}
