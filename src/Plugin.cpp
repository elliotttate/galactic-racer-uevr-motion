#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include "uevr/Plugin.hpp"
#include "Telemetry.hpp"
#include "SimHub.hpp"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

using API=uevr::API;
using Object=API::UObject;
namespace {
bool copy_memory(const void* src,void* dst,size_t size) {
    if(!src) return false;
    __try { std::memcpy(dst,src,size);return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
template<class T> bool read(Object* o,const wchar_t* name,T& result) {
    return o&&copy_memory(o->get_property_data(name),&result,sizeof(T));
}
// Property existence and type are checked before optional reads. Required data
// never falls back to an actor or camera transform.
bool boolean(Object* o,const wchar_t* name,bool fallback=false) {
    if(!o||!o->get_class()->find_property(name)) return fallback;
    return o->get_bool_property(name);
}
float number(Object* o,const wchar_t* name) {
    float v{};read(o,name,v);return std::isfinite(v)?v:0;
}
bool getter(Object* o,const wchar_t* name,bool fallback=false) {
    if(!o) return fallback;
    auto fn=o->get_class()->find_function(name);
    if(!fn||fn->get_properties_size()>256) return fallback;
    auto ret=fn->find_property(L"ReturnValue");
    if(!ret||ret->get_class()->get_name()!=L"BoolProperty") return fallback;
    std::array<uint8_t,256> params{};
    o->process_event(fn,params.data());
    return reinterpret_cast<API::FBoolProperty*>(ret)->get_value_from_object(params.data());
}
bool paused(Object* context) {
    auto cls=API::get()->find_uobject<API::UClass>(L"Class /Script/Engine.GameplayStatics");
    if(!cls) return true; // fail closed until the engine is ready
    auto fn=cls->find_function(L"IsGamePaused");
    if(!fn||fn->get_properties_size()>256) return true;
    auto arg=fn->find_property(L"WorldContextObject"),ret=fn->find_property(L"ReturnValue");
    if(!arg||!ret) return true;
    std::array<uint8_t,256> params{};
    std::memcpy(params.data()+arg->get_offset(),&context,sizeof(context));
    cls->get_class_default_object()->process_event(fn,params.data());
    return reinterpret_cast<API::FBoolProperty*>(ret)->get_value_from_object(params.data());
}
struct Transform { gr::Quat rotation;gr::Vec3 translation;double pad;gr::Vec3 scale;double pad2; };
static_assert(sizeof(Transform)==96);
bool sample_game(gr::Sample* out,float dt) {
    auto pawn=API::get()->get_local_pawn(0);
    auto cls=API::get()->find_uobject<API::UClass>(L"Class /Script/Griffin.VehiclePawn");
    if(!pawn||!cls||!pawn->is_a(cls)) return false;
    Object* state{};
    if(!read(pawn,L"VehicleState",state)||!state||!boolean(state,L"Valid")) return false;
    // Verify the UE5 double layouts before reading. Do not use fixed game offsets.
    for(const auto* name:{L"Velocity",L"AngularVelocity"}) {
        auto prop=state->get_class()->find_property(name);
        if(!prop||prop->get_class()->get_name()!=L"StructProperty"||
           reinterpret_cast<API::FStructProperty*>(prop)->get_struct()->get_properties_size()!=sizeof(gr::Vec3)) return false;
    }
    auto transformProp=state->get_class()->find_property(L"VehicleTransform");
    if(!transformProp||transformProp->get_class()->get_name()!=L"StructProperty"||
       reinterpret_cast<API::FStructProperty*>(transformProp)->get_struct()->get_properties_size()!=sizeof(Transform)) return false;
    Transform transform{};gr::Vec3 velocity{},angular{};
    if(!read(state,L"VehicleTransform",transform)||!read(state,L"Velocity",velocity)||!read(state,L"AngularVelocity",angular)) return false;
    out->identity=reinterpret_cast<uint64_t>(state);
    out->valid=true;out->dt=dt;
    out->paused=paused(pawn);
    out->cinematic=getter(pawn,L"IsUnderCinematicControl",true);
    uint8_t mode{},source{},archetype{},boost{},landing{};
    read(pawn,L"InputMode",mode);read(pawn,L"InputSource",source);
    read(state,L"VehicleArcehtype",archetype);read(state,L"BoostState",boost);read(state,L"LandingState",landing);
    // Enum values from the live Griffin SDK: Racing=3, PlayFromHere=4, Player=1.
    // The getter accounts for AI overrides after race finish or cinematics.
    auto fn=pawn->get_class()->find_function(L"GetCurrentInputSource");
    if(fn&&fn->get_properties_size()<=256) {
        auto ret=fn->find_property(L"ReturnValue");
        if(ret) {std::array<uint8_t,256> params{};pawn->process_event(fn,params.data());source=params[ret->get_offset()];}
    }
    out->controlled=(source==1)&&(mode==3||mode==4)&&getter(pawn,L"IsLocallyControlled",false);
    out->position=transform.translation*0.01;out->velocity=velocity*0.01;
    out->rotation=transform.rotation;out->angularVelocity=angular;
    out->throttle=number(state,L"Throttle");out->brake=number(state,L"Brake");out->steering=number(state,L"SteeringInput");
    out->rpm=number(state,L"RPM");out->rpmRatio=number(state,L"RPMScale");
    out->ramjetHeat=number(state,L"RamjetHeat");
    out->boost=getter(state,L"IsAfterburning")||getter(state,L"IsRamjetting");
    out->airborne=getter(state,L"IsInAir");
    out->vehicleArchetype=archetype;out->inputMode=mode;out->boostState=boost;out->landingState=landing;
    return true;
}
bool sample_guarded(gr::Sample* out,float dt) {
    __try {return sample_game(out,dt);}
    __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
struct Feed {
    bool enabled{true};std::string ip{"127.0.0.1"};int port{},hz{};
    SOCKET socket{INVALID_SOCKET};sockaddr_in address{};double nextDue{};uint64_t sent{},errors{};
    void open() {
        if(!enabled)return;
        address.sin_family=AF_INET;address.sin_port=htons(static_cast<u_short>(port));
        if(inet_pton(AF_INET,ip.c_str(),&address.sin_addr)!=1){enabled=false;API::get()->log_error("[GR Motion] Invalid IPv4 destination: %s",ip.c_str());return;}
        socket=::socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
        if(socket==INVALID_SOCKET){enabled=false;API::get()->log_error("[GR Motion] UDP socket failed: %d",WSAGetLastError());return;}
        u_long nonblocking=1;ioctlsocket(socket,FIONBIO,&nonblocking);
    }
    void send(const std::string& payload,uint64_t now) {
        if(!enabled||socket==INVALID_SOCKET||static_cast<double>(now)<nextDue)return;
        if(nextDue==0)nextDue=static_cast<double>(now);
        nextDue=std::max(nextDue+1000.0/hz,static_cast<double>(now));
        if(sendto(socket,payload.data(),static_cast<int>(payload.size()),0,reinterpret_cast<sockaddr*>(&address),sizeof(address))==SOCKET_ERROR) ++errors;
        else ++sent;
    }
    void close(){if(socket!=INVALID_SOCKET)closesocket(socket);socket=INVALID_SOCKET;}
};
std::map<std::string,std::string> config_read(const std::filesystem::path& path) {
    std::ifstream input(path);std::map<std::string,std::string> values;std::string line;
    auto trim=[](std::string s){auto a=s.find_first_not_of(" \t\r\n");auto b=s.find_last_not_of(" \t\r\n");return a==s.npos?std::string{}:s.substr(a,b-a+1);};
    while(std::getline(input,line)) {
        line=line.substr(0,line.find('#'));auto n=line.find('=');
        if(n!=line.npos)values[trim(line.substr(0,n))]=trim(line.substr(n+1));
    }return values;
}
double setting(const std::map<std::string,std::string>& v,const char* key,double fallback,double min,double max) {
    auto i=v.find(key);if(i==v.end())return fallback;
    try {size_t used{};double n=std::stod(i->second,&used);if(used!=i->second.size()||!std::isfinite(n)||n<min||n>max)throw 0;return n;}
    catch(...){API::get()->log_warn("[GR Motion] Invalid config %s; using default",key);return fallback;}
}
class GalacticRacerMotion final:public uevr::Plugin {
    std::filesystem::path profile;
    gr::Settings settings;gr::Sampler sampler;
    Feed hub{true,"127.0.0.1",20777,60},tools{true,"127.0.0.1",4123,100},external{true,"127.0.0.1",20778,120};
    std::atomic_bool enabled{true};bool sharedEnabled{true};
    HANDLE mapping{};gr::SharedMemory* shared{};
    std::mutex mutex;gr::Frame latest{};
    uint64_t tickTime{},lastStatus{};std::atomic_bool stop{false};
    uint64_t emitterId{},sessionId{};double sessionSeconds{};
    std::thread worker;
    std::atomic_bool initialized{false};
    std::atomic<uint64_t> samples{},validSamples{};
    void status(const gr::Frame& f) {
        // Write status once per second; disk I/O runs only on the sender thread.
        std::ofstream out(profile/L"galactic_racer_motion_status.json");
        out<<"{\"samples\":"<<samples.load()<<",\"validSamples\":"<<validSamples.load()
           <<",\"simHubPackets\":"<<hub.sent<<",\"simToolsPackets\":"<<tools.sent
           <<",\"simHubExternalPackets\":"<<external.sent
           <<",\"udpErrors\":"<<hub.errors+tools.errors+external.errors<<",\"sharedMemory\":"<<(shared?"true":"false")
           <<",\"frame\":"<<gr::json(f)<<"}";
    }
    void publish(gr::Frame f,uint64_t now,uint64_t session,double seconds) {
        f.TimestampMS=static_cast<uint32_t>(now);
        if(shared) {
            InterlockedIncrement(reinterpret_cast<volatile LONG*>(&shared->WriteSequence));
            MemoryBarrier();std::memcpy(&shared->Data,&f,sizeof(f));MemoryBarrier();
            InterlockedIncrement(reinterpret_cast<volatile LONG*>(&shared->WriteSequence));
        }
        if(hub.enabled)hub.send(gr::json(f),now);
        if(tools.enabled)tools.send(gr::simtools(f),now);
        if(external.enabled) {
            auto packet=gr::external_packet(f,emitterId,session,external.sent+1,seconds);
            external.send(std::string(reinterpret_cast<const char*>(&packet),sizeof(packet)),now);
        }
        if(now-lastStatus>=1000){status(f);lastStatus=now;}
    }
    void run() {
        uint32_t lastSequence{};uint64_t lastNeutral{};
        while(!stop) {
            uint64_t now=GetTickCount64(),tick{},session{};double seconds{};gr::Frame f{};
            {std::scoped_lock lock(mutex);f=latest;tick=tickTime;session=sessionId;seconds=sessionSeconds;}
            bool stale=!tick||now-tick>250;
            if(stale)f=gr::neutral(f,true);
            // Emit once per real physics sample. Never invent higher-rate frames.
            // On a frozen game thread, keep an explicitly inactive heartbeat.
            if((f.Sequence!=lastSequence)||((stale||!f.OutputActive)&&now-lastNeutral>=50)) {
                publish(f,now,session,seconds);lastSequence=f.Sequence;if(!f.OutputActive)lastNeutral=now;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
public:
    void stop_live() {
        enabled=false;stop=true;if(worker.joinable())worker.join();hub.close();tools.close();external.close();
        if(shared)UnmapViewOfFile(shared);if(mapping)CloseHandle(mapping);shared=nullptr;mapping=nullptr;
    }
    ~GalacticRacerMotion() override {stop_live();}
    void on_initialize() override {
        if(initialized.exchange(true))return;
        auto getDir=API::get()->param()->functions->get_persistent_dir;
        int n=getDir(nullptr,0);std::vector<wchar_t> buf(static_cast<size_t>(n)+2);getDir(buf.data(),static_cast<uint32_t>(buf.size()));profile=buf.data();
        auto cfg=config_read(profile/L"galactic_racer_motion.cfg");
        enabled=setting(cfg,"Enable",1,0,1)!=0;sharedEnabled=setting(cfg,"SharedMemoryEnable",1,0,1)!=0;
        auto feed=[&](Feed& f,const std::string& prefix){
            f.enabled=setting(cfg,(prefix+"Enable").c_str(),1,0,1)!=0;
            f.port=static_cast<int>(setting(cfg,(prefix+"Port").c_str(),f.port,1,65535));
            f.hz=static_cast<int>(setting(cfg,(prefix+"RateHz").c_str(),f.hz,1,240));
            if(cfg.contains(prefix+"Ip"))f.ip=cfg.at(prefix+"Ip");};
        feed(hub,"SimHub");feed(tools,"SimTools");feed(external,"SimHubExternal");
        settings.smoothingHz=setting(cfg,"AccelerationSmoothingHz",20,0,120);
        settings.maxAccelerationG=setting(cfg,"MaxAccelerationG",10,0.1,100);
        settings.maxAngularSpeedDeg=setting(cfg,"MaxAngularSpeedDeg",720,1,3600);
        settings.teleportDistanceM=setting(cfg,"TeleportDistanceM",10,1,1000);
        settings.maxDeltaSeconds=setting(cfg,"MaxDeltaSeconds",0.1,0.01,0.25);
        auto sign=[&](const char* name){double v=setting(cfg,name,1,-1,1);return v<0?-1.0:1.0;};
        settings.rollSign=sign("RollSign");settings.pitchSign=sign("PitchSign");settings.yawSign=sign("YawSign");
        settings.lateralSign=sign("LateralSign");settings.verticalSign=sign("VerticalSign");settings.longitudinalSign=sign("LongitudinalSign");
        latest.StructSize=sizeof(latest);
        if(!enabled){API::get()->log_info("[GR Motion] Disabled in configuration");return;}
        WSADATA wsa{};
        if(WSAStartup(MAKEWORD(2,2),&wsa)==0){hub.open();tools.open();external.open();}
        else {hub.enabled=false;tools.enabled=false;external.enabled=false;API::get()->log_error("[GR Motion] WSAStartup failed");}
        if(sharedEnabled) {
            mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(gr::SharedMemory),L"Local\\GalacticRacerTelemetryV1");
            if(mapping)shared=static_cast<gr::SharedMemory*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(gr::SharedMemory)));
            if(!shared)API::get()->log_error("[GR Motion] Shared memory unavailable: %lu",GetLastError());
        }
        emitterId=(GetTickCount64()<<32)^GetCurrentProcessId();sessionId=emitterId;
        worker=std::thread([this]{run();});
        API::get()->log_info("[GR Motion] v1.0.0 initialized; SimHub=%s:%d SimTools=%s:%d shared=%d",hub.ip.c_str(),hub.port,tools.ip.c_str(),tools.port,shared!=nullptr);
    }
    void on_post_engine_tick(API::UGameEngine*,float dt) override {
        if(!enabled||!initialized)return;
        gr::Sample s{};
        if(!sample_guarded(&s,dt))s={};
        s.timestampMs=GetTickCount64();++samples;if(s.valid)++validSamples;
        auto f=sampler.update(s,settings);
        {std::scoped_lock lock(mutex);
            if(s.identity&&sessionId!=(emitterId^s.identity)){sessionId=emitterId^s.identity;sessionSeconds=0;}
            if(s.valid&&!s.paused&&std::isfinite(dt)&&dt>0)sessionSeconds+=dt;
            latest=f;tickTime=s.timestampMs;
        }
    }
};
// Lifetime is the game process. UEVR has no safe callback removal for DLL unload.
auto plugin=std::make_unique<GalacticRacerMotion>();
}
// Allows the supplied dev helper to initialize this exact plugin in an already
// injected game without restarting the user's race. No standalone injection of UEVR.
extern "C" __declspec(dllexport) DWORD WINAPI SWGR_InitializeLive(void*) {
    auto backend=GetModuleHandleW(L"UEVRBackend.dll");
    auto param=backend?reinterpret_cast<UEVR_PluginInitializeParam*>(GetProcAddress(backend,"g_plugin_initialize_param")):nullptr;
    if(!param)return 1;
    return uevr_plugin_initialize(param)?0:2;
}
extern "C" __declspec(dllexport) DWORD WINAPI SWGR_StopLive(void*) {
    plugin->stop_live();return 0;
}
