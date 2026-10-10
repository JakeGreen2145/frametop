// SPDX-License-Identifier: MIT
#include "camera_binding_diagnostic.hpp"
#include <array>
#include <cassert>
#include <cstring>
#include <fcntl.h>
#include <cstdio>
#include <sys/socket.h>
using namespace native_portal::live;
int main() {
    CameraBindingDiagnostic d{};
    // Stress every scalar width, including diagnostic tri-state values. This
    // does not authorize state or imply any native source observations.
    std::array<uint64_t,sizeof(d)/sizeof(uint64_t)> words;words.fill(UINT64_MAX);
    static_assert(sizeof(words)==sizeof(d));std::memcpy(static_cast<void*>(&d),words.data(),sizeof d);
    std::array<char,1538> bytes{};bytes.front()='a';bytes.back()='z';
    const auto n=format_camera_binding(d,bytes.data()+1,1536);
    assert(n && n<1536 && bytes.front()=='a' && bytes.back()=='z' && bytes[n]=='\n');
    assert(!format_camera_binding(d,bytes.data(),1535));
    int sockets[2];assert(!socketpair(AF_UNIX,SOCK_STREAM,0,sockets));
    const auto flags=fcntl(sockets[1],F_GETFL);const int prior=dup(STDERR_FILENO);
    assert(dup2(sockets[1],STDERR_FILENO)==STDERR_FILENO);
    errno=EDOM;emit_camera_binding(d);assert(errno==EDOM);
    std::array<char,1536> received{};assert(read(sockets[0],received.data(),received.size())==ssize_t(n));
    assert(!std::memcmp(received.data(),bytes.data()+1,n));
    int small=4096;assert(!setsockopt(sockets[1],SOL_SOCKET,SO_SNDBUF,&small,sizeof small));
    while(send(sockets[1],received.data(),received.size(),MSG_DONTWAIT|MSG_NOSIGNAL)>0){}
    assert(errno==EAGAIN || errno==EWOULDBLOCK);
    errno=EDOM;emit_camera_binding(d);assert(errno==EDOM && fcntl(sockets[1],F_GETFL)==flags);
    close(sockets[0]);errno=EDOM;emit_camera_binding(d);assert(errno==EDOM);
    assert(dup2(prior,STDERR_FILENO)==STDERR_FILENO);close(prior);close(sockets[1]);
    std::printf("PASS camera binding scalar diagnostic, %zu bytes, saturated/closed sink and errno preserved\n",n);
}
