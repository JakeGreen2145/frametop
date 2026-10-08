#pragma once
#include "passthrough.h"
#include "../native-portal/control/control.hpp"
namespace native_portal {
// Owned by ft-screens, not by the GUI lifetime. All commands/ticks are on its
// existing VR thread. Only the private worker performs socket/proc/poll work.
class Owner {
    struct Impl; Impl* impl_;
public:
    explicit Owner(std::string socket_path={},Clock=boottime); ~Owner();
    Owner(const Owner&)=delete; Owner& operator=(const Owner&)=delete;
    bool command(const char*,char*,int);
    void tick(const passthrough::Volume&,bool tracking,bool paused,bool menu);
    void shutdown();
};
}
