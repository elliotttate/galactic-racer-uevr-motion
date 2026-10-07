# Galactic Racer UEVR motion telemetry

Native x64 [UEVR](https://github.com/praydog/UEVR) plugin that exports Galactic
Racer vehicle physics to SimHub, SimTools, and Windows shared memory for motion
rigs. Based on the telemetry approach used for Fox3.

Validated against the October 7, 2026 game build and a UEVR backend exposing
plugin API 2.34.0. The game and UEVR are installed separately.

## Install the prebuilt mod

1. Download `GalacticRacer-UEVR-Motion-v1.0.0.zip` from the
   [latest release](https://github.com/elliotttate/galactic-racer-uevr-motion/releases/latest).
2. Extract the archive and exit Galactic Racer.
3. Open PowerShell in the extracted folder and run `.\Install.ps1`.
4. Launch the game with UEVR injected, then configure your telemetry receiver below.

## Use

The installer puts `GalacticRacerMotion.dll` in
`%APPDATA%\UnrealVRMod\SWGR-Win64-Shipping\plugins`.
It loads automatically when UEVR injects into the game. The existing cockpit
camera script works alongside it.

| Output | Default destination | Configuration |
| --- | --- | --- |
| SimHub UDPConnector JSON | `127.0.0.1:20777` | `SimHub*` |
| SimHub External Sim binary | `127.0.0.1:20778`, 188 bytes | `SimHubExternal*` |
| SimTools v3 generic UDP | `127.0.0.1:4123` | `SimTools*` |
| Windows shared memory | `Local\GalacticRacerTelemetryV1` | `SharedMemoryEnable` |

For SimHub's motion addon, select **STAR WARS Galactic Racer (UEVR)** from
SimHub's game list and enable it. `Install.ps1` registers the definition through
its own `.simlink`; it has a different game identity from FlatOut.
The definition is installed at
`%APPDATA%\UnrealVRMod\SWGR-Win64-Shipping\simhub\GalacticRacer.simdef`.
SimHub External Sim requires 9.11.5 or later; see the
[official integration documentation](https://manual.simhubdash.com/external-sim-integration).

For SimTools, use the same v3 generic UDP game engine format as Fox3 on port
4123. The packet is:

```text
S~Roll:...:Pitch:...:Yaw:...:RollSpeed:...:PitchSpeed:...:YawSpeed:...:Vertical:...:Lateral:...:Longitudinal:...~E
```

For the legacy SimHub JSON route, configure the UDPConnector receiver for
20777. These JSON packets expose the motion properties; they do not themselves
register a game with SimHub. The binary route above provides native game
integration.

Edit `%APPDATA%\UnrealVRMod\SWGR-Win64-Shipping\galactic_racer_motion.cfg`,
then restart the game. IPs, ports, feed switches, send rate caps, acceleration
filtering, motion limits, and each axis sign are configurable. `Enable = 0`
disables the plugin's telemetry output on the next launch.

## Data and units

The source is the local player's `Griffin.VehiclePawn.VehicleState`, read on the
game thread after the engine tick. Property offsets are resolved through UEVR
reflection. Camera motion, HMD tracking, and cockpit animation do not feed the
motion channels.

- Unreal world position and velocity are converted from centimetres to metres.
- `LocalVelocityX` / `LocalAccelerationX`: lateral, positive right.
- `LocalVelocityY` / `LocalAccelerationY`: vertical, positive up.
- `LocalVelocityZ` / `LocalAccelerationZ`: longitudinal, positive forward.
- Acceleration is m/s², without added gravitational acceleration. G-force is
  acceleration divided by 9.80665.
- JSON/shared-memory angles and angular speeds are radians and radians/sec;
  separate degree copies are provided. SimTools uses degrees and degrees/sec.
- Pitch is positive nose-up in JSON/SimTools. The SimHub binary standard uses
  positive nose-down, so the binary converter flips pitch and its rate.
- Roll is positive right-side-down. Yaw is positive toward world +Y; north is
  world +X and east is +Y. World positions in the binary packet match that frame.
- Throttle, brake, steering, RPM, RPM ratio, boost transitions, airborne state,
  landing transitions, ramjet heat, archetype, and input mode come from game data.

World velocity is differentiated before acceleration is rotated into vehicle
axes, preserving centripetal acceleration when local forward speed is constant.
The default acceleration low-pass cutoff is 20 Hz; each component is limited
to 10 G, and angular speeds to 720 degrees/sec. These are telemetry limits;
rig travel and motion gains are controlled by the receiving middleware.

Output is neutral during pause, menus, cinematics, AI control, invalid vehicle
state, and the first sample after acquisition, vehicle changes, teleports, or
long frame gaps. Each reset increments `PhysicsDiscontinuityCounter`.
A sender thread emits inactive heartbeats after 250 ms without an engine tick.
It never accesses Unreal objects.

Configured rates are caps. New motion data is limited by actual engine ticks;
the plugin does not duplicate active samples to claim a higher physics rate.
Run at 60 FPS or higher for SimHub's documented minimum motion update rate.
Boost/landing edge flags last one source sample and can be missed by a slower
UDP consumer; `BoostActive` and `Airborne` remain continuous state channels.

Hover vehicles do not provide Fox3's wheel suspension, tyre slip, gearbox, or
engine torque channels here. Those fields are omitted from the SimHub definition.
Race timing and lap fields are not exported by this version.

## Shared memory

The public ABI is in `src/Telemetry.hpp`: a 4-byte `WriteSequence` followed by
a packed 216-byte `Frame`, total 220 bytes. Readers must read the sequence,
copy the frame, and read the sequence again. Accept only matching even values.
Check `Magic = 0x31545247` (`GRT1`), `Version = 1`, `HeaderSize = 48`, and
`StructSize = 216`. Consumers should honor `FrameValid`, `OutputActive`, and
`Stale`, and also implement their own timeout if the game process exits.

## Build and installation

With Visual Studio C++ and its CMake component installed:

```powershell
.\Build.ps1
.\Install.ps1
```

The C++ runtime is linked statically. UEVR plugin API 2.34.0 or later is required.
The vendored UEVR SDK headers retain their original MIT notices.
When replacing an already loaded DLL, exit the game before running the installer.
The installer preserves existing telemetry configuration and backs up a replaced
plugin DLL. To remove the mod, exit the game and remove its DLL from the profile's
`plugins` folder.

`simhub/GalacticRacer.generated.cpp` is the authoritative SimHub 9.11.12 generated
sender reference. `src/SimHubGenerated.hpp` extracts its constants and packed
struct unchanged. The plugin fills that exact layout:

```text
UniqueId: 84171ddf-ddac-4be5-9da9-5a7ffbd77b40
GameSignature: 0xE53FF221
TelemetrySignature: 0xEC368575
Layout: 1.0
Packet length: 188
```

For definition changes, regenerate with Windows PowerShell 5.1 and an installed
SimHub directory, then rebuild:

```powershell
powershell.exe -File tools\Generate-SimHub.ps1 -SimHubDirectory 'C:\Program Files (x86)\SimHub'
.\Build.ps1
```

To create a distributable ZIP after building:

```powershell
.\Package.ps1
```

The package includes the DLL, source, installer, default configuration, SimHub
integration, tools, tests, and validation summaries.

## Verification

```powershell
python tools\Listen.py --seconds 10 --external-port 20778 --output validation\capture.json
```

Close other receivers on those ports before using the listener. It validates
JSON finiteness and inactive cues, SimTools framing, the binary packet's signatures,
size and counters, and an independent sequence-lock copy of shared memory.

`galactic_racer_motion_status.json` in the UEVR profile reports sample counts,
packet counts, UDP errors, and the most recent frame once per second.

Validation completed on October 7, 2026:

- Release x64 build and CTest passed: acceleration axes, constant-speed
  cornering, angle/rate signs, units, pause/resume, vehicle changes, teleports,
  long gaps, invalid data, boost/landing edges, and SimHub layout conversion.
- Live race capture: 159 active JSON frames and 60 paused frames; changing
  vehicle velocity/orientation and acceleration returned to zero after pause.
- Final installed build: 206 JSON, 205 SimTools, and 205 correctly signed
  188-byte SimHub packets in a 4-second menu capture, plus valid shared memory.
- No UDP send errors were reported in the live captures.

Representative capture summaries are in `docs/validation/`. New captures and build
logs can be saved locally in `validation/`. A full SimHub UI/motion rig hardware
test was not performed. The live sender-thread stall probe was interrupted by a
game restart; watchdog neutralization is covered by the code and unit checks.
