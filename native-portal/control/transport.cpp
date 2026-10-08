#include "control.hpp"
#include <cerrno>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <poll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
namespace native_portal {
// /proc start ticks distinguish PID reuse; Z/X also revoke a socket inherited by
// a live child. Reads are confined to the control thread, never render callbacks.
static uint64_t identity(int pid) {
    std::ifstream f("/proc/"+std::to_string(pid)+"/stat"); std::string line; if(!std::getline(f,line))return 0;
    auto pos=line.rfind(')'); if(pos==std::string::npos)return 0;
    std::istringstream in(line.substr(pos+2)); char state; in>>state; if(state=='Z'||state=='X')return 0;
    std::string word; for(int field=4;field<=21;++field)if(!(in>>word))return 0;
    uint64_t start=0; in>>start; return in?start:0;
}
static bool peer(int fd,int& pid,uint64_t& start) {
    ucred cred{}; socklen_t n=sizeof cred;
    if(getsockopt(fd,SOL_SOCKET,SO_PEERCRED,&cred,&n)||n!=sizeof cred||cred.uid!=geteuid()||cred.pid<=0)return false;
    pid=cred.pid; start=identity(pid); return start!=0;
}
static bool private_path(const std::string& path,bool directory) {
    struct stat s{}; return !lstat(path.c_str(),&s)&&s.st_uid==geteuid()&&
        (directory?S_ISDIR(s.st_mode):S_ISSOCK(s.st_mode))&&(s.st_mode&0777)==(directory?0700:0600);
}
static bool address(const std::string& path,sockaddr_un& a) {
    if(path.empty()||path[0]!='/'||path.size()>=sizeof a.sun_path)return false;
    a={}; a.sun_family=AF_UNIX; std::memcpy(a.sun_path,path.c_str(),path.size()+1); return true;
}
void Client::close() { if(fd_>=0)::close(fd_); fd_=-1; }
bool Client::connect(const std::string& path) {
    close(); sockaddr_un a{}; if(!address(path,a)||!private_path(path.substr(0,path.rfind('/')),true)||!private_path(path,false))return false;
    fd_=socket(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC|SOCK_NONBLOCK,0); if(fd_<0)return false;
    if(::connect(fd_,reinterpret_cast<sockaddr*>(&a),sizeof a)||!peer(fd_,pid_,start_)) { close(); return false; }
    return true;
}
bool Client::exchange(const Request& q,Reply& r) {
    if(fd_<0||identity(pid_)!=start_) { close(); return false; }
    auto w=encode(q); if(send(fd_,w.data(),w.size(),MSG_NOSIGNAL)!=ssize_t(w.size())) { close(); return false; }
    pollfd p{fd_,POLLIN,0}; int rc; do { rc=poll(&p,1,100); }while(rc<0&&errno==EINTR);
    if(rc!=1||!(p.revents&POLLIN)) { close(); return false; }
    uint8_t data[129]; auto n=recv(fd_,data,sizeof data,MSG_TRUNC);
    if(n!=128||!decode(data,size_t(n),r)||identity(pid_)!=start_) { close(); return false; }
    return true;
}
struct Server::Impl {
    struct Peer { int fd=-1,pid=0; uint64_t start=0,sequence=0,epoch=0; Reason revoked=Reason::None; };
    std::string runtime,dir,socket_path; Backend& bridge; Clock clock;
    int listener=-1,writer=-1; std::array<Peer,8> peers{}; std::thread worker;
    std::atomic<bool> quit{false}; uint64_t next_epoch=0,instance=0,submission_floor=0; Lease current{};
    dev_t dev=0; ino_t ino=0;
    Impl(std::string rt,Backend& b,Clock c):runtime(std::move(rt)),dir(runtime+"/frametop-native-portal"),socket_path(dir+"/control.sock"),bridge(b),clock(c) {}
    void revoke(Reason why) {
        if(writer>=0)peers[size_t(writer)].revoked=why;
        writer=-1; current=Lease{}; bridge.publish_lease(current);
    }
    void drop(size_t i) { if(writer==int(i))revoke(Reason::OwnerGone); ::close(peers[i].fd); peers[i]=Peer{}; }
    BackendStatus backend() { BackendStatus b; if(!bridge.status(b)) { b=BackendStatus{}; b.fault=true; } return b; }
    void expire(uint64_t now,const BackendStatus& b) {
        if(writer<0)return;
        auto& p=peers[size_t(writer)];
        if(!b.capable||!b.serialized)revoke(Reason::Unsupported);
        else if(b.fault)revoke(Reason::NativeFault);
        else if(!b.external_clear)revoke(Reason::External);
        else if(b.origin!=current.origin)revoke(Reason::Origin);
        else if(current.revocation!=bridge.revocation())revoke(Reason::External);
        else if(now>=current.deadline_ns)revoke(Reason::Expired);
        else if(identity(p.pid)!=p.start)revoke(Reason::OwnerGone);
    }
    Reply reply(const Peer& p,const BackendStatus& b) {
        Reply r; r.instance=instance; r.origin=b.origin; r.sequence=p.sequence; r.epoch=p.epoch;
        r.capability=b.capable&&b.serialized;
        if(p.revoked!=Reason::None) { r.state=p.revoked==Reason::External?State::SuspendedExternal:State::Fault; r.reason=p.revoked; return r; }
        if(!b.capable||!b.serialized||!b.origin) { r.reason=Reason::Unsupported; return r; }
        if(b.fault) { r.state=State::Fault; r.reason=Reason::NativeFault; return r; }
        if(!b.external_clear) { r.state=State::SuspendedExternal; r.reason=Reason::External; return r; }
        r.state=State::Off; r.reason=Reason::Off;
        if(p.epoch&&current.epoch==p.epoch) {
            r.generation=current.generation; r.reason=Reason::None; r.state=State::Warming;
            if(current.flags&2) { r.state=State::SuppressedMenu; r.reason=Reason::Menu; }
            else if(current.generation&&b.submitted_epoch==p.epoch&&b.submitted_frame>submission_floor) { r.state=State::ActiveSubmitted; r.submitted_frame=b.submitted_frame; }
        } return r;
    }
    Reply handle(size_t i,const uint8_t* data,size_t n,uint64_t now,BackendStatus b) {
        auto& p=peers[i]; Request q; expire(now,b);
        auto bad=[&](Reason why) { if(writer==int(i))revoke(why); else p.revoked=why; return reply(p,b); };
        if(!decode(data,n,q))return bad(Reason::Invalid);
        if(q.sequence<=p.sequence)return bad(Reason::Replay);
        p.sequence=q.sequence;
        if(identity(p.pid)!=p.start)return bad(Reason::OwnerGone);
        if(q.op==Operation::Acquire) {
            if(q.epoch||q.origin||!(q.flags&1)||!deadline(q.sent_ns,now,q.ttl_ms))return bad(Reason::Invalid);
            if(writer>=0) { auto r=reply(p,b); r.reason=Reason::Busy; return r; }
            if(!b.capable||!b.serialized||!b.external_clear||b.fault||!b.origin)return reply(p,b);
            if(!bridge.new_lease_ready()) { auto r=reply(p,b); r.state=State::Draining; r.reason=Reason::Busy; return r; }
            if(next_epoch==UINT64_MAX)return bad(Reason::Epoch);
            current=Lease{}; current.epoch=++next_epoch; current.origin=b.origin; current.revocation=bridge.revocation();
            current.deadline_ns=deadline(q.sent_ns,now,q.ttl_ms); current.flags=q.flags;
            submission_floor=b.submitted_frame;
            p.epoch=current.epoch; p.revoked=Reason::None; writer=int(i); bridge.publish_lease(current); return reply(p,b);
        }
        // Revoked connections cannot renew, release another lease, or restore old
        // geometry. Only a fresh explicit Acquire may create a new epoch.
        if(p.revoked!=Reason::None)return reply(p,b);
        if(!q.epoch||q.epoch!=p.epoch)return bad(Reason::Epoch);
        if(q.op==Operation::Release) {
            if(writer==int(i))revoke(Reason::Off);
            p.revoked=Reason::None; return reply(p,b); // same-epoch idempotence
        }
        if(writer!=int(i)||q.epoch!=current.epoch)return bad(Reason::Epoch);
        if(q.origin!=current.origin)return bad(Reason::Origin);
        if(q.op==Operation::Update) {
            auto d=deadline(q.sent_ns,now,q.ttl_ms);
            if(!d)return bad(Reason::Expired);
            if(!(q.flags&1)||!q.generation||q.generation<current.generation)return bad(Reason::Invalid);
            // Reusing a geometry generation must mean byte-identical geometry.
            if(q.generation==current.generation&&std::memcmp(&q.geometry,&current.geometry,sizeof(Geometry)))return bad(Reason::Replay);
            if(q.flags&2)submission_floor=b.submitted_frame;
            current.generation=q.generation; current.geometry=q.geometry; current.flags=q.flags; current.deadline_ns=d;
            bridge.publish_lease(current);
        }
        return reply(p,b);
    }
    void run() {
        while(!quit) {
            std::array<pollfd,9> fds{}; fds[0]={listener,POLLIN,0};
            for(size_t i=0;i<peers.size();++i)fds[i+1]={peers[i].fd,POLLIN,0};
            poll(fds.data(),fds.size(),5);
            auto now=clock(); auto b=backend(); expire(now,b); // before ANY queued renewal
            if(fds[0].revents&POLLIN) {
                int fd=accept4(listener,nullptr,nullptr,SOCK_CLOEXEC|SOCK_NONBLOCK);
                if(fd>=0) { Peer p; p.fd=fd; bool added=false;
                    if(peer(fd,p.pid,p.start))for(auto& slot:peers)if(slot.fd<0) { slot=p; added=true; break; }
                    if(!added)::close(fd);
                }
            }
            for(size_t i=0;i<peers.size();++i) {
                if(peers[i].fd<0||fds[i+1].fd!=peers[i].fd)continue;
                auto events=fds[i+1].revents;
                if(events&POLLIN) {
                    uint8_t data[129]; auto n=recv(peers[i].fd,data,sizeof data,MSG_TRUNC);
                    if(n<=0) { drop(i); continue; }
                    auto r=handle(i,data,size_t(n),clock(),backend()); auto w=encode(r);
                    if(send(peers[i].fd,w.data(),w.size(),MSG_NOSIGNAL)!=ssize_t(w.size())) { drop(i); continue; }
                }
                if(events&(POLLERR|POLLHUP|POLLNVAL))drop(i);
            }
        }
        revoke(Reason::OwnerGone); for(size_t i=0;i<peers.size();++i)if(peers[i].fd>=0)drop(i);
    }
};
Server::Server(const std::string& runtime,Backend& b,Clock clock):impl_(new Impl(runtime,b,clock)) {}
Server::~Server() { stop(); delete impl_; }
const std::string& Server::path() const { return impl_->socket_path; }
bool Server::start() {
    auto& s=*impl_; if(s.listener>=0)return false; sockaddr_un a{};
    if(!private_path(s.runtime,true)||!address(s.socket_path,a))return false;
    if(mkdir(s.dir.c_str(),0700)&&errno!=EEXIST)return false;
    if(!private_path(s.dir,true))return false;
    // Refuse all collisions, including stale sockets and symlinks. Never unlink
    // another process's pathname to take ownership of the endpoint.
    struct stat st{}; if(!lstat(s.socket_path.c_str(),&st)||errno!=ENOENT)return false;
    s.listener=socket(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC|SOCK_NONBLOCK,0); if(s.listener<0)return false;
    if(bind(s.listener,reinterpret_cast<sockaddr*>(&a),sizeof a)) { ::close(s.listener); s.listener=-1; return false; }
    if(lstat(s.socket_path.c_str(),&st)) { ::close(s.listener); s.listener=-1; return false; }
    s.dev=st.st_dev; s.ino=st.st_ino;
    if(chmod(s.socket_path.c_str(),0600)||listen(s.listener,8)) { stop(); return false; }
    s.instance=boottime(); s.quit=false; s.worker=std::thread([&s]{s.run();}); return true;
}
void Server::stop() {
    auto& s=*impl_; s.quit=true; if(s.worker.joinable())s.worker.join();
    if(s.listener>=0) { ::close(s.listener); s.listener=-1; }
    struct stat st{}; if(s.ino&&!lstat(s.socket_path.c_str(),&st)&&st.st_ino==s.ino&&st.st_dev==s.dev)unlink(s.socket_path.c_str());
    s.ino=0;
}
}
