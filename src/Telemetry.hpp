#pragma once
#include <cstdint>
#include <string>

namespace gr {
constexpr double pi = 3.14159265358979323846;
constexpr double gravity = 9.80665;
struct Vec3 {
    double x{}, y{}, z{};
    Vec3 operator+(Vec3 b) const { return {x+b.x,y+b.y,z+b.z}; }
    Vec3 operator-(Vec3 b) const { return {x-b.x,y-b.y,z-b.z}; }
    Vec3 operator*(double k) const { return {x*k,y*k,z*k}; }
    double length() const;
    bool finite() const;
};
struct Quat {
    double x{}, y{}, z{}, w{1};
    Vec3 rotate(Vec3 v) const;
    Vec3 unrotate(Vec3 v) const { return Quat{-x,-y,-z,w}.rotate(v); }
    bool normalize();
};
// Sampler inputs: Unreal world X=forward/north, Y=right/east, Z=up.
// Position/velocity are converted from centimetres by the game adapter.
struct Sample {
    uint64_t identity{}, timestampMs{};
    double dt{};
    bool valid{}, paused{}, controlled{}, cinematic{}, boost{}, airborne{};
    Vec3 position{}, velocity{}, angularVelocity{}; // m, m/s, world rad/s
    Quat rotation{};
    float throttle{}, brake{}, steering{}, rpm{}, rpmRatio{}, ramjetHeat{};
    int32_t vehicleArchetype{}, inputMode{}, boostState{}, landingState{};
};
struct Settings {
    double smoothingHz{20}, maxAccelerationG{10}, maxAngularSpeedDeg{720};
    double teleportDistanceM{10}, maxDeltaSeconds{0.1};
    double rollSign{1}, pitchSign{1}, yawSign{1};
    double lateralSign{1}, verticalSign{1}, longitudinalSign{1};
};
#pragma pack(push, 4)
struct Frame {
    uint32_t Magic{0x31545247}; // little-endian GRT1
    uint16_t Version{1}, HeaderSize{48};
    uint32_t StructSize{}, Sequence{}, TimestampMS{};
    float DeltaTime{};
    int32_t GameRunning{1}, Paused{}, OutputActive{}, FrameValid{};
    uint32_t PhysicsDiscontinuityCounter{};
    int32_t Stale{};
    float SpeedMps{}, SpeedKmh{}, SpeedMph{}, RPM{}, RPMRatio{};
    float Throttle{}, Brake{}, Steering{};
    // Fox3 convention: X=lateral/right, Y=vertical/up, Z=longitudinal/forward.
    float LocalVelocityX{}, LocalVelocityY{}, LocalVelocityZ{};
    float LocalAccelerationX{}, LocalAccelerationY{}, LocalAccelerationZ{};
    float GForceLateral{}, GForceVertical{}, GForceLongitudinal{};
    float Roll{}, Pitch{}, Yaw{}; // radians; pitch nose-up, roll right-down
    float RollDegrees{}, PitchDegrees{}, YawDegrees{};
    float RollSpeed{}, PitchSpeed{}, YawSpeed{}; // rad/s
    float RollAcceleration{}, PitchAcceleration{}, YawAcceleration{}; // rad/s^2
    float PositionX{}, PositionY{}, PositionZ{}; // Unreal world, metres
    int32_t BoostActive{}, BoostStarted{}, BoostEnded{}, Airborne{}, LandingEvent{};
    int32_t VehicleArchetype{}, InputMode{}, BoostState{}, LandingState{};
    float RamjetHeat{};
};
struct SharedMemory {
    uint32_t WriteSequence{}; // read twice; matching even values mean stable copy
    Frame Data{};
};
#pragma pack(pop)
static_assert(sizeof(Frame)==216);
static_assert(sizeof(SharedMemory)==220);
class Sampler {
public:
    Frame update(Sample s, const Settings& settings);
    void reset();
private:
    Sample previous{};
    Vec3 filtered{}, previousRates{};
    bool hasPrevious{}, hasRates{};
    uint32_t sequence{}, discontinuities{};
};
Frame neutral(Frame f, bool stale=false);
std::string json(const Frame& f);
std::string simtools(const Frame& f);
}
