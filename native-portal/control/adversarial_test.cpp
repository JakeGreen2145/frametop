#include "control.hpp"
#include "policy.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <thread>
#include <chrono>
#include <spawn.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
using namespace native_portal;
static std::atomic<uint64_t> time_ns{1000000000};
static uint64_t clock_now(){return time_ns;}
template<class F> static void until(F f) { for(int i=0;i<500;++i) { if(f())return; usleep(2000); } assert(false&&"timeout"); }
static int raw_connect(const std::string& path) {
    int fd=socket(AF_UNIX,SOCK_SEQPACKET,0); assert(fd>=0); sockaddr_un a{}; a.sun_family=AF_UNIX; assert(path.size()<sizeof a.sun_path); strcpy(a.sun_path,path.c_str());
    assert(!connect(fd,(sockaddr*)&a,sizeof a)); return fd;
}
static Reply raw_exchange(int fd,const Request& q) { auto w=encode(q); assert(send(fd,w.data(),128,MSG_NOSIGNAL)==128); assert(recv(fd,w.data(),128,0)==128); Reply r; assert(decode(w.data(),128,r)); return r; }
static int child(const char* path,int channel) {
    int fd=raw_connect(path); Request q; q.sequence=1; q.sent_ns=time_ns; auto r=raw_exchange(fd,q); assert(r.epoch);
    q.op=Operation::Update; q.epoch=r.epoch; q.origin=r.origin; ++q.sequence; q.generation=1; r=raw_exchange(fd,q); assert(r.state==State::Warming);
    // Keep the socket alive in the parent after this authenticated owner dies.
    char byte='x'; iovec io{&byte,1}; alignas(cmsghdr) char ancillary[CMSG_SPACE(sizeof fd)]{}; msghdr m{};
    m.msg_iov=&io; m.msg_iovlen=1; m.msg_control=ancillary; m.msg_controllen=sizeof ancillary;
    auto h=CMSG_FIRSTHDR(&m); h->cmsg_level=SOL_SOCKET; h->cmsg_type=SCM_RIGHTS; h->cmsg_len=CMSG_LEN(sizeof fd); memcpy(CMSG_DATA(h),&fd,sizeof fd);
    assert(sendmsg(channel,&m,0)==1); char done; assert(read(channel,&done,1)==1); return 0;
}
static bool real_result=false; static unsigned native_calls=0;
static bool call(void*) { ++native_calls; return real_result; }
int main(int argc,char** argv) {
    if(argc==4&&!strcmp(argv[1],"--child"))return child(argv[2],atoi(argv[3]));
    Geometry g; g.quaternion[0]=std::numeric_limits<float>::quiet_NaN(); assert(!valid(g));
    Request q; q.sequence=1; q.sent_ns=time_ns; auto bytes=encode(q); Request decoded;
    for(int i:{0,8,12,52,108,120,127}) { auto bad=bytes; bad[i]^=0x80; assert(!decode(bad.data(),128,decoded)); }
    assert(!decode(bytes.data(),129,decoded)); assert(!deadline(1,1000000000,250));
    assert(deadline(time_ns,time_ns,1)==time_ns+1000000); assert(!deadline(time_ns,time_ns,0));
    for(int a=0;a<2;++a)for(int b=0;b<2;++b)for(int c=0;c<2;++c)for(int d=0;d<2;++d)for(float alpha:{-.1f,0.f,.5f,1.f})assert(activity(a,b,c,d,alpha)==bool(a&&(b||c||(d&&alpha>0))));
    // Failed owned acquisition never invents successful native demand.
    DemandAdapter fail; fail.establish(true,true); assert(fail.bind({1,1,1})); assert(!fail.layer(false,true,call,call,nullptr)); assert(!fail.can_acquire()&&!fail.owned_debt()&&native_calls==1);
    DemandAdapter handoff; handoff.establish(true,true); assert(handoff.bind({5,4,3})); real_result=true;
    assert(handoff.layer(false,true,call,call,nullptr)); real_result=false;
    assert(!handoff.external(true,call,nullptr)); auto before_suppress=native_calls;
    assert(handoff.layer(false,false,call,call,nullptr));
    assert(native_calls==before_suppress&&handoff.owned_debt()&&!handoff.can_acquire());
    handoff.establish(true,false); assert(handoff.owned_debt());
    handoff.establish(true,true); assert(!handoff.owned_debt()&&handoff.can_acquire());
    DemandAdapter cross; cross.establish(true,true); real_result=true;
    std::thread wrong([&]{assert(cross.external(true,call,nullptr));}); wrong.join(); assert(native_calls==before_suppress+1&&!cross.can_acquire());
    cross.establish(true,true); assert(!cross.can_acquire()); // concurrency stays latched
    Backend attached_bridge; BackendStatus attached_status;
    attached_status.capable=attached_status.serialized=attached_status.external_clear=true; attached_status.origin=1;
    attached_bridge.publish(attached_status); DemandAdapter attached(&attached_bridge); attached.establish(true,true);
    Lease held_lease; held_lease.epoch=1; held_lease.origin=1; held_lease.generation=1; held_lease.flags=1; held_lease.deadline_ns=2000000000;
    held_lease.revocation=attached_bridge.revocation(); attached_bridge.publish_lease(held_lease);
    Lease checked; assert(attached_bridge.lease(checked,time_ns)); assert(attached.bind({1,1,1}));
    assert(attached.layer(false,true,call,call,nullptr)); assert(!attached_bridge.new_lease_ready());
    assert(attached.external(true,call,nullptr)); assert(!attached_bridge.lease(checked,time_ns));
    assert(attached.layer(false,false,call,call,nullptr)); attached.establish(true,true);
    assert(attached_bridge.new_lease_ready()); assert(!attached_bridge.lease(checked,time_ns)); // no revival between control polls
    // Atomic-word snapshot readers never observe mixed generations under stress.
    struct Pair { uint64_t a=0,b=0; }; Mailbox<Pair> mailbox; mailbox.publish(Pair{1,~uint64_t(1)}); std::atomic<bool> done{false};
    std::thread writer([&]{for(uint64_t i=2;i<20000;++i)assert(mailbox.publish(Pair{i,~i})); done=true;});
    while(!done) { Pair p; if(mailbox.read(p))assert(p.b==~p.a); } writer.join();
    std::string root=std::string(getenv("TMPDIR"))+"/na-XXXXXX"; assert(mkdtemp(root.data()));
    Backend bridge; BackendStatus b; b.capable=b.serialized=b.external_clear=true; b.origin=3; bridge.publish(b);
    Server server(root,bridge,clock_now); assert(server.start()); Server collision(root,bridge,clock_now); assert(!collision.start());
    Client c; assert(c.connect(server.path())); Reply r; assert(c.exchange(q,r)); auto epoch=r.epoch;
    q.op=Operation::Update; q.epoch=epoch; q.origin=3; ++q.sequence; q.generation=1; assert(c.exchange(q,r));
    Client stranger; assert(stranger.connect(server.path())); Request wrong_epoch=q; wrong_epoch.op=Operation::Release; assert(stranger.exchange(wrong_epoch,r)&&r.reason==Reason::Epoch);
    Lease l; assert(bridge.lease(l,time_ns)); stranger.close();
    q.op=Operation::Release; ++q.sequence; assert(c.exchange(q,r)&&r.state==State::Off); ++q.sequence; assert(c.exchange(q,r)&&r.state==State::Off); c.close();
    // Malformed trailing packet bytes revoke an active owner, not truncate.
    int fd=raw_connect(server.path()); q=Request{}; q.sequence=1; q.sent_ns=time_ns; r=raw_exchange(fd,q); q.epoch=r.epoch; q.origin=r.origin; q.op=Operation::Update; q.sequence=2; q.generation=1; raw_exchange(fd,q);
    uint8_t over[129]{}; auto wire=encode(q); memcpy(over,wire.data(),128); assert(send(fd,over,129,MSG_NOSIGNAL)==129); assert(recv(fd,over,129,0)==128); assert(decode(over,128,r)&&r.reason==Reason::Invalid); assert(!bridge.lease(l,time_ns)); close(fd);
    // Real owner death while a passed descriptor keeps the connection open.
    int channel[2]; assert(!socketpair(AF_UNIX,SOCK_SEQPACKET,0,channel));
    std::string channel_fd=std::to_string(channel[1]); char* args[]={argv[0],const_cast<char*>("--child"),const_cast<char*>(server.path().c_str()),channel_fd.data(),nullptr};
    pid_t pid; assert(!posix_spawn(&pid,argv[0],nullptr,nullptr,args,environ)); close(channel[1]);
    char byte; iovec io{&byte,1}; alignas(cmsghdr) char ancillary[CMSG_SPACE(sizeof(int))]{}; msghdr m{}; m.msg_iov=&io;m.msg_iovlen=1;m.msg_control=ancillary;m.msg_controllen=sizeof ancillary;
    assert(recvmsg(channel[0],&m,0)==1); auto h=CMSG_FIRSTHDR(&m); assert(h&&h->cmsg_type==SCM_RIGHTS); int held; memcpy(&held,CMSG_DATA(h),sizeof held);
    assert(bridge.lease(l,time_ns)); assert(write(channel[0],"x",1)==1); int status; assert(waitpid(pid,&status,0)==pid&&WIFEXITED(status)&&!WEXITSTATUS(status));
    until([&]{return !bridge.lease(l,time_ns);}); close(held); close(channel[0]);
    // Unknown pre-existing external demand refuses a new lease.
    b.external_clear=false; bridge.publish(b); assert(c.connect(server.path())); q=Request{}; q.sequence=1; q.sent_ns=time_ns; assert(c.exchange(q,r)&&!r.epoch&&r.state==State::SuspendedExternal); c.close();
    server.stop();
    int ordinary=open(server.path().c_str(),O_CREAT|O_EXCL|O_WRONLY,0600); assert(ordinary>=0); close(ordinary); assert(!server.start()); assert(std::filesystem::is_regular_file(server.path()));
    std::filesystem::remove_all(root); puts("adversarial peer identity/dead-owner, packets, demand and atomic snapshot PASS");
}
