#include "Telemetry.hpp"
#include "SimHub.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace gr;
void check(bool b,const char* text){if(!b){std::cerr<<"FAIL: "<<text<<'\n';std::exit(1);}}
bool near(double a,double b,double epsilon=0.001){return std::abs(a-b)<epsilon;}
Sample base(){Sample s{};s.valid=true;s.controlled=true;s.identity=1;s.dt=0.01;return s;}
int main(){
    Settings cfg{};cfg.smoothingHz=0;Sample s=base();Sampler sampler;
    check(!sampler.update(s,cfg).OutputActive,"first sample neutral");
    s.velocity={1,0,0};s.position={0.005,0,0};auto f=sampler.update(s,cfg);
    check(f.OutputActive&&near(f.LocalAccelerationZ,98.0665,0.01),"surge clamped to configured G limit");
    check(near(f.LocalAccelerationX,0)&&near(f.LocalAccelerationY,0),"surge axes");
    check(near(f.SpeedKmh,3.6)&&near(f.SpeedMph,2.2369363),"speed units");
    // Constant speed in the chassis frame still produces centripetal force.
    sampler.reset();s=base();s.velocity={10,0,0};sampler.update(s,cfg);
    double angle=0.01;s.rotation={0,0,std::sin(angle/2),std::cos(angle/2)};
    s.velocity=s.rotation.rotate({10,0,0});s.position={0.1,0.0005,0};s.angularVelocity={0,0,1};
    f=sampler.update(s,cfg);check(near(f.LocalAccelerationX,10,0.01),"constant-speed cornering sway");
    check(near(f.YawSpeed,1)&&near(f.YawDegrees,angle*180/pi),"yaw rate and angle");
    check(near(f.YawAcceleration,0),"no invented angular derivative on acquisition");
    s.paused=true;f=sampler.update(s,cfg);check(f.Paused&&!f.OutputActive&&f.YawDegrees==0&&f.LocalAccelerationX==0,"pause neutral");
    s.paused=false;check(!sampler.update(s,cfg).OutputActive,"resume reacquires history");
    s.identity=2;check(!sampler.update(s,cfg).OutputActive,"new vehicle neutral");
    s.position={1000,0,0};check(!sampler.update(s,cfg).OutputActive,"teleport neutral");
    s.dt=1;check(!sampler.update(s,cfg).OutputActive,"long frame gap neutral");
    s.dt=0.01;s.velocity.x=std::numeric_limits<double>::quiet_NaN();f=sampler.update(s,cfg);
    check(!f.FrameValid&&!f.OutputActive,"invalid input neutral");
    check(json(f).find("nan")==std::string::npos,"JSON finite");
    f=neutral(f,true);check(f.Stale&&!f.FrameValid,"watchdog marks stale invalid");
    sampler.reset();s=base();s.rotation={-std::sin(pi/12),0,0,std::cos(pi/12)};sampler.update(s,cfg);f=sampler.update(s,cfg);
    check(near(f.RollDegrees,30),"roll right-down positive");
    sampler.reset();s=base();s.rotation={0,-std::sin(pi/12),0,std::cos(pi/12)};sampler.update(s,cfg);f=sampler.update(s,cfg);
    check(near(f.PitchDegrees,30),"pitch nose-up positive");
    sampler.reset();s=base();sampler.update(s,cfg);s.velocity={0,0,0.01};s.airborne=true;s.boost=true;f=sampler.update(s,cfg);
    check(f.BoostStarted&&f.BoostActive&&f.Airborne&&near(f.LocalAccelerationY,1),"boost and heave");
    s.airborne=false;s.boost=false;f=sampler.update(s,cfg);check(f.BoostEnded&&f.LandingEvent,"boost end and landing edge");
    f=sampler.update(s,cfg);check(!f.BoostEnded&&!f.LandingEvent,"event edges last one sample");
    check(simtools(f).starts_with("S~Roll:")&&simtools(f).ends_with("~E"),"Fox3 SimTools framing");
    f.PitchDegrees=30;f.LocalAccelerationX=2;f.LocalAccelerationY=3;f.LocalAccelerationZ=4;
    auto packet=external_packet(f,1,2,3,4);
    check(sizeof(packet)==188&&packet.GameSignature==0xE53FF221&&packet.TelemetrySignature==0xEC368575,"generated SimHub contract");
    check(packet.PitchDegrees==-30&&packet.LocalSwayMs2==2&&packet.LocalHeaveMs2==3&&packet.LocalSurgeMs2==4,"SimHub standard axes and pitch sign");
    check(packet.PacketsCounter==3&&packet.SessionId==2&&packet.SessionTimeSeconds==4,"SimHub header counters");
    std::cout<<"All motion sampler/protocol tests passed. Frame="<<sizeof(Frame)<<" SharedMemory="<<sizeof(SharedMemory)<<'\n';
}
