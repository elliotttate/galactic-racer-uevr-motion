#pragma once
#include "Telemetry.hpp"
#include "SimHubGenerated.hpp"
#include <cmath>

namespace gr {
inline SimHubExternalTelemetrySender::SHTelemetryPacket external_packet(
    const Frame& f,uint64_t emitter,uint64_t session,uint64_t counter,double seconds) {
    using namespace SimHubExternalTelemetrySender;
    SHTelemetryPacket p{};
    p.GameSignature=SHTelemetryConstants::GameSignature;
    p.TelemetrySignature=SHTelemetryConstants::TelemetrySignature;
    p.LayoutMajorVersion=1;p.LayoutMinorVersion=0;
    p.EmitterInstanceId=emitter;p.PacketId=0;p.PacketsCounter=counter;
    p.IsSessionRunning=f.FrameValid;p.IsSessionPaused=f.Paused||f.Stale;
    p.SessionId=session;p.IsUserInControl=f.OutputActive;
    p.SessionTimeSeconds=seconds;p.PhysicsDiscontinuityCounter=f.PhysicsDiscontinuityCounter;
    p.YawDegrees=f.YawDegrees;p.PitchDegrees=-f.PitchDegrees;p.RollDegrees=f.RollDegrees;
    p.YawRateDegreesPerSecond=f.YawSpeed*static_cast<float>(180/pi);
    p.PitchRateDegreesPerSecond=-f.PitchSpeed*static_cast<float>(180/pi);
    p.RollRateDegreesPerSecond=f.RollSpeed*static_cast<float>(180/pi);
    p.LocalSurgeMs2=f.LocalAccelerationZ;p.LocalSwayMs2=f.LocalAccelerationX;p.LocalHeaveMs2=f.LocalAccelerationY;
    p.LocalVelocityForwardMps=f.LocalVelocityZ;p.LocalVelocityLateralMps=f.LocalVelocityX;p.LocalVelocityUpMps=f.LocalVelocityY;
    p.VehiclePositionNorth=f.PositionX;p.VehiclePositionEast=f.PositionY;p.VehiclePositionUp=f.PositionZ;
    p.SpeedKmh=f.SpeedKmh;p.GroundSpeedKmh=std::hypot(f.LocalVelocityX,f.LocalVelocityZ)*3.6f;
    p.EngineRpm=f.RPM;p.Throttle=f.Throttle;p.Brake=f.Brake;
    p.TimestampMS=f.TimestampMS;p.DeltaTimeSeconds=f.DeltaTime;
    p.SpeedMps=f.SpeedMps;p.SpeedMph=f.SpeedMph;p.EngineRpmRatio=f.RPMRatio;
    p.Steering=f.Steering;p.GForceLateral=f.GForceLateral;p.GForceVertical=f.GForceVertical;p.GForceLongitudinal=f.GForceLongitudinal;
    p.BoostActive=static_cast<uint8_t>(f.BoostActive);p.BoostStarted=static_cast<uint8_t>(f.BoostStarted);
    p.BoostEnded=static_cast<uint8_t>(f.BoostEnded);p.Airborne=static_cast<uint8_t>(f.Airborne);p.LandingEvent=static_cast<uint8_t>(f.LandingEvent);
    return p;
}
}
