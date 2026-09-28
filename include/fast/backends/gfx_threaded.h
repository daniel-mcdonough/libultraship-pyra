#pragma once

#include "gfx_rendering_api.h"

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace Fast {

class GfxWindowBackend;

// Counters for the benchmark's BENCH rthread line (report 11), cumulative; the benchmark diffs them
// across a segment. Written with atomic adds on whichever thread counts them; read with atomic loads.
enum BenchRThread {
    BENCH_RT_RECORDS,   // calls recorded
    BENCH_RT_BYTES,     // bytes recorded, payloads included
    BENCH_RT_REPLAY_NS, // CPU time replaying on the main thread (mode 1): what would leave it
    BENCH_RT_SYNCS,     // calls that waited for the stream to drain (shader and framebuffer creation, reads)
    BENCH_RT_CHECKS,    // answers the proxy gave that modes 3 and 4 re-asked of the backend at replay
    BENCH_RT_MISMATCH,  // ... where the proxy said bound and the backend not: a wrong texture would be drawn
    BENCH_RT_SAFE_MISS, // ... where the proxy said not bound and the backend bound: an extra bind, same picture
    BENCH_RT_FRAMES,    // frames recorded (not direct)
    BENCH_RT_WAIT_NS,   // wall time the main thread waited for the GL thread (frames in flight, syncs)
    BENCH_RT_GL_NS,     // the GL thread's CPU time replaying (mode 2), the swap's CPU included
    BENCH_RT_COUNT
};
extern "C" uint64_t gBenchRThread[BENCH_RT_COUNT];

// gSettings.RenderThread (report 11): a GfxRenderingAPI that records every call into a command
// stream and replays it into the real backend. Read on the main thread at each frame start.
//   0  direct: every call is forwarded as it arrives (the default).
//   1  record, and replay on the same thread when the frame ends or a call needs an answer.
//   2  record, and replay on a GL thread that owns the context; at most one frame in flight.
//   3, 4  1 and 2 plus checks: every answer the proxy gave from its own state is re-asked of the backend
//      at replay and counted (benchmark builds; gSettings.ProbeRThreadCorrupt=1 is the positive control).
// Calls that need an answer at once are answered from the proxy's own state where that is exact
// (report 11 §11.3), and otherwise wait for the stream to drain and run where the context is.
class GfxRenderingAPIThreaded final : public GfxRenderingAPI {
  public:
    explicit GfxRenderingAPIThreaded(GfxRenderingAPI* inner);
    ~GfxRenderingAPIThreaded() override;

    // The window backend that owns the context; needed for mode 2. Without it mode 2 runs as 1.
    void SetWindowBackend(GfxWindowBackend* wapi) {
        mWapi = wapi;
    }
    // Back to direct mode with the context on the calling thread, and the GL thread stopped. For
    // teardown, before anything else touches GL.
    void Shutdown();

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
    void RunNow(void (*fn)(void*), void* arg) override;
    void RunAtFrameEnd(void (*fn)(void*), void* arg) override;
    bool DefersWork() override;

  private:
    enum class Op : uint16_t;
    struct Header {
        Op op;
        uint16_t pad;
        uint32_t size; // the whole record, header and payload, a multiple of 8
    };
    struct Block {
        std::unique_ptr<uint8_t[]> data;
        size_t cap = 0;
        size_t used = 0;
    };

    bool Recording() const {
        return mMode != 0;
    }
    bool OnGlThread() const {
        return mGlThreadId == std::this_thread::get_id();
    }
    // A record of type T with `extra` bytes of payload after it; the pointer stays valid until the
    // next Emit or Publish.
    template <typename T> T* Emit(Op op, size_t extra = 0);
    // Hands the block being written to the replay: the GL thread's queue (mode 2), or the list Drain
    // replays (mode 1).
    void Publish();
    // Everything recorded so far has been replayed when it returns: here (mode 1), or by the GL thread
    // (mode 2), which it waits for.
    void Drain();
    void ReplayBlock(Block* b);
    void Replay(const Header* h);
    Block* TakeBlock(size_t atLeast);
    void Recycle(Block* b);
    // Runs fn where the context is, after everything recorded so far, and returns when it has run.
    void Call(const std::function<void()>& fn);
    void SetMode(int mode);
    void GlThreadMain();
    // Waits on mDone while busy() holds (mMutex held around busy), counting the wall time.
    void WaitWhile(const std::function<bool()>& busy);

    std::unique_ptr<GfxRenderingAPI> mInner;
    GfxWindowBackend* mWapi = nullptr;
    int mMode = 0; // 0 direct, 1 same thread, 2 GL thread
    int mChecks = 0;
    int mCorrupt = 0;
    bool mThreadFailed = false;

    // The stream. Blocks are written on the main thread and replayed, then recycled, on the replaying one.
    static constexpr size_t kBlockBytes = 64 * 1024;
    Block* mCur = nullptr;
    std::vector<Block*> mPending;  // mode 1
    std::mutex mMutex;             // guards everything down to mStop
    std::condition_variable mWake; // the GL thread waits on it for blocks
    std::condition_variable mDone; // the main thread waits on it for progress
    std::deque<Block*> mQueue;
    std::vector<Block*> mFree;
    uint64_t mBlocksQueued = 0, mBlocksDone = 0;
    uint64_t mFramesQueued = 0, mFramesDone = 0;
    bool mStop = false;
    bool mAcquireOk = false;
    std::thread mThread;
    std::thread::id mGlThreadId;

    // State the proxy answers from (report 11 §11.3).
    const char* mName = nullptr;
    int mMaxTextureSize = 0;
    FilteringMode mFilter = FILTER_THREE_POINT;
    std::map<std::pair<uint64_t, uint32_t>, ShaderProgram*> mShaders;
    std::vector<void*> mFbTextures;
    std::vector<uint32_t> mTexturePool;
    // The last texture selected per tile, and whether the backend's IsTextureBound answers from
    // that alone this frame (gSettings.LeanGroup): if not, the proxy answers false, which is safe.
    uint32_t mSelected[8] = {};
    bool mLazyBinds = false;
    // GetClipParameters depends on the framebuffer being drawn to: each one's flip, as last set by
    // UpdateFramebufferParameters, and the current one, as set by StartDrawToFramebuffer.
    bool mClipZ01 = false;
    std::vector<bool> mFbInvertY;
    int mCurrentFb = 0;
};

} // namespace Fast
