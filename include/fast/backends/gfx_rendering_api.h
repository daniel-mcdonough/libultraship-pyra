#pragma once

#include <stdint.h>

#include <unordered_map>
#include <set>
#include "imconfig.h"

namespace Fast {
struct ShaderProgram;

struct GfxClipParameters {
    bool z_is_from_0_to_1;
    bool invertY;
};

enum FilteringMode { FILTER_THREE_POINT, FILTER_LINEAR, FILTER_NONE };

// A hash function used to hash a: pair<float, float>
struct hash_pair_ff {
    size_t operator()(const std::pair<float, float>& p) const {
        const auto hash1 = std::hash<float>{}(p.first);
        const auto hash2 = std::hash<float>{}(p.second);

        // If hash1 == hash2, their XOR is zero.
        return (hash1 != hash2) ? hash1 ^ hash2 : hash1;
    }
};

class GfxRenderingAPI {
  public:
    virtual ~GfxRenderingAPI() = default;
    virtual const char* GetName() = 0;
    virtual int GetMaxTextureSize() = 0;
    virtual GfxClipParameters GetClipParameters() = 0;
    virtual void UnloadShader(ShaderProgram* oldPrg) = 0;
    virtual void LoadShader(ShaderProgram* newPrg) = 0;
    virtual ShaderProgram* CreateAndLoadNewShader(uint64_t shaderId0, uint32_t shaderId1) = 0;
    virtual ShaderProgram* LookupShader(uint64_t shaderId0, uint32_t shaderId1) = 0;
    virtual void ShaderGetInfo(ShaderProgram* prg, uint8_t* numInputs, bool usedTextures[2]) = 0;
    virtual uint32_t NewTexture() = 0;
    virtual void SelectTexture(int tile, uint32_t textureId) = 0;
    // True only when the backend knows textureId is bound to this unit right now (§8.37). A backend
    // that does not track its bindings says false, and the interpreter then binds as before.
    virtual bool IsTextureBound(int tile, uint32_t textureId) {
        return false;
    }
    virtual void UploadTexture(const uint8_t* rgba32Buf, uint32_t width, uint32_t height) = 0;
    virtual void SetSamplerParameters(int sampler, bool linear_filter, uint32_t cms, uint32_t cmt) = 0;
    virtual void SetDepthTestAndMask(bool depth_test, bool z_upd) = 0;
    virtual void SetZmodeDecal(bool decal) = 0;
    virtual void SetViewport(int x, int y, int width, int height) = 0;
    virtual void SetScissor(int x, int y, int width, int height) = 0;
    virtual void SetUseAlpha(bool useAlpha) = 0;
    virtual void DrawTriangles(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris) = 0;
    virtual void Init() = 0;
    virtual void OnResize() = 0;
    virtual void StartFrame() = 0;
    virtual void EndFrame() = 0;
    virtual void FinishRender() = 0;
    virtual int CreateFramebuffer() = 0;
    virtual void UpdateFramebufferParameters(int fb_id, uint32_t width, uint32_t height, uint32_t msaa_level,
                                             bool opengl_invertY, bool render_target, bool has_depth_buffer,
                                             bool can_extract_depth) = 0;
    virtual void StartDrawToFramebuffer(int fbId, float noiseScale) = 0;
    virtual void CopyFramebuffer(int fbDstId, int fbSrcId, int srcX0, int srcY0, int srcX1, int srcY1, int dstX0,
                                 int dstY0, int dstX1, int dstY1) = 0;
    virtual void ClearFramebuffer(bool color, bool depth) = 0;
    // Signals that the depth/stencil contents of the currently bound framebuffer will not be read
    // again. A tile-based deferred renderer otherwise has to resolve them out to main memory at the
    // end of every render pass. Immediate-mode backends gain nothing from this, so it defaults to a
    // no-op rather than being pure virtual.
    virtual void DiscardDepthStencil() {
    }
    // Drops every compiled shader variant so that the next draw rebuilds them. Needed when a
    // setting baked into the shader source at build time changes - texture filtering is the one
    // that exists today - because the variant key does not include it. Only safe to call between
    // frames, never with a draw in flight. Backends with no such cache need do nothing.
    virtual void ClearShaderCache() {
    }
    // Draws anything the backend has deferred (the GLES2 backend's gSettings.LeanGroup). Called at
    // the end of the interpreter's frame, before the GUI draws into the same context.
    virtual void FlushDeferredDraws() {
    }
    // True when IsTextureBound will answer, in the frame about to start, from the last SelectTexture
    // per tile alone (the GLES2 backend's gSettings.LeanGroup). Reads the settings StartFrame is about
    // to apply; called on the main thread before it. Report 11's proxy shadows the answer then.
    virtual bool WillBindLazily() {
        return false;
    }
    // Runs fn(arg) in order with the calls made on this object: at once for a backend, when the
    // stream reaches it for report 11's proxy. For GL work done outside the interface (ImGui's draw).
    virtual void RunInOrder(void (*fn)(void*), void* arg) {
        fn(arg);
    }
    // Runs fn(arg) after the calls made on this object and returns when it has run: where the context is,
    // which for report 11's proxy in its threaded mode is the GL thread, while the caller waits. For GL
    // work whose results the caller reads at once (ImGui's font atlas).
    virtual void RunNow(void (*fn)(void*), void* arg) {
        fn(arg);
    }
    // The end of a frame: the swap and anything that must follow the frame's GL work (report 11). At once
    // for a backend. Report 11's proxy runs it after the frame's calls, on the GL thread in its threaded
    // mode, and holds the caller back while more than one frame is in flight.
    virtual void RunAtFrameEnd(void (*fn)(void*), void* arg) {
        fn(arg);
    }
    // True while GL work is done later on another thread (report 11's threaded mode): data handed to
    // RunInOrder must then outlive the caller's frame.
    virtual bool DefersWork() {
        return false;
    }
    // The settings StartFrame reads from the CVar store, captured on the main thread so a GL thread
    // never reads the store (it is not thread-safe). StartFrame() is CaptureFrameSettings then
    // StartFrameWith; report 11's proxy captures when it records and applies when it replays.
    virtual size_t FrameSettingsSize() {
        return 0;
    }
    virtual void CaptureFrameSettings(void* out) {
    }
    virtual void StartFrameWith(const void* settings) {
        StartFrame();
    }
    virtual void ReadFramebufferToCPU(int fbId, uint32_t width, uint32_t height, uint16_t* rgba16Buf) = 0;
    virtual void ResolveMSAAColorBuffer(int fbIdTarger, int fbIdSrc) = 0;
    virtual std::unordered_map<std::pair<float, float>, uint16_t, hash_pair_ff>
    GetPixelDepth(int fb_id, const std::set<std::pair<float, float>>& coordinates) = 0;
    virtual void* GetFramebufferTextureId(int fbId) = 0;
    virtual void SelectTextureFb(int fbId) = 0;
    virtual void DeleteTexture(uint32_t texId) = 0;
    virtual void SetTextureFilter(FilteringMode mode) = 0;
    virtual FilteringMode GetTextureFilter() = 0;
    virtual void SetSrgbMode() = 0;
    virtual ImTextureID GetTextureById(int id) = 0;

  protected:
    int8_t mCurrentDepthTest = 0;
    int8_t mCurrentDepthMask = 0;
    int8_t mCurrentZmodeDecal = 0;
    int8_t mLastDepthTest = -1;
    int8_t mLastDepthMask = -1;
    int8_t mLastZmodeDecal = -1;
    bool mSrgbMode = false;
};
} // namespace Fast
