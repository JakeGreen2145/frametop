// SPDX-License-Identifier: MIT
// Direct helper checks with real AF_UNIX output supplied by the Python runner.
#include "camera_fatal.hpp"
#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <sys/resource.h>
using namespace native_portal::live;
static unsigned sends=0;
extern "C" ssize_t __real_send(int,const void*,size_t,int);
extern "C" ssize_t __wrap_send(int fd,const void* data,size_t size,int flags) {
    ++sends;
    assert(fd==STDERR_FILENO);
    assert(flags==(MSG_DONTWAIT|MSG_NOSIGNAL));
    assert(size>0 && size<=256);
    assert(static_cast<const char*>(data)[size-1]=='\n');
    return __real_send(fd,data,size,flags);
}
int main(int argc,char** argv) {
    assert(argc==3);
    rlimit zero{0,0};setrlimit(RLIMIT_CORE,&zero);
    signal(SIGPIPE,SIG_DFL);
    const int fd=std::atoi(argv[2]);
    if(fd<0)close(STDERR_FILENO);else assert(dup2(fd,STDERR_FILENO)==STDERR_FILENO);
    const bool fatal=std::strcmp(argv[1],"abort")==0;
    for(unsigned reason=1;reason<=13;++reason) {
        const CameraFatalRecord record{static_cast<CameraFatalReason>(reason),UINT64_MAX,UINT64_MAX,
                                       0,123,bool(reason&1),bool(reason&2),bool(reason&4),bool(reason&8)};
        errno=EDOM;
        if(fatal)abort_camera_failure(record);
        emit_camera_failure(record);
        assert(errno==EDOM);
        assert(sends==reason); // Exactly one send, including every failed send.
    }
    return 0;
}
