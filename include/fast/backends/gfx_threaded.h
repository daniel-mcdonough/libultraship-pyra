#pragma once

#include "gfx_rendering_api.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

namespace Fast {

// Counters for the benchmark's BENCH rthread line (report 11), cumulative; the benchmark diffs them
// across a segment. Written on the thread that replays.
enum BenchRThread {
    BENCH_RT_RECORDS,   // calls recorded
    BENCH_RT_BYTES,     // bytes recorded, payloads included
    BENCH_RT_REPLAY_NS, // CPU time replaying: what would leave the main thread
    BENCH_RT_SYNCS,     // calls that waited for the stream to drain (shader and framebuffer creation, reads)
    BENCH_RT_CHECKS,    // answers the proxy gave that mode 3 re-asked of the backend at replay
    BENCH_RT_MISMATCH,  // ... where the proxy said bound and the backend not: a wrong texture would be drawn
    BENCH_RT_SAFE_MISS, // ... where the proxy said not bound and the backend bound: an extra bind, same picture
    BENCH_RT_FRAMES,    // frames recorded (not direct)
    BENCH_RT_COUNT
};
extern "C" uint64_t gBenchRThread[BENCH_RT_COUNT];

// gSettings.RenderThread (report 11): a GfxRenderingAPI that records every call into a command
// stream and replays it into the real backend. Read on the main thread at each frame start.
//   0  direct: every call is forwarded as it arrives (the default).
//   1  record, and replay on the same thread when the frame ends or a call needs an answer.
//   3  1 plus checks: every answer the proxy gave from its own state is re-asked of the backend at
//      replay and counted (benchmark builds; gSettings.ProbeRThreadCorrupt=1 is the positive control).
// Stage 2 adds 2, the replay on a GL thread. Calls that need an answer at once are answered from the
// proxy's own state where that is exact (11.3), and otherwise wait for the stream to drain.
class GfxRenderingAPIThreaded final : public GfxRenderingAPI {
  public:
    explicit GfxRenderingAPIThreaded(GfxRenderingAPI* inner);
    ~GfxRenderingAPIThreaded() override;

    GfxRenderingAPI* Inner() const {
        return mInner.get();
    }

    const char* GetName() override;
    int GetMaxTextureSize() override;
    GfxClipParameters GetClipParameters() override;
    void UnloadShader(ShaderProgram* oldPrg) override;
    void LoadShader(ShaderProgram* newPrg) override;
    ShaderProgram* CreateAndLoadNewShader(uint64_t shaderId0, uint32_t shaderId1) override;
    ShaderProgram* LookupShader(uint64_t shaderId0, uint32_t shaderId1) override;
    void ShaderGetInfo(ShaderProgram* prg, uint8_t* numInputs, bool usedTextures[2]) override;
    uint32_t NewTexture() override;
    void SelectTexture(int tile, uint32_t textureId) override;
    bool IsTextureBound(int tile, uint32_t textureId) override;
    void UploadTexture(const uint8_t* rgba32Buf, uint32_t width, uint32_t height) override;
    void SetSamplerParameters(int sampler, bool linear_filter, uint32_t cms, uint32_t cmt) override;
    void SetDepthTestAndMask(bool depth_test, bool z_upd) override;
    void SetZmodeDecal(bool decal) override;
    void SetViewport(int x, int y, int width, int height) override;
    void SetScissor(int x, int y, int width, int height) override;
    void SetUseAlpha(bool useAlpha) override;
    void DrawTriangles(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris) override;
    void Init() override;
    void OnResize() override;
    void StartFrame() override;
    void EndFrame() override;
    void FinishRender() override;
    int CreateFramebuffer() override;
    void UpdateFramebufferParameters(int fb_id, uint32_t width, uint32_t height, uint32_t msaa_level,
                                     bool opengl_invertY, bool render_target, bool has_depth_buffer,
                                     bool can_extract_depth) override;
    void StartDrawToFramebuffer(int fbId, float noiseScale) override;
    void CopyFramebuffer(int fbDstId, int fbSrcId, int srcX0, int srcY0, int srcX1, int srcY1, int dstX0, int dstY0,
                         int dstX1, int dstY1) override;
    void ClearFramebuffer(bool color, bool depth) override;
    void DiscardDepthStencil() override;
    void ClearShaderCache() override;
    void FlushDeferredDraws() override;
    void ReadFramebufferToCPU(int fbId, uint32_t width, uint32_t height, uint16_t* rgba16Buf) override;
    void ResolveMSAAColorBuffer(int fbIdTarger, int fbIdSrc) override;
    std::unordered_map<std::pair<float, float>, uint16_t, hash_pair_ff>
    GetPixelDepth(int fb_id, const std::set<std::pair<float, float>>& coordinates) override;
    void* GetFramebufferTextureId(int fbId) override;
    void SelectTextureFb(int fbId) override;
    void DeleteTexture(uint32_t texId) override;
    void SetTextureFilter(FilteringMode mode) override;
    FilteringMode GetTextureFilter() override;
    void SetSrgbMode() override;
    ImTextureID GetTextureById(int id) override;
    bool WillBindLazily() override;
    void RunInOrder(void (*fn)(void*), void* arg) override;

  private:
    enum class Op : uint16_t;
    struct Header {
        Op op;
        uint16_t pad;
        uint32_t size; // the whole record, header and payload, a multiple of 8
    };

    bool Recording() const {
        return mMode != 0;
    }
    // A record of type T with `extra` bytes of payload after it; the pointer stays valid until the
    // stream is next replayed.
    template <typename T> T* Emit(Op op, size_t extra = 0);
    uint8_t* Reserve(size_t bytes);
    // Replays everything recorded so far, on this thread.
    void Drain();
    void Replay(const Header* h);
    // A call that needs the backend now: the stream drains first.
    void Sync();

    std::unique_ptr<GfxRenderingAPI> mInner;
    int mMode = 0;
    int mChecks = 0;
    int mCorrupt = 0;

    // The stream: one growing buffer in stage 1, reused every frame.
    std::vector<uint8_t> mStream;
    size_t mStreamUsed = 0;

    // State the proxy answers from (report 11 §11.3).
    const char* mName = nullptr;
    int mMaxTextureSize = 0;
    // GetClipParameters depends on the framebuffer being drawn to: each one's flip, as last set by
    // UpdateFramebufferParameters, and the current one, as set by StartDrawToFramebuffer.
    bool mClipZ01 = false;
    std::vector<bool> mFbInvertY;
    int mCurrentFb = 0;
    FilteringMode mFilter = FILTER_THREE_POINT;
    std::map<std::pair<uint64_t, uint32_t>, ShaderProgram*> mShaders;
    std::vector<void*> mFbTextures;
    std::vector<uint32_t> mTexturePool;
    // The last texture selected per tile, and whether the backend's IsTextureBound answers from
    // that alone this frame (gSettings.LeanGroup): if not, the proxy answers false, which is safe.
    uint32_t mSelected[8] = {};
    bool mLazyBinds = false;
};

} // namespace Fast
