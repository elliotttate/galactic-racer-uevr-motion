#pragma once
// Extracted unchanged from SimHub 9.11.12's generated sender.
#include <cstdint>
namespace SimHubExternalTelemetrySender {
    struct SHTelemetryConstants
    {
        static constexpr const char* DefinitionUniqueId = "84171ddf-ddac-4be5-9da9-5a7ffbd77b40";
        static constexpr uint32_t GameSignature = 0xE53FF221u;
        static constexpr uint32_t TelemetrySignature = 0xEC368575u;

        static constexpr uint16_t LayoutMajorVersion = 1;
        static constexpr uint16_t LayoutMinorVersion = 0;

        static constexpr int ExpectedPacketLength = 188;

        static constexpr const char* DefaultHost = "127.0.0.1";
        static constexpr uint16_t DefaultPort = 20778;
    };
    #pragma pack(push, 1)
    struct SHTelemetryPacket
    {
        // Header
        // Game signature : Stable identity of the game. Must match the definition GameSignature.
        uint32_t GameSignature;

        // Telemetry signature : Schema/protocol identity (includes game identity and telemetry layout). Must match the expected signature.
        uint32_t TelemetrySignature;

        // Major layout version : Increment at every breaking change (Fields removal, fields reordering).
        uint16_t LayoutMajorVersion;

        // Minor layout version : Increment at every minor change (Only fields append).
        uint16_t LayoutMinorVersion;

        // Emitter instance ID : Unique identifier of the telemetry emitter instance. Must be generated once at emitter startup and remain constant for the lifetime of the process or telemetry session. Used to distinguish multiple concurrent emitters and avoid mixing data streams. Should be non-zero when possible.
        uint64_t EmitterInstanceId;

        // Packet type Id : Packet type identifier. Currently always 0. Reserved for future extension (e.g. multiple packet types or partial updates).
        uint8_t PacketId;

        // Packet counter : Monotonic increasing counter incremented for every packet sent. Must not reset during a session. Used to detect packet loss, duplication, or reordering. Packets should continue to be emitted even when the game is paused.
        uint64_t PacketsCounter;

        // Session running flag : 1 if a session is currently active, 0 otherwise.
        uint8_t IsSessionRunning;

        // Session paused flag : 1 if the current session is paused, 0 otherwise.
        uint8_t IsSessionPaused;

        // Session unique ID : Unique identifier of the current session. Changes when a new session starts or restarts. Can be a random 64-bit integer.
        uint64_t SessionId;

        // Replay mode flag : 1 if the telemetry is coming from replayed data rather than live gameplay. When set, consumers may choose to disable real-time features such as motion or haptics, but all telemetry fields should otherwise remain valid and consistent with normal playback.
        uint8_t IsReplay;

        // User in control flag : 1 if the user is currently in control, 0 otherwise.
        uint8_t IsUserInControl;

        // AI in control flag : 1 if the AI is currently in control of the user's vehicle. Can be combined with IsUserInControl during assisted drivings (pit entry, pit exit etc ...)
        uint8_t IsAIInControl;

        // Spectator mode flag : 1 if the user is spectating rather than actively participating. Telemetry remains valid, but may not represent a player-controlled vehicle.
        uint8_t IsSpectator;

        // Session time (seconds) : Accurate session time in seconds. Intended for physics computations and precise timing.  May reset when SessionId changes.
        double SessionTimeSeconds;

        // Physics discontinuity counter : Incremented once each time a discontinuity occurs where physics continuity may be broken (teleportation, AI control switch, replay rewind/seek, etc.). Used to detect abrupt changes and allow consumers (e.g. motion systems) to reset or smooth transitions. Must remain constant otherwise. May increment together with a SessionId change.
        uint32_t PhysicsDiscontinuityCounter;


        // Fields
        // Yaw (degrees) : Vehicle heading relative to the world frame. 0 = north, positive clockwise (90 = east). Must align with VehiclePositionNorth/East
        float YawDegrees;

