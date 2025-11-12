# GPU Resource Management Best Practices

## Document Information
- **Created**: 2025-11-12
- **Status**: Active Standard
- **Category**: Coding Standards
- **Related**: Diligent Engine, Graphics Programming

## Problem Summary

### The Bug We Fixed

**Symptom**: Crash at line 777 in `DiligentRenderer.cpp` when implementing object picking functionality.

**Root Cause**: Dangling pointer issue caused by:
1. Using raw pointers (`ITextureView*`) instead of smart pointers for GPU resources
2. Not recreating resolution-dependent resources on window resize
3. Local `RefCntAutoPtr` variables releasing resources when going out of scope
4. Buffer overrun in `memcpy` (writing `sizeof(VSConstants)` instead of actual data size)

**Error Signature**:
```
Memory corruption: 0xFDFDFDFD (MSVC debug marker for "no-man's-land")
Wild pointer: 0xFFFFFFFFFFFFFFDF (freed memory being accessed)
```

### What Went Wrong

```cpp
// ❌ WRONG - Raw pointers lose reference, resource gets destroyed
ITextureView* m_pPickingRTV = nullptr;
ITextureView* m_pPickingDSV = nullptr;

void CreatePickingResources() {
    RefCntAutoPtr<ITexture> pTex;  // Local variable!
    m_pDevice->CreateTexture(..., &pTex);
    m_pPickingRTV = pTex->GetDefaultView(...);  // Raw pointer copy
    // When this function exits, pTex is destroyed
    // pTex's ref count goes to 0, texture is freed
    // m_pPickingRTV now points to freed memory!
}

void Resize(uint32_t w, uint32_t h) {
    m_pSwapChain->Resize(w, h);
    // ❌ Forgot to recreate picking RT/DSV
    // Old resources are destroyed but pointers still exist
}
```

### The Fix

```cpp
// ✅ CORRECT - Smart pointers maintain reference count
RefCntAutoPtr<ITextureView> m_pPickingRTV;
RefCntAutoPtr<ITextureView> m_pPickingDSV;

void CreatePickingRenderTargets() {
    RefCntAutoPtr<ITexture> pPickingTex;
    m_pDevice->CreateTexture(..., &pPickingTex);
    m_pPickingRTV = pPickingTex->GetDefaultView(...);  // Smart pointer assignment
    // m_pPickingRTV now holds a reference, texture won't be freed
}

void Resize(uint32_t w, uint32_t h) {
    m_pSwapChain->Resize(w, h);
    CreatePickingRenderTargets();  // ✅ Recreate resolution-dependent resources
}
```

---

## Mandatory Rules

### Rule 1: Never Use Raw Pointers for GPU Resources

**❌ FORBIDDEN:**
```cpp
ITexture* m_pTexture = nullptr;
IBuffer* m_pBuffer = nullptr;
ITextureView* m_pRTV = nullptr;
```

**✅ REQUIRED:**
```cpp
Diligent::RefCntAutoPtr<ITexture> m_pTexture;
Diligent::RefCntAutoPtr<IBuffer> m_pBuffer;
Diligent::RefCntAutoPtr<ITextureView> m_pRTV;
```

**Why**: Raw pointers don't maintain reference counts. When the underlying resource is destroyed, you get a dangling pointer that causes crashes when accessed.

**Exception**: Only use raw pointers for temporary function-local usage where you don't need to store the pointer.

---

### Rule 2: Recreate Resolution-Dependent Resources on Resize

**Required Pattern:**
```cpp
void Resize(uint32_t width, uint32_t height) {
    m_Width = width;
    m_Height = height;
    
    if (m_pSwapChain) {
        m_pSwapChain->Resize(m_Width, m_Height);
        m_pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
        m_pDSV = m_pSwapChain->GetDepthBufferDSV();
        
        // ✅ MANDATORY: Recreate ALL resolution-dependent resources
        RecreateResolutionDependentResources();
    }
}

void RecreateResolutionDependentResources() {
    // Recreate any texture that uses m_Width/m_Height
    CreatePickingRenderTargets();
    CreateShadowMaps();
    CreatePostProcessBuffers();
    // etc.
}
```

**Checklist**: Any resource created with `m_Width` or `m_Height` MUST be recreated on resize.

---

### Rule 3: Separate Static and Dynamic Resource Creation

**✅ CORRECT Pattern:**
```cpp
void Initialize() {
    CreateStaticResources();     // PSO, shaders, static buffers
    CreateDynamicResources();    // Resolution-dependent textures
}

void CreateStaticResources() {
    // Only create once, never recreated
    if (m_pPSO) return;  // Guard against double-creation
    
    CreateShaders();
    CreatePipelineState();
    CreateConstantBuffers();  // Size independent of window
}

void CreateDynamicResources() {
    // Recreated on every resize
    CreateRenderTargets();    // Size = m_Width x m_Height
    CreateDepthBuffers();     // Size = m_Width x m_Height
}

void Resize(uint32_t w, uint32_t h) {
    m_Width = w;
    m_Height = h;
    CreateDynamicResources();  // Only recreate what needs to change
}
```

---

