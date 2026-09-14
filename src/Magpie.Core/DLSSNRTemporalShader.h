#pragma once
#include <string_view>

namespace Magpie {
inline constexpr std::string_view DLSSNR_TEMPORAL_SHADER = R"hlsl(
Texture2D<float4> Input : register(t0);
Texture2D<float4> Base : register(t1);
Texture2D<float4> Raw : register(t2);
Texture2D<float4> History : register(t3);
Texture2D<float4> PreviousGuide : register(t4);
Texture2D<float2> Motion : register(t5);
RWTexture2D<float4> Output : register(u0);
RWTexture2D<float4> NextHistory : register(u1);
RWTexture2D<float4> NextGuide : register(u2);
SamplerState LinearClamp : register(s0);
cbuffer Settings : register(b0) {
    uint2 Size; uint UseMotion; uint Hdr;
    float HistoryWeight; uint3 Padding;
    uint4 Region; // x, y, exclusive right, exclusive bottom
};
float3 Guide(float3 c) { return Hdr ? c / (1 + abs(c)) : c; }
bool Inside(int2 p) { return all(p >= int2(Region.xy)) && all(p < int2(Region.zw)); }
bool PreviousPosition(int2 p, out float2 previous) {
    previous = p;
    if (!Inside(p)) return false;
    if (UseMotion) {
        float2 mv = Motion.Load(int3(p, 0));
        if (!all(isfinite(mv))) return false;
        previous += mv;
    }
    // Validate the entire bilinear footprint before a history/guide read.
    return all(previous >= float2(Region.xy)) &&
        all(previous <= float2(Region.zw) - 1);
}
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID) {
    int2 p = id.xy;
    if (any(id.xy >= Size)) return;
    float4 original = Input.Load(int3(p,0));
    float4 base = Base.Load(int3(p,0));
    float4 raw = Raw.Load(int3(p,0));
    bool finiteInput = all(isfinite(original));
    bool finiteCurrent = finiteInput && all(isfinite(base)) && all(isfinite(raw));
    NextGuide[p] = finiteInput ? float4(Guide(original.rgb),1) : 0;
    if (!finiteCurrent) {
        NextHistory[p] = 0;
        Output[p] = all(isfinite(base)) ? base : (finiteInput ? original : float4(0,0,0,1));
        return;
    }
    float3 current = raw.rgb - base.rgb;
    float3 result = current;
    float2 previous;
    if (HistoryWeight > 0 && PreviousPosition(p, previous)) {
        float error = 0;
        float maximumError = 0;
        bool valid = true;
        float3 lo = current, hi = current;
        [unroll] for (int y=-1; y<=1; ++y) {
            [unroll] for (int x=-1; x<=1; ++x) {
                int2 n = p + int2(x,y);
                float2 previousN;
                if (!PreviousPosition(n, previousN)) { valid = false; continue; }
                // Reject patch support crossing a motion discontinuity.
                if (UseMotion && any(abs((previousN-n)-(previous-p)) > 2)) { valid = false; continue; }
                float4 inputN = Input.Load(int3(n,0));
                float4 old = PreviousGuide.SampleLevel(LinearClamp, (previousN+.5)/Size, 0);
                float3 residualN = Raw.Load(int3(n,0)).rgb - Base.Load(int3(n,0)).rgb;
                if (!all(isfinite(inputN)) || !all(isfinite(old)) || old.a < .999 ||
                    !all(isfinite(residualN))) { valid = false; continue; }
                float3 delta = abs(Guide(inputN.rgb) - old.rgb);
                float e = max(delta.r, max(delta.g, delta.b));
                error += e / 9;
                maximumError = max(maximumError, e);
                lo = min(lo, residualN); hi = max(hi, residualN);
            }
        }
        // Raw-color patch matching is the initial B.1 baseline: no residual blur.
        float q = (1-smoothstep(.008, .04, error)) *
                  (1-smoothstep(.025, .10, maximumError));
        if (valid && q > 0) {
            float4 old = History.SampleLevel(LinearClamp, (previous+.5)/Size, 0);
            if (all(isfinite(old)) && old.a >= .999) {
                // Include the center and permit a stable correction to decay even
                // when the current residual vanishes. A tight variance box would
                // erase the very on/off history this filter needs to stabilize.
                float3 margin = .02 + q * abs(old.rgb);
                float3 safe = clamp(old.rgb, lo-margin, hi+margin);
                result = lerp(current, safe, HistoryWeight*q);
            }
        }
    }
    // FP16 history is signed; its alpha marks a valid observation, not opacity.
    NextHistory[p] = float4(clamp(result,-65504,65504),1);
    float3 color = base.rgb + result;
    Output[p] = float4(Hdr ? clamp(color,-65504,65504) : saturate(color), raw.a);
}
)hlsl";
}
