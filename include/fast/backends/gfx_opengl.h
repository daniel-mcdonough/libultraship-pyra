#ifdef ENABLE_OPENGL
#pragma once

#include "gfx_rendering_api.h"
#include "../interpreter.h"

#ifdef _MSC_VER
#include <SDL2/SDL.h>
// #define GL_GLEXT_PROTOTYPES 1
#include <GL/glew.h>
#elif FOR_WINDOWS
#include <GL/glew.h>
#include "SDL.h"
#define GL_GLEXT_PROTOTYPES 1
#include "SDL_opengl.h"
#elif __APPLE__
#include <SDL2/SDL.h>
#include <GL/glew.h>
#elif USE_OPENGLES
#include <SDL2/SDL.h>
#ifdef USE_OPENGLES2
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#else
#include <GLES3/gl3.h>
#endif
#else
#include <SDL2/SDL.h>
#define GL_GLEXT_PROTOTYPES 1
#include <SDL2/SDL_opengl.h>
#endif
namespace Fast {

// Bits of CVAR_SHADER_OPTS. Both are fragment-shader economies, measured on the Pyra and on by
// default since; 0 is the shader as generated upstream. A bitmask rather than two CVars because
// the benchmark harness switches arms by setting a single CVar, and because the pair can then be
// split afterwards without adding a second one - which is how the two were shown not to be
// additive (the wrap elision is essentially the whole win). The default lives at the one read
// site in CreateAndLoadNewShader.
enum ShaderOptBits {
    // Emit the combiner's WRAP() only for cycles whose output can actually leave [0,1].
    SHADER_OPT_ELIDE_NOOP_WRAP = 1 << 0,
    // Pass texture and mask dimensions as vec2 rather than as int pairs converted per fragment.
    SHADER_OPT_FLOAT_TEX_SIZE = 1 << 1,
};

struct ShaderProgram {
    GLuint openglProgramId;
    uint8_t numInputs;
    bool usedTextures[SHADER_MAX_TEXTURES];
    uint8_t numFloats;
    GLint attribLocations[16];
    uint8_t attribSizes[16];
    uint8_t numAttribs;
    GLint frameCountLocation;
    GLint noiseScaleLocation;
    GLint texture_width_location;
    GLint texture_height_location;
    GLint texture_filtering_location;
    GLint mask_width_location;
    GLint mask_height_location;
    GLint texture_size_location;
    GLint mask_size_location;
    // Which of the two uniform layouts this program was compiled with. Held per program rather
    // than read from the CVar at draw time so that a program always outlives its own layout.
    bool floatTexSize;

    // gSettings.LeanGLState >= 2: the values this program's uniforms last received, so that a
    // draw which would send the same values again sends nothing. Uniform values live in the
    // program object and only this backend writes them, so the shadow cannot go stale.
    bool texUniformsValid = false;
    bool maskUniformsValid = false;
    bool frameUniformsValid = false;
    GLint lastFiltering[2] = {};
    GLfloat lastTexSize[4] = {};
    GLint lastTexWidth[2] = {}, lastTexHeight[2] = {};
    GLfloat lastMaskSize[4] = {};
    GLint lastMaskWidth[2] = {}, lastMaskHeight[2] = {};
    int lastFrameCount = 0;
    float lastNoiseScale = 0.0f;
};

struct FramebufferOGL {
    uint32_t width, height;
    bool has_depth_buffer;
    uint32_t msaa_level;
    bool invertY;

    GLuint fbo, clrbuf, clrbufMsaa, rbo;
};

struct TextureInfo {
    uint16_t width;
    uint16_t height;
    uint16_t filtering;
};

class GfxRenderingAPIOGL final : public GfxRenderingAPI {
  public:
    ~GfxRenderingAPIOGL() override = default;
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
#ifdef SOH_BENCHMARK
    void DumpFramebuffer(int fb_id, const char* path);
#endif
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

  private:
    void SetUniforms(ShaderProgram* prg) const;
    std::string BuildFsShader(const CCFeatures& cc_features, int shaderOpts);
    void SetPerDrawUniforms();
    void SetVertexAttribsLean(ShaderProgram* prg);
    void ForgetVertexAttribs(bool disableAll);
    void BindArrayBuffer(GLuint buffer);

    // gSettings.LeanGLState (report 8 §8.24), read once per frame. 1: vertex attribute arrays are
    // enabled, disabled and re-pointed only where the new program differs from what is already
    // set, instead of all disabled on every shader unload and all re-specified on every load.
    // 2: also skip uniform uploads that would resend a program's current values.
    // 3: also draw from client-side vertex arrays instead of a glBufferData per draw.
    int mLeanGLState = 0;
#ifdef SOH_BENCHMARK
    // gSettings.ProbeSkipDraw, read each frame (report 8 §8.36).
    int mBenchSkipDraw = 0;
#endif
    // What the context's vertex attribute arrays are known to hold, for mode >= 1. GLES 2 has no
    // VAOs, so this is global context state; anything else that touches it must call
    // ForgetVertexAttribs, and StartFrame re-establishes it from nothing every frame.
    struct AttribPointer {
        bool valid = false;
        uint8_t size = 0;
        uint16_t stride = 0;
        GLuint buffer = 0;
        uintptr_t pointer = 0; // an offset into `buffer`, or an address when `buffer` is 0
    };
    static constexpr int kMaxShadowAttribs = 16;
    AttribPointer mAttribPointers[kMaxShadowAttribs];
    uint32_t mAttribEnabled = 0;
    int mMaxVertexAttribs = 8;
    // The GL_ARRAY_BUFFER binding as this backend last set it, and for mode 3 the address the
    // interpreter's vertex buffer lives at (it is allocated once and never moves).
    GLuint mBoundArrayBuffer = 0;
    bool mBoundArrayBufferKnown = false;
    const float* mClientVertexBase = nullptr;

    std::vector<TextureInfo> textures;
    GLuint mCurrentTextureIds[SHADER_MAX_TEXTURES];
    GLuint mLastBoundTextures[SHADER_MAX_TEXTURES] = {};
    uint8_t mCurrentTile;
    int8_t mLastActiveTexture = -1;
    int8_t mLastBlendEnabled = -1;
    int8_t mLastScissorEnabled = -1;

    std::map<std::pair<uint64_t, uint32_t>, ShaderProgram> mShaderProgramPool;
    ShaderProgram* mCurrentShaderProgram;
    ShaderProgram* mLastLoadedShader = nullptr;

    GLuint mOpenglVbo = 0;
#if defined(__APPLE__) || (defined(USE_OPENGLES) && !defined(USE_OPENGLES2))
    GLuint mOpenglVao;
#endif

    uint32_t mFrameCount = 0;

    std::vector<FramebufferOGL> mFrameBuffers;
    size_t mCurrentFrameBuffer = 0;
    float mCurrentNoiseScale = 0.0f;
    FilteringMode mCurrentFilterMode = FILTER_THREE_POINT;

    GLint mMaxMsaaLevel = 1;
#ifdef USE_OPENGLES2
    // GLES 2.0 has no glBlitFramebuffer; framebuffer copies go through a quad.
    GLuint mBlitProgram = 0;
    GLint mBlitTexLocation = -1;
    void EnsureBlitProgram();
    void BlitFramebufferAsQuad(GLuint dstFbo, GLuint srcTex, float u0, float v0, float u1, float v1, int dstX0,
                               int dstY0, int dstX1, int dstY1);
#endif
    GLuint mPixelDepthRb = 0;
    GLuint mPixelDepthFb = 0;
    size_t mPixelDepthRbSize = 0;
};

} // namespace Fast
#endif
