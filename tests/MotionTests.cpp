#include "Telemetry.hpp"
#include "SimHub.hpp"
#include "Srs.hpp"
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <cstring>
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
    // SRS API v103 packet: layout, header, units and field mapping.
    {
        Frame g{};g.SpeedMps=12.5f;g.RPM=3000;g.RPMRatio=0.5f;g.VehicleArchetype=7;
        g.PitchDegrees=10;g.RollDegrees=-20;g.YawDegrees=30;
        g.GForceLateral=0.5f;g.GForceVertical=1.25f;g.GForceLongitudinal=-0.75f;
        g.LocalVelocityZ=40;g.Steering=-0.5f;
        auto q=srs_packet(g);
        check(sizeof(q)==244,"SRS packet is 244 bytes");
        check(std::memcmp(q.Api,"srs",3)==0&&q.Version==103,"SRS header and version");
        check(std::strcmp(q.Game,"STAR WARS Galactic Racer")==0&&std::strcmp(q.Vehicle,"Archetype 7")==0&&q.Location[0]==0,"SRS names");
        check(near(q.Speed,12.5)&&near(q.Rpm,3000)&&near(q.MaxRpm,6000)&&q.Gear==0,"SRS speed, rpm, max rpm, gear");
        check(near(q.Pitch,10)&&near(q.Roll,-20)&&near(q.Yaw,30),"SRS angles in degrees");
        check(near(q.LateralAcceleration,0.5)&&near(q.VerticalAcceleration,1.25)&&near(q.LongitudinalAcceleration,-0.75),"SRS accelerations in G");
        check(near(q.LongitudinalVelocity,40)&&near(q.LateralVelocity,0),"SRS velocities");
        check(near(q.SteeringWheelAngle,-50),"SRS steering range");
        check(q.SuspensionFrontLeft==0&&q.TerrainRearRight==0,"SRS unused channels are zero");
        Frame z{};z.RPMRatio=0;z.RPM=5;check(srs_packet(z).MaxRpm==0,"SRS max rpm without ratio");
        Frame n=neutral(g,true);auto nq=srs_packet(n);
        check(nq.Pitch==0&&nq.Roll==0&&nq.Yaw==0&&nq.LateralAcceleration==0&&nq.VerticalAcceleration==0&&nq.LongitudinalAcceleration==0,"SRS neutral frame has no motion");
        // Dump a known packet so an external test can decode it with SRS's own parser.
        if(const char* dump=std::getenv("SRS_PACKET_DUMP")){
            std::ofstream out(dump,std::ios::binary);check(static_cast<bool>(out),"open SRS dump");
            out.write(reinterpret_cast<const char*>(&q),sizeof(q));
        }
    }
    std::cout<<"All motion sampler/protocol tests passed. Frame="<<sizeof(Frame)<<" SharedMemory="<<sizeof(SharedMemory)<<'\n';
}