### Rule 4: Always Use Actual Data Size in memcpy

**❌ DANGEROUS:**
```cpp
struct VSConstants {
    Moon::Matrix4x4 WorldViewProj;  // 64 bytes
};

void UpdateBuffer() {
    Moon::Matrix4x4 wvp = ...;
    void* pData;
    MapBuffer(..., pData);
    memcpy(pData, &wvp, sizeof(VSConstants));  // ❌ May overrun if struct has padding
}
```

**✅ SAFE:**
```cpp
void UpdateBuffer() {
    Moon::Matrix4x4 wvp = ...;
    void* pData;
    MapBuffer(..., pData);
    memcpy(pData, &wvp, sizeof(wvp));  // ✅ Use actual data size
    // OR
    memcpy(pData, &wvp, sizeof(Moon::Matrix4x4));  // ✅ Explicit type
}
```

**Why**: Struct size may include padding. Writing beyond buffer bounds causes memory corruption.

---

### Rule 5: No GPU Resources in Value-Type Structs

**❌ FORBIDDEN:**
```cpp
struct MeshData {
    IBuffer* vertexBuffer;  // ❌ Raw pointer in struct
    IBuffer* indexBuffer;
};

void ProcessMesh() {
    MeshData data1;
    MeshData data2 = data1;  // ❌ Shallow copy, both point to same buffer
    // When data1 is destroyed, data2 has dangling pointers
}
```

**✅ REQUIRED:**
```cpp
struct MeshGPUResources {
    RefCntAutoPtr<IBuffer> vertexBuffer;  // ✅ Smart pointer
    RefCntAutoPtr<IBuffer> indexBuffer;
    size_t vertexCount;
    size_t indexCount;
    
    // ✅ Delete copy constructor to prevent accidental copies
    MeshGPUResources(const MeshGPUResources&) = delete;
    MeshGPUResources& operator=(const MeshGPUResources&) = delete;
    
    // ✅ Allow move operations
    MeshGPUResources(MeshGPUResources&&) = default;
    MeshGPUResources& operator=(MeshGPUResources&&) = default;
};
```

---

### Rule 6: Explicit Resource Cleanup in Destructor

**✅ REQUIRED Pattern:**
```cpp
void Shutdown() {
    // Release resources in reverse order of creation
    m_pPickingSRB.Release();
    m_pPickingPSO.Release();
    m_pPickingPSConstants.Release();
    m_pPickingDSV.Release();
    m_pPickingRTV.Release();
    
    m_pSRB.Release();
    m_pPSO.Release();
    m_pVSConstants.Release();
    m_pIndexBuffer.Release();
    m_pVertexBuffer.Release();
    
    // Device context and device last
    m_pImmediateContext.Release();
    m_pSwapChain.Release();
    m_pDevice.Release();
}
```

**Why**: Explicit ordering prevents dependencies on automatic destruction order.

---

### Rule 7: Validate Resource State Before Use

**✅ REQUIRED in Production Code:**
```cpp
void RenderSceneForPicking(Scene* scene) {
    // Validate critical resources
    if (!m_pPickingRTV || !m_pPickingDSV) {
        MOON_LOG_ERROR("DiligentRenderer", "Picking RTV/DSV is NULL!");
        return;
    }
    
    // Validate resource types (optional in release, mandatory in debug)
    #ifdef _DEBUG
    const auto& rtvDesc = m_pPickingRTV->GetDesc();
    if (rtvDesc.ViewType != TEXTURE_VIEW_RENDER_TARGET) {
        MOON_LOG_ERROR("DiligentRenderer", "Invalid RTV type!");
        return;
    }
    #endif
    
    // Proceed with rendering...
}
```

---

### Rule 8: Use Shader Resource Binding Correctly

**✅ REQUIRED Pattern:**
```cpp
void CreatePSO() {
    m_pDevice->CreateGraphicsPipelineState(..., &m_pPSO);
    
    // Bind static resources
    m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "VSConstants")->Set(m_pVSConstants);
    
    // Create SRB AFTER setting static variables
    m_pPSO->CreateShaderResourceBinding(&m_pSRB, true);
}

void Render() {
    m_pImmediateContext->SetPipelineState(m_pPSO);
    
    // MUST commit before drawing
    m_pImmediateContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
    m_pImmediateContext->DrawIndexed(...);
}
```

**Common Mistake**: Forgetting to call `CommitShaderResources()` before draw calls.

---

## Code Review Checklist

### For Every New GPU Resource:

- [ ] Is it stored using `RefCntAutoPtr<T>` and not raw pointer?
- [ ] If resolution-dependent, is it recreated in `Resize()`?
- [ ] Is it released in `Shutdown()` in proper order?
- [ ] Is it validated before use (null check at minimum)?

### For Every Buffer Update:

- [ ] Does `memcpy` use actual data size, not struct size?
- [ ] Is buffer size verified before writing?
- [ ] Is `MapBuffer` paired with `UnmapBuffer`?

### For Every Pipeline State:

- [ ] Is SRB created after setting static variables?
- [ ] Is `CommitShaderResources` called before draw?
- [ ] Are all required shader variables bound?

### For Every Resize Handler:

