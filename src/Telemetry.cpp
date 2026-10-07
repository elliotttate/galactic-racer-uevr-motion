#include "Telemetry.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>

namespace gr {
double Vec3::length() const { return std::sqrt(x*x+y*y+z*z); }
bool Vec3::finite() const { return std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z); }
Vec3 Quat::rotate(Vec3 v) const {
    Vec3 t{2*(y*v.z-z*v.y),2*(z*v.x-x*v.z),2*(x*v.y-y*v.x)};
    return v+t*w+Vec3{y*t.z-z*t.y,z*t.x-x*t.z,x*t.y-y*t.x};
}
bool Quat::normalize() {
    double n=std::sqrt(x*x+y*y+z*z+w*w);
    if (!std::isfinite(n)||n<0.5||n>1.5) return false;
    x/=n;y/=n;z/=n;w/=n;return true;
}
static float bounded(double v,double limit) {
    return std::isfinite(v)?static_cast<float>(std::clamp(v,-limit,limit)):0.0f;
}
Frame neutral(Frame f,bool stale) {
    // Preserve identity and position for diagnostics; clear all usable rig cues.
    f.OutputActive=0;f.Stale=stale;f.DeltaTime=0;
    f.SpeedMps=f.SpeedKmh=f.SpeedMph=f.RPM=f.RPMRatio=0;
    f.Throttle=f.Brake=f.Steering=0;
    f.LocalVelocityX=f.LocalVelocityY=f.LocalVelocityZ=0;
    f.LocalAccelerationX=f.LocalAccelerationY=f.LocalAccelerationZ=0;
    f.GForceLateral=f.GForceVertical=f.GForceLongitudinal=0;
    f.Roll=f.Pitch=f.Yaw=f.RollDegrees=f.PitchDegrees=f.YawDegrees=0;
    f.RollSpeed=f.PitchSpeed=f.YawSpeed=0;
    f.RollAcceleration=f.PitchAcceleration=f.YawAcceleration=0;
    f.BoostActive=f.BoostStarted=f.BoostEnded=f.Airborne=f.LandingEvent=0;
    if(stale) f.FrameValid=0;
    return f;
}
void Sampler::reset() { hasPrevious=false;hasRates=false;filtered={};previousRates={}; }
Frame Sampler::update(Sample s,const Settings& cfg) {
    Frame f{};f.StructSize=sizeof(f);f.Sequence=++sequence;
    f.TimestampMS=static_cast<uint32_t>(s.timestampMs);f.Paused=s.paused;
    f.FrameValid=s.valid&&s.position.finite()&&s.velocity.finite()&&s.angularVelocity.finite()&&s.rotation.normalize();
    f.VehicleArchetype=s.vehicleArchetype;f.InputMode=s.inputMode;
    f.BoostState=s.boostState;f.LandingState=s.landingState;
    f.PhysicsDiscontinuityCounter=discontinuities;
    bool active=f.FrameValid&&!s.paused&&s.controlled&&!s.cinematic;
    if(!active) { if(hasPrevious)++discontinuities;reset();f.PhysicsDiscontinuityCounter=discontinuities;return neutral(f); }
    f.PositionX=bounded(s.position.x,1e9);f.PositionY=bounded(s.position.y,1e9);f.PositionZ=bounded(s.position.z,1e9);
    bool continuous=hasPrevious&&s.identity==previous.identity&&std::isfinite(s.dt)&&s.dt>1e-5&&s.dt<=cfg.maxDeltaSeconds;
    if(continuous&&s.timestampMs-previous.timestampMs>250)continuous=false;
    if(continuous) {
        // Prediction residual rejects respawns/teleports, allowing normal high speed travel.
        Vec3 predicted=(s.velocity+previous.velocity)*(0.5*s.dt);
        if((s.position-previous.position-predicted).length()>cfg.teleportDistanceM) continuous=false;
    }
    if(!continuous) {
        ++discontinuities;reset();previous=s;hasPrevious=true;
        f.PhysicsDiscontinuityCounter=discontinuities;
        return neutral(f); // one neutral sample; never differentiate across a gap
    }
    f.OutputActive=1;f.DeltaTime=static_cast<float>(s.dt);
    // Differentiate WORLD velocity first. Differentiating local velocity loses
    // centripetal acceleration during a turn at constant local forward speed.
    Vec3 accel=s.rotation.unrotate((s.velocity-previous.velocity)*(1/s.dt));
    double limit=cfg.maxAccelerationG*gravity;
    accel={bounded(accel.x,limit),bounded(accel.y,limit),bounded(accel.z,limit)};
    double alpha=cfg.smoothingHz>0?1-std::exp(-2*pi*cfg.smoothingHz*s.dt):1;
    filtered=filtered+(accel-filtered)*alpha;
    Vec3 local=s.rotation.unrotate(s.velocity);
    f.LocalVelocityX=bounded(local.y,10000);f.LocalVelocityY=bounded(local.z,10000);f.LocalVelocityZ=bounded(local.x,10000);
    f.LocalAccelerationX=bounded(filtered.y*cfg.lateralSign,limit);
    f.LocalAccelerationY=bounded(filtered.z*cfg.verticalSign,limit);
    f.LocalAccelerationZ=bounded(filtered.x*cfg.longitudinalSign,limit);
    f.GForceLateral=f.LocalAccelerationX/static_cast<float>(gravity);
    f.GForceVertical=f.LocalAccelerationY/static_cast<float>(gravity);
    f.GForceLongitudinal=f.LocalAccelerationZ/static_cast<float>(gravity);
    Vec3 forward=s.rotation.rotate({1,0,0}),right=s.rotation.rotate({0,1,0}),up=s.rotation.rotate({0,0,1});
    f.Roll=static_cast<float>(std::atan2(-right.z,up.z)*cfg.rollSign);
    f.Pitch=static_cast<float>(std::asin(std::clamp(forward.z,-1.0,1.0))*cfg.pitchSign);
    f.Yaw=static_cast<float>(std::atan2(forward.y,forward.x)*cfg.yawSign);
    constexpr double deg=180/pi;
    f.RollDegrees=f.Roll*static_cast<float>(deg);f.PitchDegrees=f.Pitch*static_cast<float>(deg);f.YawDegrees=f.Yaw*static_cast<float>(deg);
    Vec3 av=s.rotation.unrotate(s.angularVelocity);
    double maxRate=cfg.maxAngularSpeedDeg/deg;
    f.RollSpeed=bounded(-av.x*cfg.rollSign,maxRate);
    f.PitchSpeed=bounded(-av.y*cfg.pitchSign,maxRate);
    f.YawSpeed=bounded(av.z*cfg.yawSign,maxRate);
    Vec3 rates{f.RollSpeed,f.PitchSpeed,f.YawSpeed};
    // The first rate sample after a reset has no prior derivative.
    if(hasRates) {
        f.RollAcceleration=bounded((rates.x-previousRates.x)/s.dt,1000);
        f.PitchAcceleration=bounded((rates.y-previousRates.y)/s.dt,1000);
        f.YawAcceleration=bounded((rates.z-previousRates.z)/s.dt,1000);
    }
    double speed=s.velocity.length();
    f.SpeedMps=bounded(speed,10000);f.SpeedKmh=f.SpeedMps*3.6f;f.SpeedMph=f.SpeedMps*2.2369363f;
    f.Throttle=std::clamp(bounded(s.throttle,1),0.0f,1.0f);f.Brake=std::clamp(bounded(s.brake,1),0.0f,1.0f);f.Steering=bounded(s.steering,1);
    f.RPM=std::max(0.0f,bounded(s.rpm,1e6));f.RPMRatio=std::clamp(bounded(s.rpmRatio,1),0.0f,1.0f);
    f.BoostActive=s.boost;f.BoostStarted=s.boost&&!previous.boost;f.BoostEnded=!s.boost&&previous.boost;
    f.Airborne=s.airborne;f.LandingEvent=!s.airborne&&previous.airborne;
    f.RamjetHeat=bounded(s.ramjetHeat,1e6);
    f.PhysicsDiscontinuityCounter=discontinuities;
    previous=s;previousRates=rates;hasRates=true;return f;
}
std::string json(const Frame& f) {
    std::ostringstream s;s.imbue(std::locale::classic());s<<std::fixed<<std::setprecision(6);
    s<<"{\"Game\":\"STAR WARS Galactic Racer\",\"TelemetryVersion\":1";
#define FIELD(name) s<<",\"" #name "\":"<<f.name
    FIELD(GameRunning);FIELD(Paused);FIELD(OutputActive);FIELD(FrameValid);FIELD(Stale);
    FIELD(Sequence);FIELD(TimestampMS);FIELD(DeltaTime);FIELD(PhysicsDiscontinuityCounter);
    FIELD(SpeedMps);FIELD(SpeedKmh);FIELD(SpeedMph);FIELD(RPM);FIELD(RPMRatio);
    FIELD(Throttle);FIELD(Brake);FIELD(Steering);
    FIELD(LocalVelocityX);FIELD(LocalVelocityY);FIELD(LocalVelocityZ);
    FIELD(LocalAccelerationX);FIELD(LocalAccelerationY);FIELD(LocalAccelerationZ);
    FIELD(GForceLateral);FIELD(GForceVertical);FIELD(GForceLongitudinal);
    FIELD(Roll);FIELD(Pitch);FIELD(Yaw);FIELD(RollDegrees);FIELD(PitchDegrees);FIELD(YawDegrees);
    FIELD(RollSpeed);FIELD(PitchSpeed);FIELD(YawSpeed);FIELD(RollAcceleration);FIELD(PitchAcceleration);FIELD(YawAcceleration);
    FIELD(PositionX);FIELD(PositionY);FIELD(PositionZ);
    FIELD(BoostActive);FIELD(BoostStarted);FIELD(BoostEnded);FIELD(Airborne);FIELD(LandingEvent);
    FIELD(VehicleArchetype);FIELD(InputMode);FIELD(BoostState);FIELD(LandingState);FIELD(RamjetHeat);
#undef FIELD
    s<<"}";return s.str();
}
std::string simtools(const Frame& f) {
    std::ostringstream s;s.imbue(std::locale::classic());s<<std::fixed<<std::setprecision(6);
    s<<"S~Roll:"<<f.RollDegrees<<":Pitch:"<<f.PitchDegrees<<":Yaw:"<<f.YawDegrees
     <<":RollSpeed:"<<f.RollSpeed*(180/pi)<<":PitchSpeed:"<<f.PitchSpeed*(180/pi)<<":YawSpeed:"<<f.YawSpeed*(180/pi)
     <<":Vertical:"<<f.LocalAccelerationY<<":Lateral:"<<f.LocalAccelerationX<<":Longitudinal:"<<f.LocalAccelerationZ<<"~E";
    return s.str();
}
}
