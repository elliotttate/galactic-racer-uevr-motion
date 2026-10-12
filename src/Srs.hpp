#pragma once
#include "Telemetry.hpp"
#include <cmath>
#include <cstddef>
#include <algorithm>
#include <cstring>
#include <string>

// Sim Racing Studio (SRS) UDP API, version 103. SRS listens on UDP port 33001
// (configurable in its config.ini) and shows any sender as the "API" game.
// The layout is the natural (aligned) layout SRS decodes with Python's native
// struct format '3sI50s50s50sfffiffffffffffffIIIIf': 244 bytes in total.
// Units: speed in m/s, angles in degrees, accelerations in G.
namespace gr {
struct SrsPacket {
    char Api[3];            // "srs"
    uint32_t Version;       // 103
    char Game[50], Vehicle[50], Location[50];
    float Speed, Rpm, MaxRpm;
    int32_t Gear;           // -1 reverse, 0 neutral, 1..9 gears
    float Pitch, Roll, Yaw; // degrees
    float LateralVelocity, LongitudinalVelocity;
    float LateralAcceleration, VerticalAcceleration, LongitudinalAcceleration; // G
    float SuspensionFrontLeft, SuspensionFrontRight, SuspensionRearLeft, SuspensionRearRight;
    uint32_t TerrainFrontLeft, TerrainFrontRight, TerrainRearLeft, TerrainRearRight;
    float SteeringWheelAngle;
};
static_assert(sizeof(SrsPacket)==244,"SRS API v103 packet must be 244 bytes");
static_assert(offsetof(SrsPacket,Version)==4&&offsetof(SrsPacket,Game)==8&&offsetof(SrsPacket,Speed)==160,"SRS API v103 offsets");

// Copies text into a fixed, NUL-terminated field (SRS reads 50-byte strings).
template<size_t N> void srs_text(char (&field)[N],const std::string& text) {
    std::memset(field,0,N);
    std::memcpy(field,text.data(),std::min(text.size(),N-1));
}

inline SrsPacket srs_packet(const Frame& f) {
    SrsPacket p{};
    std::memcpy(p.Api,"srs",3);
    p.Version=103;
    srs_text(p.Game,"STAR WARS Galactic Racer");
    srs_text(p.Vehicle,"Archetype "+std::to_string(f.VehicleArchetype));
    p.Speed=f.SpeedMps;p.Rpm=f.RPM;
    p.MaxRpm=f.RPMRatio>0.01f?f.RPM/f.RPMRatio:0.0f;
    p.Gear=0; // hover vehicle: no gearbox
    p.Pitch=f.PitchDegrees;p.Roll=f.RollDegrees;p.Yaw=f.YawDegrees;
    p.LongitudinalVelocity=f.LocalVelocityZ;
    p.LateralAcceleration=f.GForceLateral;
    p.VerticalAcceleration=f.GForceVertical;
    p.LongitudinalAcceleration=f.GForceLongitudinal;
    p.SteeringWheelAngle=f.Steering*100.0f; // SRS API steering range is +/-100
    return p;
}
}
