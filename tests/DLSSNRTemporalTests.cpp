#define NOMINMAX
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <DirectXPackedVector.h>
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>
#include "DLSSNRTemporalShader.h"
#include "DLSSNRTemporalState.h"
using Microsoft::WRL::ComPtr;
using namespace Magpie;
constexpr unsigned W = 16, H = 16;
using Pixel = std::array<float,4>;
using Image = std::vector<Pixel>;
void Check(HRESULT hr) { if (FAILED(hr)) { std::cerr << "HRESULT " << std::hex << hr << '\n'; std::abort(); } }
struct Texture {
	ComPtr<ID3D11Texture2D> texture;
	ComPtr<ID3D11ShaderResourceView> srv;
	ComPtr<ID3D11UnorderedAccessView> uav;
};
struct Harness {
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> dc;
	ComPtr<ID3D11ComputeShader> shader;
	ComPtr<ID3D11SamplerState> sampler;
	ComPtr<ID3D11Buffer> cb;
	std::array<Texture,6> inputs;
	std::array<Texture,3> outputs;
	struct Constants {
		unsigned w = W, h = H, motion = 0, hdr = 0;
		float weight = .8f; unsigned padding[3]{};
		unsigned left = 0, top = 0, right = W, bottom = H;
	} constants;
	Harness() {
		Check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&dc));
		ComPtr<ID3DBlob> blob, errors;
		auto hr = D3DCompile(DLSSNR_TEMPORAL_SHADER.data(),DLSSNR_TEMPORAL_SHADER.size(),"DLSSNRTemporal",nullptr,nullptr,
			"main","cs_5_0",D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_WARNINGS_ARE_ERRORS|D3DCOMPILE_ALL_RESOURCES_BOUND|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&errors);
		if (errors) std::cerr << static_cast<const char*>(errors->GetBufferPointer());
		Check(hr);
		Check(device->CreateComputeShader(blob->GetBufferPointer(),blob->GetBufferSize(),nullptr,&shader));
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width=W; desc.Height=H; desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;
		desc.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;
		desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS;
		for (auto* list : {&inputs}) for (auto& t : *list) Create(t,desc);
		Create(outputs[0],desc);
		desc.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;
		Create(outputs[1],desc); Create(outputs[2],desc);
		D3D11_BUFFER_DESC buffer{}; buffer.ByteWidth=sizeof(constants); buffer.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
		Check(device->CreateBuffer(&buffer,nullptr,&cb));
		D3D11_SAMPLER_DESC s{}; s.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		s.AddressU=s.AddressV=s.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP; s.MaxLOD=D3D11_FLOAT32_MAX;
		Check(device->CreateSamplerState(&s,&sampler));
	}
	void Create(Texture& t,const D3D11_TEXTURE2D_DESC& desc) {
		Check(device->CreateTexture2D(&desc,nullptr,&t.texture));
		Check(device->CreateShaderResourceView(t.texture.Get(),nullptr,&t.srv));
		Check(device->CreateUnorderedAccessView(t.texture.Get(),nullptr,&t.uav));
	}
	void Set(unsigned i,const Image& image) { dc->UpdateSubresource(inputs[i].texture.Get(),0,nullptr,image.data(),W*sizeof(Pixel),0); }
	void Constant(unsigned i,Pixel p) { Set(i,Image(W*H,p)); }
	void Default() {
		constants={};
		Constant(0,{.4f,.4f,.4f,1}); Constant(1,{.4f,.4f,.4f,1}); Constant(2,{.5f,.5f,.5f,.7f});
		Constant(3,{.3f,.3f,.3f,1}); Constant(4,{.4f,.4f,.4f,1}); Constant(5,{0,0,0,0});
	}
	Image Run(unsigned result = 0) {
		dc->UpdateSubresource(cb.Get(),0,nullptr,&constants,0,0);
		ID3D11ShaderResourceView* srvs[6]; for (unsigned i=0;i<6;++i) srvs[i]=inputs[i].srv.Get();
		ID3D11UnorderedAccessView* uavs[3]; for (unsigned i=0;i<3;++i) uavs[i]=outputs[i].uav.Get();
		auto* buffer=cb.Get(); auto* s=sampler.Get();
		dc->CSSetShader(shader.Get(),nullptr,0); dc->CSSetShaderResources(0,6,srvs);
		dc->CSSetUnorderedAccessViews(0,3,uavs,nullptr); dc->CSSetConstantBuffers(0,1,&buffer); dc->CSSetSamplers(0,1,&s);
		dc->Dispatch(2,2,1);
		ID3D11ShaderResourceView* ns[6]{}; ID3D11UnorderedAccessView* nu[3]{};
		dc->CSSetShaderResources(0,6,ns); dc->CSSetUnorderedAccessViews(0,3,nu,nullptr);
		D3D11_TEXTURE2D_DESC desc{}; outputs[result].texture->GetDesc(&desc);
		desc.Usage=D3D11_USAGE_STAGING; desc.BindFlags=0; desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
		ComPtr<ID3D11Texture2D> staging; Check(device->CreateTexture2D(&desc,nullptr,&staging));
		dc->CopyResource(staging.Get(),outputs[result].texture.Get());
		D3D11_MAPPED_SUBRESOURCE mapped{}; Check(dc->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped));
		Image image(W*H);
		for (unsigned y=0;y<H;++y) {
			const auto* row=static_cast<const char*>(mapped.pData)+y*mapped.RowPitch;
			if (desc.Format==DXGI_FORMAT_R16G16B16A16_FLOAT) {
				const auto* half=reinterpret_cast<const DirectX::PackedVector::HALF*>(row);
				for (unsigned x=0;x<W;++x) for (unsigned c=0;c<4;++c)
					image[y*W+x][c]=DirectX::PackedVector::XMConvertHalfToFloat(half[x*4+c]);
			} else memcpy(image.data()+W*y,row,W*sizeof(Pixel));
		}
		dc->Unmap(staging.Get(),0); return image;
	}
};
void Near(float a,float b,float tolerance=1e-4f) { if (std::abs(a-b)>tolerance) { std::cerr<<"Expected "<<b<<", got "<<a<<'\n'; std::abort(); } }
int main() {
	DLSSNRTemporalState state;
	assert(state.Weight(1,0,0,1000000,false)==0);
	state.Commit(1,4,3,1000000);
	assert(state.Duplicate(1,4) && !state.Duplicate(1,5));
	assert(state.Weight(1,4,3,1166667,false)==0);
	assert(state.Weight(2,5,3,1166667,false)==0);
	assert(state.Weight(2,4,4,1166667,false)==0);
	assert(state.Weight(2,4,3,1000000,false)==0);
	assert(state.Weight(2,4,3,4000000,false)==0);
	assert(state.Weight(2,4,3,1166667,true)==0);
	Near(state.Weight(2,4,3,1166667,false),std::exp(-1.f/60/.08f));
	Near(static_cast<float>(std::pow(state.Weight(2,4,3,1166667,false),60)),
		static_cast<float>(std::pow(state.Weight(2,4,3,1333333,false),30)),1e-7f);
	Harness h;
	const unsigned center=8*W+8;
	h.Default(); auto out=h.Run(); Near(out[center][0],.66f); Near(out[center][3],.7f);
	// Stable negative residual and on/off disappearance remain signed and decay.
	h.Constant(3,{-.2f,-.2f,-.2f,1}); h.Constant(2,{.4f,.4f,.4f,.7f});
	Near(h.Run()[center][0],.24f);
	// Current input changes: reject old correction immediately.
	h.Constant(0,{.7f,.7f,.7f,1}); Near(h.Run()[center][0],.4f);
	// Reset ignores invalid old values, and nonfinite current values do not seed history.
	h.Default(); h.constants.weight=0;
	const float nan=std::numeric_limits<float>::quiet_NaN();
	h.Constant(3,{nan,nan,nan,1}); Near(h.Run()[center][0],.5f);
	h.Constant(2,{nan,nan,nan,1}); Near(h.Run()[center][0],.4f); Near(h.Run(1)[center][3],0);
	h.Default(); h.Constant(3,{nan,nan,nan,1}); Near(h.Run()[center][0],.5f);
	// Isolated correct correction must survive a 3x3 neighborhood box.
	h.Default(); Image residual(W*H,{0,0,0,1}), raw(W*H,{.4f,.4f,.4f,1});
	residual[center]={.4f,-.3f,.2f,1}; raw[center]={.8f,.1f,.6f,1};
	h.Set(3,residual); h.Set(2,raw); out=h.Run(); Near(out[center][0],.8f); Near(out[center][1],.1f);
	// Known backward subpixel displacement; B follows the history coordinate.
	h.Default(); h.constants.motion=1; h.Constant(5,{-.5f,0,0,0});
	for (unsigned y=0;y<H;++y) for (unsigned x=0;x<W;++x) residual[y*W+x]={float(x)*.01f,0,0,1};
	h.Set(3,residual); Near(h.Run()[center][0],.4f+.02f+.8f*.075f);
	// Invalid footprint and nonfinite optical flow reject before sampling.
	h.Constant(5,{100,0,0,0}); Near(h.Run()[center][0],.5f);
	h.Constant(5,{nan,0,0,0}); Near(h.Run()[center][0],.5f);
	h.Constant(5,{0,0,0,0}); h.constants.left=8; Near(h.Run()[center][0],.5f);
	// Wrong motion over textured input is rejected by color, even with finite vectors.
	h.Default(); h.constants.motion=1;
	Image guide(W*H), input(W*H);
	for (unsigned y=0;y<H;++y) for (unsigned x=0;x<W;++x) {
		guide[y*W+x]={float(x)*.04f,0,0,1}; input[y*W+x]={float(x-1)*.04f,0,0,1};
	}
	h.Set(0,input); h.Set(4,guide); h.Constant(5,{-1,0,0,0}); Near(h.Run()[center][0],.66f);
	h.Constant(5,{2,0,0,0}); Near(h.Run()[center][0],.5f);
	// HDR values are not clamped to SDR [0,1].
	h.Default(); h.constants.hdr=1; h.constants.weight=0;
	h.Constant(2,{2,-.1f,.5f,1}); out=h.Run(); Near(out[center][0],2); Near(out[center][1],-.1f);
	// A sustained alternating residual loses temporal variation without losing
	// its mean correction. Exercise the real shader's recurrent history output.
	h.Default(); h.Constant(3,{.15f,.15f,.15f,1});
	float sum=0, sumSquares=0;
	for (int frame=0;frame<80;++frame) {
		const float residualValue=.15f + (frame%2 ? .1f : -.1f);
		h.Constant(2,{.4f+residualValue,.4f+residualValue,.4f+residualValue,1});
		const Image history=h.Run(1); h.Set(3,history);
		if (frame>=40) { sum+=history[center][0]; sumSquares+=history[center][0]*history[center][0]; }
	}
	Near(sum/40,.15f,.00075f); // <=0.5% mean error after recurrent FP16 storage.
	assert(sumSquares/40-(sum/40)*(sum/40) < .0002f); // Raw variance is .01.
	std::cout<<"Temporal state and production HLSL WARP tests passed: signed EMA, on/off, changes, reset, NaN, detail, backward flow, bounds, wrong matches, HDR.\n";
}
