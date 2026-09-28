// gSettings.RenderThread (report 11): the recording proxy on GfxRenderingAPI, replayed on the same
// thread (mode 1) or on a GL thread that owns the context (mode 2). The design, the crossings and their
// reasons are in docs/pyra/11-two-core-render.md.

#include "fast/backends/gfx_threaded.h"

#include <chrono>
#include <cstring>
#include <ctime>
#include <pthread.h>

#include <spdlog/spdlog.h>

#include "fast/backends/gfx_window_manager_api.h"
#include "ship/Context.h"
#include "ship/config/ConsoleVariable.h"
#ifdef SOH_BENCHMARK
#include "fast/interpreter.h"
#endif

extern "C" uint64_t gBenchRThread[Fast::BENCH_RT_COUNT] = {};

namespace Fast {

enum class GfxRenderingAPIThreaded::Op : uint16_t {
    UnloadShader,
    LoadShader,
    SelectTexture,
    UploadTexture,
    SetSamplerParameters,
    SetDepthTestAndMask,
    SetZmodeDecal,
    SetViewport,
    SetScissor,
    SetUseAlpha,
    DrawTriangles,
    OnResize,
    StartFrame,
    EndFrame,
    UpdateFramebufferParameters,
    StartDrawToFramebuffer,
    CopyFramebuffer,
    ClearFramebuffer,
    DiscardDepthStencil,
    ClearShaderCache,
    FlushDeferredDraws,
    ResolveMSAAColorBuffer,
    PixelDepthSideEffects,
    SelectTextureFb,
    DeleteTexture,
    SetSrgbMode,
    RunInOrder,
    FrameEnd,
    SyncCall,
    Acquire,
    Release,
    CheckBound,
    CheckClip,
    DumpPath,
};

namespace {

struct PrgArgs {
    ShaderProgram* prg;
};
struct TileTexArgs {
    int32_t tile;
    uint32_t tex;
};
struct UploadArgs {
    uint32_t width, height;
    // then width * height * 4 bytes
};
struct SamplerArgs {
    int32_t sampler;
    bool linear;
    uint32_t cms, cmt;
};
struct TwoBools {
    bool a, b;
};
struct OneBool {
    bool a;
};
struct RectArgs {
    int32_t x, y, w, h;
};
struct DrawArgs {
    uint32_t len, tris;
    // then len floats
};
struct StartArgs {
    uint32_t settingsBytes;
    uint32_t pad;
    // then the backend's frame settings
};
struct FbParamArgs {
    int32_t fb;
    uint32_t width, height, msaa;
    bool invertY, renderTarget, hasDepth, canExtractDepth;
};
struct StartFbArgs {
    int32_t fb;
    float noiseScale;
};
struct CopyFbArgs {
    int32_t dst, src, sx0, sy0, sx1, sy1, dx0, dy0, dx1, dy1;
};
struct TwoInts {
    int32_t a, b;
};
struct OneInt {
    int32_t a;
};
struct PixelDepthArgs {
    int32_t fb;
    uint32_t count;
    // then count pairs of floats
};
struct CallbackArgs {
    void (*fn)(void*);
    void* arg;
};
struct SyncCallArgs {
    const std::function<void()>* fn;
};
struct CheckArgs {
    int32_t tile;
    uint32_t tex;
    bool answer;
};

constexpr size_t Align8(size_t n) {
    return (n + 7) & ~(size_t)7;
}

uint64_t ThreadCpuNanos() {
    timespec ts{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

void Count(int k, uint64_t n) {
    __atomic_fetch_add(&gBenchRThread[k], n, __ATOMIC_RELAXED);
}

int CVarInt(const char* name, int fallback) {
    return Ship::Context::GetInstance()->GetConsoleVariables()->GetInteger(name, fallback);
}

// Names handed out ahead of use (report 11 §11.3). A GL name from glGenTextures is reserved until
// deleted, so holding some unused is harmless.
constexpr size_t kTexturePoolRefill = 32;

constexpr size_t kMaxTiles = 8;

} // namespace

GfxRenderingAPIThreaded::GfxRenderingAPIThreaded(GfxRenderingAPI* inner) : mInner(inner) {
}

GfxRenderingAPIThreaded::~GfxRenderingAPIThreaded() {
    Shutdown();
    for (Block* b : mFree) {
        delete b;
    }
    delete mCur;
}

void GfxRenderingAPIThreaded::Shutdown() {
    SetMode(0);
    if (mThread.joinable()) {
        {
            std::lock_guard<std::mutex> lk(mMutex);
            mStop = true;
        }
        mWake.notify_all();
        mThread.join();
        mGlThreadId = std::thread::id();
    }
}

// --- The stream ----------------------------------------------------------------------------------

GfxRenderingAPIThreaded::Block* GfxRenderingAPIThreaded::TakeBlock(size_t atLeast) {
    {
        std::lock_guard<std::mutex> lk(mMutex);
        for (size_t i = 0; i < mFree.size(); i++) {
            if (mFree[i]->cap >= atLeast) {
                Block* b = mFree[i];
                mFree[i] = mFree.back();
                mFree.pop_back();
                b->used = 0;
                return b;
            }
        }
    }
    Block* b = new Block();
    b->cap = std::max(kBlockBytes, atLeast);
    b->data.reset(new uint8_t[b->cap]);
    return b;
}

void GfxRenderingAPIThreaded::Recycle(Block* b) {
    b->used = 0;
    std::lock_guard<std::mutex> lk(mMutex);
    mFree.push_back(b);
}

template <typename T> T* GfxRenderingAPIThreaded::Emit(Op op, size_t extra) {
    const size_t size = Align8(sizeof(Header)) + Align8(sizeof(T) + extra);
    if (mCur == nullptr || mCur->used + size > mCur->cap) {
        Publish();
        mCur = TakeBlock(size);
    }
    uint8_t* p = mCur->data.get() + mCur->used;
    mCur->used += size;
    Header* h = reinterpret_cast<Header*>(p);
    h->op = op;
    h->pad = 0;
    h->size = (uint32_t)size;
    Count(BENCH_RT_RECORDS, 1);
    Count(BENCH_RT_BYTES, size);
    return reinterpret_cast<T*>(p + Align8(sizeof(Header)));
}

void GfxRenderingAPIThreaded::Publish() {
    if (mCur == nullptr || mCur->used == 0) {
        return;
    }
    Block* b = mCur;
    mCur = nullptr;
    if (mMode == 2) {
        {
            std::lock_guard<std::mutex> lk(mMutex);
            mQueue.push_back(b);
            mBlocksQueued++;
        }
        mWake.notify_one();
    } else {
        mPending.push_back(b);
    }
}

void GfxRenderingAPIThreaded::WaitWhile(const std::function<bool()>& busy) {
    std::unique_lock<std::mutex> lk(mMutex);
    if (!busy()) {
        return;
    }
    const auto start = std::chrono::steady_clock::now();
    mDone.wait(lk, [&] { return !busy(); });
    Count(BENCH_RT_WAIT_NS,
          (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start)
              .count());
}

void GfxRenderingAPIThreaded::Drain() {
    Publish();
    if (mMode == 2) {
        WaitWhile([&] { return mBlocksDone != mBlocksQueued; });
        return;
    }
    if (mPending.empty()) {
        return;
    }
    const uint64_t start = ThreadCpuNanos();
    for (Block* b : mPending) {
        ReplayBlock(b);
        Recycle(b);
    }
    mPending.clear();
    Count(BENCH_RT_REPLAY_NS, ThreadCpuNanos() - start);
}

void GfxRenderingAPIThreaded::ReplayBlock(Block* b) {
    size_t pos = 0;
    while (pos < b->used) {
        const Header* h = reinterpret_cast<const Header*>(b->data.get() + pos);
        Replay(h);
        pos += h->size;
    }
}

void GfxRenderingAPIThreaded::Call(const std::function<void()>& fn) {
    if (mMode == 0) {
        fn();
        return;
    }
    Count(BENCH_RT_SYNCS, 1);
    if (mMode == 2) {
        Emit<SyncCallArgs>(Op::SyncCall)->fn = &fn;
        Drain(); // the GL thread has run fn when it returns
        return;
    }
    Drain();
    fn();
}

// --- The GL thread -------------------------------------------------------------------------------

void GfxRenderingAPIThreaded::GlThreadMain() {
    pthread_setname_np(pthread_self(), "soh-gl");
    std::unique_lock<std::mutex> lk(mMutex);
    for (;;) {
        mWake.wait(lk, [&] { return mStop || !mQueue.empty(); });
        if (mQueue.empty()) {
            break; // mStop
        }
        Block* b = mQueue.front();
        mQueue.pop_front();
        lk.unlock();
        const uint64_t start = ThreadCpuNanos();
        ReplayBlock(b);
        Count(BENCH_RT_GL_NS, ThreadCpuNanos() - start);
        b->used = 0;
        lk.lock();
        mFree.push_back(b);
        mBlocksDone++;
        mDone.notify_all();
    }
}

// Mode changes happen at a frame start, with the previous frame's work done in the old mode.
void GfxRenderingAPIThreaded::SetMode(int mode) {
    if (mode == 2 && (mWapi == nullptr || mThreadFailed)) {
        mode = 1;
    }
    if (mode == mMode) {
        return;
    }
    Drain();
    if (mMode == 2) {
        // Leaving: the GL thread lets the context go, then this thread takes it.
        Emit<OneInt>(Op::Release);
        Drain();
        if (!mWapi->MakeContextCurrent(true)) {
            SPDLOG_ERROR("RenderThread: could not take the GL context back from the GL thread");
        }
    }
    mMode = mode;
    if (mode != 2) {
        return;
    }
    if (!mWapi->MakeContextCurrent(false)) {
        SPDLOG_ERROR("RenderThread: could not release the GL context; staying on this thread");
        mThreadFailed = true;
        mMode = 1;
        return;
    }
    if (!mThread.joinable()) {
        std::lock_guard<std::mutex> lk(mMutex);
        mStop = false;
        mThread = std::thread([this] { GlThreadMain(); });
        mGlThreadId = mThread.get_id();
    }
    mAcquireOk = false;
    Emit<OneInt>(Op::Acquire);
    Drain();
    if (!mAcquireOk) {
        SPDLOG_ERROR("RenderThread: the GL thread could not make the context current; staying on this thread");
        mThreadFailed = true;
        mMode = 1;
        mWapi->MakeContextCurrent(true);
        return;
    }
    SPDLOG_INFO("RenderThread: GL context moved to the GL thread");
}

// --- Replay --------------------------------------------------------------------------------------

void GfxRenderingAPIThreaded::Replay(const Header* h) {
    const uint8_t* a = reinterpret_cast<const uint8_t*>(h) + Align8(sizeof(Header));
    GfxRenderingAPI* r = mInner.get();
    switch (h->op) {
        case Op::UnloadShader:
            r->UnloadShader(reinterpret_cast<const PrgArgs*>(a)->prg);
            break;
        case Op::LoadShader:
            r->LoadShader(reinterpret_cast<const PrgArgs*>(a)->prg);
            break;
        case Op::SelectTexture: {
            const auto* x = reinterpret_cast<const TileTexArgs*>(a);
            r->SelectTexture(x->tile, x->tex);
            break;
        }
        case Op::UploadTexture: {
            const auto* x = reinterpret_cast<const UploadArgs*>(a);
            r->UploadTexture(a + sizeof(UploadArgs), x->width, x->height);
            break;
        }
        case Op::SetSamplerParameters: {
            const auto* x = reinterpret_cast<const SamplerArgs*>(a);
            r->SetSamplerParameters(x->sampler, x->linear, x->cms, x->cmt);
            break;
        }
        case Op::SetDepthTestAndMask: {
            const auto* x = reinterpret_cast<const TwoBools*>(a);
            r->SetDepthTestAndMask(x->a, x->b);
            break;
        }
        case Op::SetZmodeDecal:
            r->SetZmodeDecal(reinterpret_cast<const OneBool*>(a)->a);
            break;
        case Op::SetViewport: {
            const auto* x = reinterpret_cast<const RectArgs*>(a);
            r->SetViewport(x->x, x->y, x->w, x->h);
            break;
        }
        case Op::SetScissor: {
            const auto* x = reinterpret_cast<const RectArgs*>(a);
            r->SetScissor(x->x, x->y, x->w, x->h);
            break;
        }
        case Op::SetUseAlpha:
            r->SetUseAlpha(reinterpret_cast<const OneBool*>(a)->a);
            break;
        case Op::DrawTriangles: {
            const auto* x = reinterpret_cast<const DrawArgs*>(a);
            // The backend takes a mutable pointer; it does not write through it.
            r->DrawTriangles(const_cast<float*>(reinterpret_cast<const float*>(a + sizeof(DrawArgs))), x->len, x->tris);
            break;
        }
        case Op::OnResize:
            r->OnResize();
            break;
        case Op::StartFrame:
            r->StartFrameWith(a + sizeof(StartArgs));
            break;
        case Op::EndFrame:
            r->EndFrame();
            break;
        case Op::UpdateFramebufferParameters: {
            const auto* x = reinterpret_cast<const FbParamArgs*>(a);
            r->UpdateFramebufferParameters(x->fb, x->width, x->height, x->msaa, x->invertY, x->renderTarget,
                                           x->hasDepth, x->canExtractDepth);
            break;
        }
        case Op::StartDrawToFramebuffer: {
            const auto* x = reinterpret_cast<const StartFbArgs*>(a);
            r->StartDrawToFramebuffer(x->fb, x->noiseScale);
            break;
        }
        case Op::CopyFramebuffer: {
            const auto* x = reinterpret_cast<const CopyFbArgs*>(a);
            r->CopyFramebuffer(x->dst, x->src, x->sx0, x->sy0, x->sx1, x->sy1, x->dx0, x->dy0, x->dx1, x->dy1);
            break;
        }
        case Op::ClearFramebuffer: {
            const auto* x = reinterpret_cast<const TwoBools*>(a);
            r->ClearFramebuffer(x->a, x->b);
            break;
        }
        case Op::DiscardDepthStencil:
            r->DiscardDepthStencil();
            break;
        case Op::ClearShaderCache:
            r->ClearShaderCache();
            break;
        case Op::FlushDeferredDraws:
            r->FlushDeferredDraws();
            break;
        case Op::ResolveMSAAColorBuffer: {
            const auto* x = reinterpret_cast<const TwoInts*>(a);
            r->ResolveMSAAColorBuffer(x->a, x->b);
            break;
        }
        case Op::PixelDepthSideEffects: {
            // GLES2: the backend reads nothing and answers zeros (report 11 §11.3), but it flushes its
            // deferred draws and rebinds a framebuffer on the way. Replayed for those, answer dropped.
            const auto* x = reinterpret_cast<const PixelDepthArgs*>(a);
            const float* xy = reinterpret_cast<const float*>(a + sizeof(PixelDepthArgs));
            std::set<std::pair<float, float>> coords;
            for (uint32_t i = 0; i < x->count; i++) {
                coords.emplace(xy[2 * i], xy[2 * i + 1]);
            }
            r->GetPixelDepth(x->fb, coords);
            break;
        }
        case Op::SelectTextureFb:
            r->SelectTextureFb(reinterpret_cast<const OneInt*>(a)->a);
            break;
        case Op::DeleteTexture:
            r->DeleteTexture((uint32_t)reinterpret_cast<const OneInt*>(a)->a);
            break;
        case Op::SetSrgbMode:
            r->SetSrgbMode();
            break;
        case Op::RunInOrder: {
            const auto* x = reinterpret_cast<const CallbackArgs*>(a);
            x->fn(x->arg);
            break;
        }
        case Op::FrameEnd: {
            const auto* x = reinterpret_cast<const CallbackArgs*>(a);
            x->fn(x->arg);
            {
                std::lock_guard<std::mutex> lk(mMutex);
                mFramesDone++;
            }
            mDone.notify_all();
            break;
        }
        case Op::SyncCall:
            (*reinterpret_cast<const SyncCallArgs*>(a)->fn)();
            break;
        case Op::Acquire:
            mAcquireOk = mWapi->MakeContextCurrent(true);
            break;
        case Op::Release:
            mWapi->MakeContextCurrent(false);
            break;
        case Op::CheckBound: {
            const auto* x = reinterpret_cast<const CheckArgs*>(a);
            Count(BENCH_RT_CHECKS, 1);
            const bool backend = r->IsTextureBound(x->tile, x->tex);
            if (x->answer && !backend) {
                Count(BENCH_RT_MISMATCH, 1);
            } else if (!x->answer && backend) {
                Count(BENCH_RT_SAFE_MISS, 1);
            }
            break;
        }
        case Op::DumpPath:
#ifdef SOH_BENCHMARK
            strncpy(gBenchDumpPathGL, reinterpret_cast<const char*>(a), sizeof(gBenchDumpPathGL) - 1);
#endif
            break;
        case Op::CheckClip: {
            // Counted with the binding checks: a wrong answer here flips the picture.
            Count(BENCH_RT_CHECKS, 1);
            if (r->GetClipParameters().invertY != reinterpret_cast<const CheckArgs*>(a)->answer) {
                Count(BENCH_RT_MISMATCH, 1);
            }
            break;
        }
    }
}

// --- Answered by the proxy -----------------------------------------------------------------------

const char* GfxRenderingAPIThreaded::GetName() {
    return mName != nullptr ? mName : mInner->GetName();
}

int GfxRenderingAPIThreaded::GetMaxTextureSize() {
    return mMaxTextureSize;
}

GfxClipParameters GfxRenderingAPIThreaded::GetClipParameters() {
    if (!Recording()) {
        return mInner->GetClipParameters();
    }
    const bool invertY = mCurrentFb >= 0 && mCurrentFb < (int)mFbInvertY.size() && mFbInvertY[mCurrentFb];
    if (mChecks) {
        Emit<CheckArgs>(Op::CheckClip)->answer = invertY;
    }
    return { mClipZ01, invertY };
}

ShaderProgram* GfxRenderingAPIThreaded::LookupShader(uint64_t shaderId0, uint32_t shaderId1) {
    if (!Recording()) {
        return mInner->LookupShader(shaderId0, shaderId1);
    }
    auto it = mShaders.find({ shaderId0, shaderId1 });
    return it == mShaders.end() ? nullptr : it->second;
}

// The fields it reads are written once, when the backend creates the program, in a synchronous call.
void GfxRenderingAPIThreaded::ShaderGetInfo(ShaderProgram* prg, uint8_t* numInputs, bool usedTextures[2]) {
    mInner->ShaderGetInfo(prg, numInputs, usedTextures);
}

bool GfxRenderingAPIThreaded::IsTextureBound(int tile, uint32_t textureId) {
    if (!Recording()) {
        return mInner->IsTextureBound(tile, textureId);
    }
    bool answer = mLazyBinds && textureId != 0 && tile >= 0 && tile < (int)kMaxTiles && mSelected[tile] == textureId;
    if (mChecks) {
        // The positive control: one true answer in 64 turned false. A false is always safe (the
        // interpreter binds as it did before §8.37), so the picture does not change; safe_miss must.
        static uint32_t sCorruptTick = 0;
        if (mCorrupt && answer && (++sCorruptTick & 63) == 0) {
            answer = false;
        }
        auto* c = Emit<CheckArgs>(Op::CheckBound);
        c->tile = tile;
        c->tex = textureId;
        c->answer = answer;
    }
    return answer;
}

void* GfxRenderingAPIThreaded::GetFramebufferTextureId(int fbId) {
    if (!Recording()) {
        return mInner->GetFramebufferTextureId(fbId);
    }
    if (fbId >= 0 && fbId < (int)mFbTextures.size() && mFbTextures[fbId] != nullptr) {
        return mFbTextures[fbId];
    }
    void* tex = nullptr;
    Call([&] { tex = mInner->GetFramebufferTextureId(fbId); });
    return tex;
}

// The GL backend's is a pure cast.
ImTextureID GfxRenderingAPIThreaded::GetTextureById(int id) {
    return mInner->GetTextureById(id);
}

FilteringMode GfxRenderingAPIThreaded::GetTextureFilter() {
    return mFilter;
}

// Reads the CVar store only: main thread.
bool GfxRenderingAPIThreaded::WillBindLazily() {
    return mInner->WillBindLazily();
}

bool GfxRenderingAPIThreaded::DefersWork() {
    return mMode == 2;
}

std::unordered_map<std::pair<float, float>, uint16_t, hash_pair_ff>
GfxRenderingAPIThreaded::GetPixelDepth(int fb_id, const std::set<std::pair<float, float>>& coordinates) {
#ifdef USE_OPENGLES2
    if (Recording()) {
        auto* x = Emit<PixelDepthArgs>(Op::PixelDepthSideEffects, coordinates.size() * 2 * sizeof(float));
        x->fb = fb_id;
        x->count = (uint32_t)coordinates.size();
        float* xy = reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(x) + sizeof(PixelDepthArgs));
        std::unordered_map<std::pair<float, float>, uint16_t, hash_pair_ff> res;
        for (const auto& c : coordinates) {
            *xy++ = c.first;
            *xy++ = c.second;
            res.emplace(c, 0);
        }
        return res;
    }
#endif
    std::unordered_map<std::pair<float, float>, uint16_t, hash_pair_ff> res;
    Call([&] { res = mInner->GetPixelDepth(fb_id, coordinates); });
    return res;
}

// --- Synchronous ---------------------------------------------------------------------------------

// At startup, in direct mode.
void GfxRenderingAPIThreaded::Init() {
    mInner->Init();
    mName = mInner->GetName();
    mMaxTextureSize = mInner->GetMaxTextureSize();
    mFilter = mInner->GetTextureFilter();
    // Framebuffer 0 exists from Init and is current.
    const GfxClipParameters clip = mInner->GetClipParameters();
    mClipZ01 = clip.z_is_from_0_to_1;
    mFbInvertY.assign(1, clip.invertY);
    mCurrentFb = 0;
}

ShaderProgram* GfxRenderingAPIThreaded::CreateAndLoadNewShader(uint64_t shaderId0, uint32_t shaderId1) {
    ShaderProgram* prg = nullptr;
    Call([&] { prg = mInner->CreateAndLoadNewShader(shaderId0, shaderId1); });
    mShaders[{ shaderId0, shaderId1 }] = prg;
    return prg;
}

int GfxRenderingAPIThreaded::CreateFramebuffer() {
    int fb = 0;
    void* tex = nullptr;
    Call([&] {
        fb = mInner->CreateFramebuffer();
        tex = mInner->GetFramebufferTextureId(fb);
    });
    if (fb >= (int)mFbTextures.size()) {
        mFbTextures.resize(fb + 1, nullptr);
    }
    // Created here and never replaced: UpdateFramebufferParameters resizes it in place.
    mFbTextures[fb] = tex;
    return fb;
}

void GfxRenderingAPIThreaded::ReadFramebufferToCPU(int fbId, uint32_t width, uint32_t height, uint16_t* rgba16Buf) {
    Call([&] { mInner->ReadFramebufferToCPU(fbId, width, height, rgba16Buf); });
}

uint32_t GfxRenderingAPIThreaded::NewTexture() {
    if (!Recording()) {
        return mInner->NewTexture();
    }
    if (mTexturePool.empty()) {
        Call([&] {
            for (size_t i = 0; i < kTexturePoolRefill; i++) {
                mTexturePool.push_back(mInner->NewTexture());
            }
        });
    }
    const uint32_t id = mTexturePool.back();
    mTexturePool.pop_back();
    return id;
}

// Synchronous: the GL backend's also clears the interpreter's texture cache, which is the main
// thread's (it waits meanwhile). A settings change, rare.
void GfxRenderingAPIThreaded::SetTextureFilter(FilteringMode mode) {
    mFilter = mode;
    Call([&] { mInner->SetTextureFilter(mode); });
}

// Called from inside the swap (Interpreter::SwapNow), which runs on the GL thread in mode 2.
void GfxRenderingAPIThreaded::FinishRender() {
    if (OnGlThread() || mMode != 2) {
        // Modes 0 and 1: RunAtFrameEnd drained the stream before the swap.
        mInner->FinishRender();
        return;
    }
    Call([&] { mInner->FinishRender(); });
}

// --- Queued --------------------------------------------------------------------------------------

void GfxRenderingAPIThreaded::StartFrame() {
    const int value = CVarInt("gSettings.RenderThread", kRenderThreadDefault);
    const int mode = value == 0 ? 0 : (value == 2 || value == 4) ? 2 : 1;
#ifdef SOH_BENCHMARK
    const int checks = value >= 3;
#else
    const int checks = 0;
#endif
    SetMode(mode);
    mChecks = checks;
#ifdef SOH_BENCHMARK
    // The frame dump's path (§8.32) reaches the backend with the frame it belongs to (report 11).
    const bool dump = gBenchDumpPath[0] != '\0';
    if (dump && !Recording()) {
        strncpy(gBenchDumpPathGL, gBenchDumpPath, sizeof(gBenchDumpPathGL) - 1);
    }
#endif
    mCorrupt = mChecks ? CVarInt("gSettings.ProbeRThreadCorrupt", 0) : 0;
    mLazyBinds = mInner->WillBindLazily();
    if (!Recording()) {
        mInner->StartFrame();
        return;
    }
    Count(BENCH_RT_FRAMES, 1);
    // What the backend's StartFrame reads from the CVar store, read here (report 11 §11.4).
    const size_t n = mInner->FrameSettingsSize();
    auto* x = Emit<StartArgs>(Op::StartFrame, n);
    x->settingsBytes = (uint32_t)n;
    mInner->CaptureFrameSettings(reinterpret_cast<uint8_t*>(x) + sizeof(StartArgs));
#ifdef SOH_BENCHMARK
    if (dump) {
        const size_t len = strnlen(gBenchDumpPath, sizeof(gBenchDumpPath) - 1);
        char* p = Emit<char>(Op::DumpPath, len + 1);
        memcpy(p, gBenchDumpPath, len);
        p[len] = '\0';
    }
#endif
}

void GfxRenderingAPIThreaded::EndFrame() {
    if (!Recording()) {
        mInner->EndFrame();
    } else {
        Emit<OneInt>(Op::EndFrame);
        if (mMode == 2) {
            Publish();
        } else {
            Drain();
        }
    }
#ifdef SOH_BENCHMARK
    // The backend dumps from its own copy; this thread's is done with at the end of the frame.
    gBenchDumpPath[0] = '\0';
#endif
}

void GfxRenderingAPIThreaded::RunInOrder(void (*fn)(void*), void* arg) {
    if (!Recording()) {
        fn(arg);
        return;
    }
    *Emit<CallbackArgs>(Op::RunInOrder) = { fn, arg };
}

void GfxRenderingAPIThreaded::RunNow(void (*fn)(void*), void* arg) {
    Call([&] { fn(arg); });
}

// The swap. Mode 2 lets this thread run one frame ahead of the GL thread and no more.
void GfxRenderingAPIThreaded::RunAtFrameEnd(void (*fn)(void*), void* arg) {
    if (mMode != 2) {
        Drain();
        fn(arg);
        return;
    }
    *Emit<CallbackArgs>(Op::FrameEnd) = { fn, arg };
    Publish();
    {
        std::lock_guard<std::mutex> lk(mMutex);
        mFramesQueued++;
    }
    WaitWhile([&] { return mFramesQueued - mFramesDone > 1; });
}

void GfxRenderingAPIThreaded::UnloadShader(ShaderProgram* oldPrg) {
    if (!Recording()) {
        mInner->UnloadShader(oldPrg);
        return;
    }
    Emit<PrgArgs>(Op::UnloadShader)->prg = oldPrg;
}

void GfxRenderingAPIThreaded::LoadShader(ShaderProgram* newPrg) {
    if (!Recording()) {
        mInner->LoadShader(newPrg);
        return;
    }
    Emit<PrgArgs>(Op::LoadShader)->prg = newPrg;
}

void GfxRenderingAPIThreaded::SelectTexture(int tile, uint32_t textureId) {
    if (tile >= 0 && tile < (int)kMaxTiles) {
        mSelected[tile] = textureId;
    }
    if (!Recording()) {
        mInner->SelectTexture(tile, textureId);
        return;
    }
    auto* x = Emit<TileTexArgs>(Op::SelectTexture);
    x->tile = tile;
    x->tex = textureId;
}

void GfxRenderingAPIThreaded::UploadTexture(const uint8_t* rgba32Buf, uint32_t width, uint32_t height) {
    if (!Recording()) {
        mInner->UploadTexture(rgba32Buf, width, height);
        return;
    }
    const size_t bytes = (size_t)width * height * 4;
    auto* x = Emit<UploadArgs>(Op::UploadTexture, bytes);
    x->width = width;
    x->height = height;
    memcpy(reinterpret_cast<uint8_t*>(x) + sizeof(UploadArgs), rgba32Buf, bytes);
}

void GfxRenderingAPIThreaded::SetSamplerParameters(int sampler, bool linear_filter, uint32_t cms, uint32_t cmt) {
    if (!Recording()) {
        mInner->SetSamplerParameters(sampler, linear_filter, cms, cmt);
        return;
    }
    auto* x = Emit<SamplerArgs>(Op::SetSamplerParameters);
    x->sampler = sampler;
    x->linear = linear_filter;
    x->cms = cms;
    x->cmt = cmt;
}

void GfxRenderingAPIThreaded::SetDepthTestAndMask(bool depth_test, bool z_upd) {
    if (!Recording()) {
        mInner->SetDepthTestAndMask(depth_test, z_upd);
        return;
    }
    *Emit<TwoBools>(Op::SetDepthTestAndMask) = { depth_test, z_upd };
}

void GfxRenderingAPIThreaded::SetZmodeDecal(bool decal) {
    if (!Recording()) {
        mInner->SetZmodeDecal(decal);
        return;
    }
    Emit<OneBool>(Op::SetZmodeDecal)->a = decal;
}

void GfxRenderingAPIThreaded::SetViewport(int x, int y, int width, int height) {
    if (!Recording()) {
        mInner->SetViewport(x, y, width, height);
        return;
    }
    *Emit<RectArgs>(Op::SetViewport) = { x, y, width, height };
}

void GfxRenderingAPIThreaded::SetScissor(int x, int y, int width, int height) {
    if (!Recording()) {
        mInner->SetScissor(x, y, width, height);
        return;
    }
    *Emit<RectArgs>(Op::SetScissor) = { x, y, width, height };
}

void GfxRenderingAPIThreaded::SetUseAlpha(bool useAlpha) {
    if (!Recording()) {
        mInner->SetUseAlpha(useAlpha);
        return;
    }
    Emit<OneBool>(Op::SetUseAlpha)->a = useAlpha;
}

void GfxRenderingAPIThreaded::DrawTriangles(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris) {
    if (!Recording()) {
        mInner->DrawTriangles(buf_vbo, buf_vbo_len, buf_vbo_num_tris);
        return;
    }
    // The interpreter reuses its buffer after the call: the vertices are copied (report 11 §11.5).
    auto* x = Emit<DrawArgs>(Op::DrawTriangles, buf_vbo_len * sizeof(float));
    x->len = (uint32_t)buf_vbo_len;
    x->tris = (uint32_t)buf_vbo_num_tris;
    memcpy(reinterpret_cast<uint8_t*>(x) + sizeof(DrawArgs), buf_vbo, buf_vbo_len * sizeof(float));
}

void GfxRenderingAPIThreaded::OnResize() {
    if (!Recording()) {
        mInner->OnResize();
        return;
    }
    Emit<OneInt>(Op::OnResize);
}

void GfxRenderingAPIThreaded::UpdateFramebufferParameters(int fb_id, uint32_t width, uint32_t height,
                                                          uint32_t msaa_level, bool opengl_invertY, bool render_target,
                                                          bool has_depth_buffer, bool can_extract_depth) {
    if (fb_id >= 0) {
        if (fb_id >= (int)mFbInvertY.size()) {
            mFbInvertY.resize(fb_id + 1, false);
        }
        mFbInvertY[fb_id] = opengl_invertY;
    }
    if (!Recording()) {
        mInner->UpdateFramebufferParameters(fb_id, width, height, msaa_level, opengl_invertY, render_target,
                                            has_depth_buffer, can_extract_depth);
        return;
    }
    *Emit<FbParamArgs>(Op::UpdateFramebufferParameters) = {
        fb_id, width, height, msaa_level, opengl_invertY, render_target, has_depth_buffer, can_extract_depth
    };
}

void GfxRenderingAPIThreaded::StartDrawToFramebuffer(int fbId, float noiseScale) {
    mCurrentFb = fbId;
    if (!Recording()) {
        mInner->StartDrawToFramebuffer(fbId, noiseScale);
        return;
    }
    *Emit<StartFbArgs>(Op::StartDrawToFramebuffer) = { fbId, noiseScale };
}

void GfxRenderingAPIThreaded::CopyFramebuffer(int fbDstId, int fbSrcId, int srcX0, int srcY0, int srcX1, int srcY1,
                                              int dstX0, int dstY0, int dstX1, int dstY1) {
    if (!Recording()) {
        mInner->CopyFramebuffer(fbDstId, fbSrcId, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1);
        return;
    }
    *Emit<CopyFbArgs>(
        Op::CopyFramebuffer) = { fbDstId, fbSrcId, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1 };
}

void GfxRenderingAPIThreaded::ClearFramebuffer(bool color, bool depth) {
    if (!Recording()) {
        mInner->ClearFramebuffer(color, depth);
        return;
    }
    *Emit<TwoBools>(Op::ClearFramebuffer) = { color, depth };
}

void GfxRenderingAPIThreaded::DiscardDepthStencil() {
    if (!Recording()) {
        mInner->DiscardDepthStencil();
        return;
    }
    Emit<OneInt>(Op::DiscardDepthStencil);
}

void GfxRenderingAPIThreaded::ClearShaderCache() {
    // Every program the proxy knows is about to be freed; the ones still in the stream are used
    // before the clear reaches the backend.
    mShaders.clear();
    if (!Recording()) {
        mInner->ClearShaderCache();
        return;
    }
    Emit<OneInt>(Op::ClearShaderCache);
}

void GfxRenderingAPIThreaded::FlushDeferredDraws() {
    if (!Recording()) {
        mInner->FlushDeferredDraws();
        return;
    }
    Emit<OneInt>(Op::FlushDeferredDraws);
}

void GfxRenderingAPIThreaded::ResolveMSAAColorBuffer(int fbIdTarger, int fbIdSrc) {
    if (!Recording()) {
        mInner->ResolveMSAAColorBuffer(fbIdTarger, fbIdSrc);
        return;
    }
    *Emit<TwoInts>(Op::ResolveMSAAColorBuffer) = { fbIdTarger, fbIdSrc };
}

void GfxRenderingAPIThreaded::SelectTextureFb(int fbId) {
    // The backend selects the framebuffer's colour texture on tile 0 through SelectTexture.
    if (fbId >= 0 && fbId < (int)mFbTextures.size()) {
        mSelected[0] = (uint32_t)(uintptr_t)mFbTextures[fbId];
    }
    if (!Recording()) {
        mInner->SelectTextureFb(fbId);
        return;
    }
    Emit<OneInt>(Op::SelectTextureFb)->a = fbId;
}

void GfxRenderingAPIThreaded::DeleteTexture(uint32_t texId) {
    if (!Recording()) {
        mInner->DeleteTexture(texId);
        return;
    }
    Emit<OneInt>(Op::DeleteTexture)->a = (int32_t)texId;
}

void GfxRenderingAPIThreaded::SetSrgbMode() {
    if (!Recording()) {
        mInner->SetSrgbMode();
        return;
    }
    Emit<OneInt>(Op::SetSrgbMode);
}

} // namespace Fast