- [ ] Are all resolution-dependent resources recreated?
- [ ] Is SwapChain resized first?
- [ ] Are RTV/DSV updated from SwapChain?

---

## Common Patterns

### Pattern: Picking/Selection System

```cpp
class Renderer {
private:
    // Static resources (created once)
    RefCntAutoPtr<IPipelineState> m_pPickingPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_pPickingSRB;
    RefCntAutoPtr<IBuffer> m_pPickingPSConstants;
    
    // Dynamic resources (recreated on resize)
    RefCntAutoPtr<ITextureView> m_pPickingRTV;  // Size = m_Width x m_Height
    RefCntAutoPtr<ITextureView> m_pPickingDSV;  // Size = m_Width x m_Height

public:
    void Initialize() {
        CreatePickingPSO();              // Static: Once only
        CreatePickingRenderTargets();    // Dynamic: Initial creation
    }
    
    void Resize(uint32_t w, uint32_t h) {
        m_Width = w;
        m_Height = h;
        CreatePickingRenderTargets();    // Dynamic: Recreate
        // DON'T recreate PSO/SRB/ConstantBuffers
    }
    
    void CreatePickingPSO() {
        if (m_pPickingPSO) return;  // Guard
        
        // Create shaders, PSO, constant buffers
        // Create SRB last
        m_pPickingPSO->CreateShaderResourceBinding(&m_pPickingSRB, true);
    }
    
    void CreatePickingRenderTargets() {
        // Create textures with current window size
        TextureDesc desc;
        desc.Width = m_Width;
        desc.Height = m_Height;
        // ...
    }
};
```

---

## Debugging Tips

### Detecting Dangling Pointers

1. **Check memory pattern**: `0xFDFDFDFD` = freed memory (MSVC debug heap)
2. **Check pointer value**: `0xFFFF...` patterns often indicate invalid/freed memory
3. **Enable heap validation**: Use `/GS` flag and debug heap
4. **Use AddressSanitizer**: Add `/fsanitize=address` to compiler flags

### Finding Resource Lifetime Issues

```cpp
// Add logging to track resource lifecycle
void CreateTexture() {
    MOON_LOG_DEBUG("Resource", "Creating texture %p", m_pTexture.RawPtr());
}

void ReleaseTexture() {
    MOON_LOG_DEBUG("Resource", "Releasing texture %p (RefCount=%d)", 
                   m_pTexture.RawPtr(), m_pTexture->GetReferenceCount());
}
```

### Verification Macros

```cpp
#define VERIFY_GPU_RESOURCE(ptr, name) \
    do { \
        if (!(ptr)) { \
            MOON_LOG_ERROR("Renderer", #name " is NULL!"); \
            return; \
        } \
    } while(0)

void Render() {
    VERIFY_GPU_RESOURCE(m_pRTV, RTV);
    VERIFY_GPU_RESOURCE(m_pDSV, DSV);
    VERIFY_GPU_RESOURCE(m_pPSO, PSO);
    // ...
}
```

---

## Migration Guide

### Converting Existing Code

**Step 1**: Replace raw pointers with smart pointers
```cpp
// Before
IBuffer* m_pBuffer = nullptr;

// After
RefCntAutoPtr<IBuffer> m_pBuffer;
```

**Step 2**: Add include for RefCntAutoPtr
```cpp
#include "../../external/DiligentEngine/DiligentCore/Common/interface/RefCntAutoPtr.hpp"
```

**Step 3**: Update resource creation
```cpp
// Before
m_pDevice->CreateBuffer(desc, nullptr, &m_pBuffer);

// After (same code, but m_pBuffer is now smart pointer)
m_pDevice->CreateBuffer(desc, nullptr, &m_pBuffer);
```

**Step 4**: Update Resize() to recreate dynamic resources
```cpp
void Resize(uint32_t w, uint32_t h) {
    m_Width = w;
    m_Height = h;
    
    // Recreate ALL resolution-dependent resources
    CreateRenderTargets();
    CreateDepthBuffers();
    CreatePickingBuffers();
    // etc.
}
```

---

## Testing Checklist

- [ ] Test window resize multiple times
- [ ] Test minimize/maximize window
- [ ] Test switching to fullscreen and back
- [ ] Test rapid resize events
- [ ] Run with debug heap enabled
- [ ] Run with AddressSanitizer
- [ ] Verify no memory leaks on shutdown
- [ ] Test picking after resize
- [ ] Test all render features after resize

---

## Reference Implementation

See: `engine/render/DiligentRenderer.cpp`
- Lines 650-750: Picking resource creation pattern (CORRECT)
- Lines 530-545: Resize handling (CORRECT)
- Lines 620-645: Shutdown resource cleanup (CORRECT)

---

## Related Documents

- [ADR-0006: Static CRT Configuration](adr-0006-static-crt-configuration.md)
- [Engine Render Architecture](engine-render.md)
- [Developer Guide](DEVELOPER_GUIDE.md)

---

## Revision History

| Date | Version | Changes | Author |
|------|---------|---------|--------|
| 2025-11-12 | 1.0 | Initial document based on picking bug fix | AI Assistant |

