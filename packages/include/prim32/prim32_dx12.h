

















#pragma once
#include <d3d12.h>
#include <prim32/prim32.h>

namespace prim32_dx12 {

struct InitDesc {
    ID3D12Device*       device;
    ID3D12CommandQueue* queue;          
    DXGI_FORMAT         rtvFormat;      
    uint32_t            framesInFlight; 
    uint32_t            maxPrims;       
    uint32_t            maxTextures;    
};

struct GpuStats {
    float    gpuMs;          
    uint64_t bytesStreamed;  
};

PRIM32_API bool           Init(prim32::Context* ctx, const InitDesc& desc);
PRIM32_API void           Shutdown();
PRIM32_API prim32::FrameMem NewFrame();
PRIM32_API void           Render(prim32::DrawData* dd, ID3D12GraphicsCommandList* cmd);
PRIM32_API void           NotifyExecuted();
PRIM32_API uint32_t       RegisterTexture(ID3D12Resource* tex, DXGI_FORMAT srvFormat);



PRIM32_API void           GpuZoneBegin(ID3D12GraphicsCommandList* cmd, const char* name);
PRIM32_API void           GpuZoneEnd(ID3D12GraphicsCommandList* cmd);
PRIM32_API GpuStats       GetGpuStats();

} 
