#include <windows.h>
#include <cassert>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include "../src/Magpie.Core/include/XeSSFGParameters.h"
#include "../src/Magpie.Core/include/OpticalFlowDefaults.h"
#include "../src/Magpie.Core/XeSSFGTiming.h"
#include "../src/Magpie.Core/XeSSFGCompatibility.h"

using namespace Magpie;
using namespace Magpie::XeSSFGCompatibility;
struct Effect { std::wstring name; std::map<std::wstring, float> parameters; int scale = 7; };
struct Mode { std::wstring name; std::vector<Effect> effects; };

static void Migration() {
    for (const auto* id : {L"XeSSFG\\XeSS_FrameGeneration_x2_ZeroMV", L"XeSSFG\\XeSS_MultiFrameGeneration_ZeroMV"}) {
        for (int method = 0; method < 3; ++method) for (int amd = 0; amd < 2; ++amd) for (int nv = 1; nv <= 5; ++nv) {
            std::vector<Mode> modes{{L"Custom", {{L"First"}, {id, {{L"multiplier",4.0f},
                {L"opticalFlowMethod",float(method)}, {L"amdOpticalFlowMode",float(amd)}, {L"nvidiaOpticalFlowQuality",float(nv)}}},
                {L"Last"}, {id}}}};
            assert(MigrateXeSSFGEffects(modes));
            uint32_t oldFlowVersion = 0;
            ApplyOpticalFlowDefaultsMigration(modes, oldFlowVersion);
            auto& e = modes[0].effects[1];
            assert(modes.size()==1 && modes[0].name==L"Custom" && modes[0].effects.size()==3);
            assert(e.name==XESS_FG_EFFECT && e.scale==7 && e.parameters[L"multiplier"]==2);
            assert(e.parameters[L"opticalFlowMethod"]==method && e.parameters[L"amdOpticalFlowMode"]==amd && e.parameters[L"nvidiaOpticalFlowQuality"]==nv);
            e.parameters[L"multiplier"] = 4;
            assert(!MigrateXeSSFGEffects(modes));
            assert(e.parameters[L"multiplier"]==4);
        }
        std::vector<Mode> missing{{L"Missing", {{id}}}};
        assert(MigrateXeSSFGEffects(missing));
        assert(missing[0].effects[0].parameters[L"opticalFlowMethod"]==0);
    }
    std::vector<Mode> fresh{{L"New", {{std::wstring(XESS_FG_EFFECT)}}}};
    assert(MigrateXeSSFGEffects(fresh));
    auto& p=fresh[0].effects[0].parameters;
    assert(p[L"multiplier"]==2 && p[L"opticalFlowMethod"]==1 && p[L"amdOpticalFlowMode"]==1);
    p[L"opticalFlowMethod"] = NAN;
    p[L"multiplier"] = 3.5f;
    assert(MigrateXeSSFGEffects(fresh));
    assert(p[L"opticalFlowMethod"]==1 && p[L"multiplier"]==2);
}
struct FaultBackend {
    std::array<uint8_t, 8> memory{};
    int reject = -1, fail = -1, rollbackFail = -1, writes = 0;
    bool Preflight(Patch& p) noexcept { return int(p.rva)!=reject && memory[p.rva]==p.original[0]; }
    bool Write(Patch& p, bool restore) noexcept {
        ++writes;
        memory[p.rva] = restore ? p.original[0] : p.replacement[0];
        return int(p.rva)!=(restore?rollbackFail:fail);
    }
};
static void Transactions() {
    std::array<Patch,8> patches{};
    for(uint32_t i=0;i<8;++i) { patches[i].rva=i; patches[i].size=1; patches[i].replacement[0]=9; }
    for(int i=0;i<8;++i) {
        FaultBackend rejected; rejected.reject=i;
        assert(InstallPatches(patches,rejected)==PatchResult::Rejected && rejected.writes==0);
        FaultBackend failed; failed.fail=i;
        assert(InstallPatches(patches,failed)==PatchResult::RolledBack);
        for(auto v:failed.memory) assert(v==0);
        failed.rollbackFail=i;
        assert(InstallPatches(patches,failed)==PatchResult::Poisoned);
    }
    FaultBackend ok;
    assert(InstallPatches(patches,ok)==PatchResult::Success);
    assert(RestorePatches(patches,ok));
    for(auto v:ok.memory) assert(v==0);
}
static void Timing() {
    XeSSFGTiming clock;
    assert(clock.Submit({1,1,1,100000},100,0).reset);
    auto x=clock.Submit({2,2,1,300000},160,40);
    assert(x.captured && x.sourceMs==20 && x.fedMs==20); // independent capture ignores Present wait
    x=clock.Submit({3,4,1,700000},220,40);
    assert(!x.captured && x.fedMs==20); // skipped capture uses attributed submit fallback
    assert(clock.Submit({4,5,2,900000},240,0).reset);
    assert(clock.Submit({5,6,2,800000},260,0).reset); // time goes backwards
    assert(clock.Submit({6,7,2,1000000},1000,0).reset);
    clock.Reset();
    assert(clock.Submit({1,1,1,0},100,0).reset);
    assert(clock.Submit({2,2,1,0},150,30).fedMs==20);
    assert(clock.Submit({2,2,1,0},170,0).reset); // repeated source identity
}
static void* NativeTimestamp(void*, int64_t* out, void*, void*, uint32_t, uint32_t) { *out=100000000; return out; }
static void Deadlines() {
    Pacing::g_enabled = true;
    Pacing::g_tsNative=&NativeTimestamp;
    QueryPerformanceFrequency(&Pacing::g_freq);
    std::array<uint8_t, 0x400> ctx{};
    float measured=4;
    memcpy(ctx.data()+Pacing::RingOffset+Pacing::RingMeasuredOffset,&measured,4);
    Pacing::EnterContext(ctx.data());
    int64_t timing[2]{9,20000000}, out=0;
    Pacing::TsDetour(nullptr,&out,nullptr,timing,1,4);
    assert(out==104000000); // restores 5 ms step versus native 1 ms clamp
    Pacing::TsDetour(nullptr,&out,nullptr,timing,2,4);
    assert(out==109000000);
    Pacing::sourcePeriodNs.store(16000000);
    timing[1]=200000000; // inflated SDK ring must not stretch the source cadence
    Pacing::TsDetour(nullptr,&out,nullptr,timing,1,4);
    assert(out==103000000);
    Pacing::TsDetour(nullptr,&out,nullptr,timing,2,4);
    assert(out==107000000);
    Pacing::sourcePeriodNs.store(0);
    Pacing::resetEpoch.fetch_add(1);
    Pacing::EnterContext(ctx.data());
    assert(Pacing::g_nextDeadlineNs==0 && Pacing::g_lastTsIndex==0);
    Pacing::g_enabled = false;
}
static void Runtime(const wchar_t* path) {
    HMODULE module=LoadLibraryW(path);
    assert(module && KnownFile(module));
    const auto* entry=reinterpret_cast<const void*>(GetProcAddress(module,"xefgSwapChainD3D12CreateContext"));
    ImageBackend backend{reinterpret_cast<uint8_t*>(module),0x015ED000};
    for(uint32_t multiplier:{3u,4u,3u,4u}) {
        Lease lease, duplicate;
        assert(lease.Acquire(true,multiplier,entry));
        assert(!duplicate.Acquire(false,2,entry));
        assert(lease.Release(true));
        auto original=MakePatches(multiplier-1);
        for(auto& p:original) assert(backend.Preflight(p));
        assert(duplicate.Acquire(false,2,entry));
        assert(duplicate.Release(true));
    }
    Lease unknown;
    assert(!unknown.Acquire(true,4,reinterpret_cast<const void*>(&Runtime)));
    assert(!unknown.Acquire(true,5,entry));
    Lease uncertain;
    assert(uncertain.Acquire(false,2,entry));
    assert(!uncertain.Release(false));
    assert(!unknown.Acquire(false,2,entry));
    FreeLibrary(module);
}
int wmain(int argc,wchar_t** argv) {
    Migration(); Transactions(); Timing(); Deadlines();
    if(argc>1) Runtime(argv[1]);
    std::cout<<"XeSSFG migration, rollback faults, timing, deadlines and requested runtime checks passed\n";
}
