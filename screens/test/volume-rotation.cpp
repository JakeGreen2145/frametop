#include "../passthrough.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
using namespace passthrough;
void near(const Point &a, const Point &b) { for(int i=0;i<3;++i) assert(std::fabs(a[i]-b[i])<1e-8); }
int main() {
    Volume v; char reply[512];
    assert(Command("passthrough set 1 1 2 3 2 1 3 31 24 -17",v,reply,sizeof reply));
    assert(std::strncmp(reply,"ok ",3)==0);
    // Independent Ry Rx Rz reference, not the implementation's own inverse.
    const double y=31*M_PI/180,p=24*M_PI/180,r=-17*M_PI/180;
    const double cy=cos(y),sy=sin(y),cp=cos(p),sp=sin(p),cr=cos(r),sr=sin(r);
    const Point X{cy*cr+sy*sp*sr,cp*sr,-sy*cr+cy*sp*sr};
    const Point Y{-cy*sr+sy*sp*cr,cp*cr,sy*sr+cy*sp*cr};
    const Point Z{sy*cp,-sp,cy*cp};
    for(int j=0;j<3;++j) {
        Point unit{},want{};unit[j]=1;
        for(int k=0;k<3;++k) want[k]=v.center[k]+(j==0?X[k]:j==1?Y[k]:Z[k]);
        near(ToWorld(v,unit),want);near(ToLocal(v,want),unit);
    }
    // Native portal compatibility: the aperture is the box top, not its center.
    const auto plane=TopFace(v);
    assert(plane.width==v.size[0] && plane.length==v.size[2]);
    for(int k=0;k<3;++k) {
        assert(std::fabs(plane.pose[k][3]-v.center[k]-Y[k]*v.size[1]/2)<1e-8);
        assert(std::fabs(plane.pose[k][0]-X[k])<1e-8);
        assert(std::fabs(plane.pose[k][1]+Z[k])<1e-8);
        assert(std::fabs(plane.pose[k][2]-Y[k])<1e-8);
    }
    assert(ValidPose(plane.pose));
    const auto faces=FaceCenters(v);
    for(int face=0;face<6;++face) {
        Volume resized;assert(ResizeFace(resized,v,face,.3));
        near(FaceCenters(resized)[face^1],faces[face^1]);
    }
    near(GrabCenter(v),ToWorld(v,{0,.5,1.5}));
    assert(Occludes(v,ToWorld(v,{0,0,5}),ToWorld(v,{0,0,-5})));
    assert(!Occludes(v,ToWorld(v,{2,0,5}),ToWorld(v,{2,0,-5})));
    assert(Command("passthrough state3d",v,reply,sizeof reply));
    assert(std::strstr(reply,"31 24 -17"));
    const auto good=v;
    for(const char *bad:{"passthrough set 1 1 2 3 2 1 3 31 24", "passthrough set 1 1 2 3 2 1 3 31 24 nan", "passthrough set 1 1 2 3 2 1 3 31 24 181"}) {
        Command(bad,v,reply,sizeof reply);assert(std::strncmp(reply,"error",5)==0);
        near(ToWorld(v,{1,2,3}),ToWorld(good,{1,2,3}));
    }
    Command("passthrough set 1 1 2 3 2 1 3 31",v,reply,sizeof reply);
    near(ToWorld(v,{0,1,0}),{1,3,3}); // legacy set resets pitch/roll
    // Local slabs and six face resize rays must rotate with the volume.
    v=good;
    for(int face=0;face<6;++face) {
        const int axis=face/2, sign=face%2 ? 1 : -1;
        Point outside{},inside{},miss{};
        outside[axis]=sign*(v.size[axis]/2+1);
        inside[axis]=-outside[axis];
        miss=outside;miss[(axis+1)%3]=v.size[(axis+1)%3];
        assert(Occludes(v,ToWorld(v,outside),ToWorld(v,inside)));
        auto other=miss;other[axis]=inside[axis];
        assert(!Occludes(v,ToWorld(v,miss),ToWorld(v,other)));
        Point direction{},origin=FaceCenters(v)[face];
        const auto pose=VolumePose(v);
        const int transverse=(axis+1)%3;
        for(int k=0;k<3;++k) { direction[k]=-pose[k][transverse]; origin[k]+=pose[k][transverse]; }
        ResizeDrag resize;auto edited=v;
        assert(resize.Begin(v,face,7,{origin,direction}));
        assert(resize.Update(edited,{origin,direction}));near(edited.center,v.center);
        for(int k=0;k<3;++k) origin[k]+=.2*sign*pose[k][axis];
        assert(resize.Update(edited,{origin,direction}));
        assert(std::fabs(edited.size[axis]-v.size[axis]-.2)<1e-8);
        near(FaceCenters(edited)[face^1],FaceCenters(v)[face^1]);
        assert(edited.yaw==v.yaw && edited.pitch==v.pitch && edited.roll==v.roll);
        assert(!resize.Finish(edited,8) && resize.Active());
        resize.Cancel(edited);near(edited.center,v.center);assert(edited.size==v.size);
    }
    // Decompose and reconstruct beyond canonical pitch, including both poles.
    for(double yaw:{-180.,-79.,0.,31.,180.})
        for(double pitch:{-180.,-91.,-90.,-89.9999,0.,89.9999,90.,91.,180.})
            for(double roll:{-180.,-17.,0.,122.,180.}) {
                TranslateDrag drag;Volume edited=good,target=good;
                target.yaw=yaw;target.pitch=pitch;target.roll=roll;
                const auto g=GrabCenter(edited);
                assert(drag.Begin(edited,7,{{g[0],g[1],g[2]+1},{0,0,-1}}));
                assert(drag.UpdatePose(edited,VolumePose(target)));
                for(Point unit: {Point{1,0,0},Point{0,1,0},Point{0,0,1}})
                    near(ToWorld(edited,unit),ToWorld(target,unit));
                const auto once=VolumePose(edited);
                for(int i=0;i<30;++i) assert(drag.UpdatePose(edited,VolumePose(target)));
                assert(VolumePose(edited)==once);
                assert(!drag.Finish(edited,8) && drag.Active());
                assert(drag.Finish(edited,7));
            }
    TranslateDrag drag;v=good;
    const auto g=GrabCenter(v);
    assert(drag.Begin(v,7,{{g[0],g[1],g[2]+1},{0,0,-1}}));
    auto rotated=good;rotated.roll+=20;
    assert(drag.UpdatePose(v,VolumePose(rotated))); // rotation alone counts as a change
    for(int bad=0;bad<4;++bad) {
        auto invalid=VolumePose(v);
        if(bad==0) invalid[0][0]=std::numeric_limits<double>::quiet_NaN();
        if(bad==1) for(int k=0;k<3;++k) invalid[k][0]*=-1; // reflection
        if(bad==2) for(int k=0;k<3;++k) invalid[k][1]*=1.1; // scale
        if(bad==3) invalid[0][1]+=.1; // shear
        const auto previous=VolumePose(v);
        assert(!drag.UpdatePose(v,invalid));assert(VolumePose(v)==previous);
    }
    drag.Cancel(v);assert(VolumePose(v)==VolumePose(good));
    assert(drag.Begin(v,7,{{g[0],g[1],g[2]+1},{0,0,-1}}));
    assert(drag.UpdatePose(v,VolumePose(good)));assert(!drag.Finish(v,7));
    std::puts("volume rotation: 3-axis basis, inverse, slabs, handles, opposite-face resize, IPC legacy/strict");
}
