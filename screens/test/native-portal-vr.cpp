// Reuse only pinned-header-derived in-memory OpenVR boundaries, not live VR.
#define main ControlsRegressionMain
#include "volume-controls.cpp"
#undef main
#include "../../native-portal/control/control.hpp"
#include <filesystem>
#include <thread>
template<class F> static void wait_for(F f) { for(int i=0;i<500;++i) { Tick(); if(f())return; usleep(2000); } assert(false&&"portal adapter timeout"); }
int main() {
    const char* runtime=std::getenv("XDG_RUNTIME_DIR"); assert(runtime&&std::string(runtime).find("np-test-")!=std::string::npos);
    assert(Reply("passthrough portal status").find("ok portal {")==0);
    assert(Reply("passthrough portal on extra").find("error")==0);
    assert(Reply("passthrough portal status").find("\"wanted\":false")!=std::string::npos);
    native_portal::Backend bridge; native_portal::Server server(runtime,bridge); assert(server.start());
    g_vr=true; systemVR.poses[0].bPoseIsValid=true; systemVR.poses[0].bDeviceIsConnected=true;
    systemVR.poses[0].eTrackingResult=vr::TrackingResult_Running_OK;
    systemVR.poses[0].mDeviceToAbsoluteTracking=Translation(0,1.6,0);
    Command("passthrough portal on");
    wait_for([&]{return Reply("passthrough portal status").find("unsupported-backend")!=std::string::npos;});
    native_portal::Lease lease; assert(!bridge.lease(lease,native_portal::boottime()));
    native_portal::BackendStatus backend; backend.capable=true; backend.serialized=true; backend.external_clear=true; backend.origin=19; bridge.publish(backend);
    // Real poll-loop recovery must retain the failed On, even after capability appears.
    for(bool paused:{false,true}) {
        g_paused=paused; systemVR.poses[0].bPoseIsValid=paused;
        wait_for([&]{return Reply("passthrough portal status").find("tracking-invalid")!=std::string::npos;});
        g_paused=false; systemVR.poses[0].bPoseIsValid=true;
        wait_for([&]{
            assert(!bridge.lease(lease,native_portal::boottime()));
            return Reply("passthrough portal status").find("unsupported-backend")!=std::string::npos;
        });
        assert(Reply("passthrough portal status").find("\"epoch\":1,")!=std::string::npos);
    }
    Command("passthrough set 0 0.4 0.8 -0.7 1.2 0.3 0.6 37 25 -15");
    Command("passthrough portal on");
    wait_for([&]{return bridge.lease(lease,native_portal::boottime());});
    assert(lease.origin==19 && !g_passthrough.enabled && lease.geometry.center[0]==float(g_passthrough.center[0]));
    // Held/live geometry flows without save, and never depends on legacy enabled.
    g_passthrough.center[0]=.5; g_passthrough.pitch=70;
    wait_for([&]{return bridge.lease(lease,native_portal::boottime())&&lease.geometry.center[0]==.5f;});
    const auto pose=passthrough::VolumePose(g_passthrough); const auto& q=lease.geometry.quaternion;
    assert(std::abs(pose[0][0]-(1-2*(q[1]*q[1]+q[2]*q[2])))<1e-6);
    // Pause and invalid tracking remove only this owner's lease.
    g_paused=true; wait_for([&]{return !bridge.lease(lease,native_portal::boottime());});
    g_paused=false; wait_for([&]{return bridge.lease(lease,native_portal::boottime());});
    systemVR.poses[0].bPoseIsValid=false; wait_for([&]{return !bridge.lease(lease,native_portal::boottime());});
    systemVR.poses[0].bPoseIsValid=true; wait_for([&]{return bridge.lease(lease,native_portal::boottime());});
    overlay.steamMenu=true; wait_for([&]{return Reply("passthrough portal status").find("suppressed-menu")!=std::string::npos;});
    assert(!bridge.lease(lease,native_portal::boottime()));
    Command("passthrough layer 1"); usleep(70000); Tick(); usleep(70000);
    assert(!bridge.lease(lease,native_portal::boottime())); // control preference is not camera ordering proof
    Command("passthrough layer 0");
    overlay.steamMenu=false; wait_for([&]{return bridge.lease(lease,native_portal::boottime());});
    Command("passthrough portal off"); wait_for([&]{return !bridge.lease(lease,native_portal::boottime());});
    assert(Reply("passthrough portal status").find("\"wanted\":false")!=std::string::npos);
    assert(launches.empty());
    Command("passthrough portal on"); wait_for([&]{return bridge.lease(lease,native_portal::boottime());});
    ft_vr_shutdown(); wait_for([&]{return !bridge.lease(lease,native_portal::boottime());});
    server.stop(); std::puts("native ft_vr_command/poll/shutdown private backend PASS");
}