        // Pitch (degrees) : Vehicle pitch angle. Positive when the nose moves downward (driver perspective)
        float PitchDegrees;

        // Roll (degrees) : Vehicle roll angle. Positive when the vehicle tilts to the right (driver perspective)
        float RollDegrees;

        // Yaw rate (degrees/s) : Rate of change of vehicle heading. Positive for clockwise rotation (turning right). Must be continuous and not affected by angle wrapping
        float YawRateDegreesPerSecond;

        // Pitch rate (degrees/s) : Rate of change of vehicle pitch. Positive when the nose moves downward
        float PitchRateDegreesPerSecond;

        // Roll rate (degrees/s) : Rate of change of vehicle roll. Positive when the vehicle tilts to the right (driver perspective)
        float RollRateDegreesPerSecond;

        // Local surge acceleration (m/s²) : Relative to vehicle local orientation. Positive is acceleration, negative deceleration
        float LocalSurgeMs2;

        // Local sway acceleration (m/s²) : Relative to vehicle local orientation. Positive is an acceleration to the right, negative to the left
        float LocalSwayMs2;

        // Local heave acceleration (m/s²) : Relative to vehicle local orientation. Positive is an up acceleration, negative down, do not include gravity
        float LocalHeaveMs2;

        // Local forward velocity (m/s) : Relative to vehicle local orientation. Positive is forward
        float LocalVelocityForwardMps;

        // Local lateral velocity (m/s) : Relative to vehicle local orientation. Positive is right
        float LocalVelocityLateralMps;

        // Local up velocity (m/s) : Relative to vehicle local orientation. Positive is up
        float LocalVelocityUpMps;

        // Vehicle position north (m) : World position along north-south axis (positive toward north)
        double VehiclePositionNorth;

        // Vehicle position east (m) : World position along east-west axis (positive toward east)
        double VehiclePositionEast;

        // Vehicle position up (m) : World vertical position (positive upward)
        double VehiclePositionUp;

        // Vehicle speed (km/h) : Vehicle speed in km/h (as reported by the HUD including wheels spin etc ...)
        float SpeedKmh;

        // Ground speed (km/h) : Ground speed derived from physics (Real ground velocity)
        float GroundSpeedKmh;

        // Engine RPM : Current engine revolutions per minute
        float EngineRpm;

        // Throttle input : Throttle pedal position (0-1)
        float Throttle;

        // Brake input : Brake pedal position (0-1)
        float Brake;

        // TimestampMS : Game tick count timestamp in milliseconds
        uint32_t TimestampMS;

        // DeltaTimeSeconds : Physics delta time in seconds
        float DeltaTimeSeconds;

        // SpeedMps : Vehicle speed in meters per second
        float SpeedMps;

        // SpeedMph : Vehicle speed in miles per hour
        float SpeedMph;

        // EngineRpmRatio : Current RPM divided by engine max RPM, 0-1
        float EngineRpmRatio;

        // Steering : Steering input, -1 left to +1 right
        float Steering;

        // GForceLateral : Local lateral acceleration in G
        float GForceLateral;

        // GForceVertical : Local vertical acceleration in G
        float GForceVertical;

        // GForceLongitudinal : Local longitudinal acceleration in G
        float GForceLongitudinal;

        // BoostActive : Boost currently active flag, 0 or 1
        uint8_t BoostActive;

        // BoostStarted : One-packet boost started event flag
        uint8_t BoostStarted;

        // BoostEnded : One-packet boost ended event flag
        uint8_t BoostEnded;

        // Airborne : VehicleState.IsInAir flag, 0 or 1
        uint8_t Airborne;

        // LandingEvent : One-packet landing event flag
        uint8_t LandingEvent;

    };
    #pragma pack(pop)
    static_assert(sizeof(SHTelemetryPacket) == SHTelemetryConstants::ExpectedPacketLength,
                  "SHTelemetryPacket size mismatch: packing/layout is wrong");
}
