#include "ship/window/Window.h"
#ifdef ENABLE_OPENGL

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <chrono>
#include <map>
#include <unordered_map>

#ifndef _LANGUAGE_C
#define _LANGUAGE_C
#endif

#ifdef __MINGW32__
#define FOR_WINDOWS 1
#else
#define FOR_WINDOWS 0
#endif

#include "fast/backends/gfx_opengl.h"
#include "ship/window/gui/Gui.h"
#include <prism/processor.h>
#include <fstream>
#include "ship/Context.h"
#include "ship/resource/factory/ShaderFactory.h"
#include "fast/interpreter.h"
#include "ship/config/ConsoleVariable.h"

#include <sstream>

#ifdef SOH_BENCHMARK
// Count every call into the GL driver: BENCH_GL_CALLS per frame, and gBenchGlFn per entry point,
// cumulative, which the harness snapshots at segment start and end (report 8 §8.24). These are
// function-like macros over the real entry points, so they must stay below the last #include: a
// GL header seen after them would have its prototypes rewritten. A name used below and missing
// from the list goes uncounted.
#define BENCH_GL_FNS(X, U) \
    X(glActiveTexture) \
    X(glAttachShader) \
    X(glBindAttribLocation) \
    X(glBindBuffer) \
    X(glBindFramebuffer) \
    X(glBindRenderbuffer) \
    X(glBindTexture) \
    X(glBindVertexArray) \
    X(glBlendFunc) \
    X(glBlitFramebuffer) \
    X(glBufferData) \
    X(glClear) \
    X(glClearColor) \
    X(glCompileShader) \
    X(glCreateProgram) \
    X(glCreateShader) \
    X(glDeleteProgram) \
    X(glDeleteRenderbuffers) \
    X(glDeleteShader) \
    X(glDeleteTextures) \
    X(glDepthFunc) \
    X(glDepthMask) \
    X(glDisable) \
    X(glDisableVertexAttribArray) \
    X(glDrawArrays) \
    X(glEnable) \
    X(glEnableVertexAttribArray) \
    X(glFinish) \
    X(glFlush) \
    X(glFramebufferRenderbuffer) \
    X(glFramebufferTexture2D) \
    X(glGenBuffers) \
    X(glGenFramebuffers) \
    X(glGenRenderbuffers) \
    X(glGenTextures) \
    X(glGenVertexArrays) \
    X(glGetAttribLocation) \
    X(glGetIntegerv) \
    X(glGetProgramInfoLog) \
    X(glGetProgramiv) \
    X(glGetShaderInfoLog) \
    X(glGetShaderiv) \
    X(glGetString) \
    X(glGetUniformLocation) \
    X(glLinkProgram) \
    X(glPolygonOffset) \
    X(glReadBuffer) \
    X(glReadPixels) \
    X(glRenderbufferStorage) \
    X(glRenderbufferStorageMultisample) \
    X(glScissor) \
    X(glShaderSource) \
    X(glTexImage2D) \
    X(glTexParameteri) \
    X(glUseProgram) \
    X(glVertexAttribPointer) \
    X(glViewport) \
    U(glUniform1f) \
    U(glUniform1i) \
    U(glUniform1iv) \
    U(glUniform2fv)
#define BENCH_GL_ENUM(n) BGL_##n,
enum BenchGlFn { BENCH_GL_FNS(BENCH_GL_ENUM, BENCH_GL_ENUM) BGL_COUNT };
#define BENCH_GL_NAME(n) #n,
extern "C" {
uint64_t gBenchGlFn[BGL_COUNT] = {};
extern const char* const gBenchGlFnNames[BGL_COUNT] = { BENCH_GL_FNS(BENCH_GL_NAME, BENCH_GL_NAME) };
extern const int gBenchGlFnCount = BGL_COUNT;
}
#define BENCH_GL_CALL(fn, ...) (BENCH_COUNT(BENCH_GL_CALLS, 1), gBenchGlFn[BGL_##fn]++, fn(__VA_ARGS__))
#define BENCH_GL_UNIFORM(fn, ...) (BENCH_COUNT(BENCH_GL_UNIFORMS, 1), BENCH_GL_CALL(fn, __VA_ARGS__))
#define glActiveTexture(...) BENCH_GL_CALL(glActiveTexture, __VA_ARGS__)
#define glAttachShader(...) BENCH_GL_CALL(glAttachShader, __VA_ARGS__)
#define glBindAttribLocation(...) BENCH_GL_CALL(glBindAttribLocation, __VA_ARGS__)
#define glBindBuffer(...) BENCH_GL_CALL(glBindBuffer, __VA_ARGS__)
#define glBindFramebuffer(...) BENCH_GL_CALL(glBindFramebuffer, __VA_ARGS__)
#define glBindRenderbuffer(...) BENCH_GL_CALL(glBindRenderbuffer, __VA_ARGS__)
#define glBindTexture(...) BENCH_GL_CALL(glBindTexture, __VA_ARGS__)
#define glBindVertexArray(...) BENCH_GL_CALL(glBindVertexArray, __VA_ARGS__)
#define glBlendFunc(...) BENCH_GL_CALL(glBlendFunc, __VA_ARGS__)
#define glBlitFramebuffer(...) BENCH_GL_CALL(glBlitFramebuffer, __VA_ARGS__)
#define glBufferData(...) BENCH_GL_CALL(glBufferData, __VA_ARGS__)
#define glClear(...) BENCH_GL_CALL(glClear, __VA_ARGS__)
#define glClearColor(...) BENCH_GL_CALL(glClearColor, __VA_ARGS__)
#define glCompileShader(...) BENCH_GL_CALL(glCompileShader, __VA_ARGS__)
#define glCreateProgram(...) BENCH_GL_CALL(glCreateProgram, __VA_ARGS__)
#define glCreateShader(...) BENCH_GL_CALL(glCreateShader, __VA_ARGS__)
#define glDeleteProgram(...) BENCH_GL_CALL(glDeleteProgram, __VA_ARGS__)
#define glDeleteRenderbuffers(...) BENCH_GL_CALL(glDeleteRenderbuffers, __VA_ARGS__)
#define glDeleteShader(...) BENCH_GL_CALL(glDeleteShader, __VA_ARGS__)
#define glDeleteTextures(...) BENCH_GL_CALL(glDeleteTextures, __VA_ARGS__)
#define glDepthFunc(...) BENCH_GL_CALL(glDepthFunc, __VA_ARGS__)
#define glDepthMask(...) BENCH_GL_CALL(glDepthMask, __VA_ARGS__)
#define glDisable(...) BENCH_GL_CALL(glDisable, __VA_ARGS__)
#define glDisableVertexAttribArray(...) BENCH_GL_CALL(glDisableVertexAttribArray, __VA_ARGS__)
#define glDrawArrays(...) BENCH_GL_CALL(glDrawArrays, __VA_ARGS__)
#define glEnable(...) BENCH_GL_CALL(glEnable, __VA_ARGS__)
#define glEnableVertexAttribArray(...) BENCH_GL_CALL(glEnableVertexAttribArray, __VA_ARGS__)
#define glFinish(...) BENCH_GL_CALL(glFinish, __VA_ARGS__)
#define glFlush(...) BENCH_GL_CALL(glFlush, __VA_ARGS__)
#define glFramebufferRenderbuffer(...) BENCH_GL_CALL(glFramebufferRenderbuffer, __VA_ARGS__)
#define glFramebufferTexture2D(...) BENCH_GL_CALL(glFramebufferTexture2D, __VA_ARGS__)
#define glGenBuffers(...) BENCH_GL_CALL(glGenBuffers, __VA_ARGS__)
#define glGenFramebuffers(...) BENCH_GL_CALL(glGenFramebuffers, __VA_ARGS__)
#define glGenRenderbuffers(...) BENCH_GL_CALL(glGenRenderbuffers, __VA_ARGS__)
#define glGenTextures(...) BENCH_GL_CALL(glGenTextures, __VA_ARGS__)
#define glGenVertexArrays(...) BENCH_GL_CALL(glGenVertexArrays, __VA_ARGS__)
#define glGetAttribLocation(...) BENCH_GL_CALL(glGetAttribLocation, __VA_ARGS__)
#define glGetIntegerv(...) BENCH_GL_CALL(glGetIntegerv, __VA_ARGS__)
#define glGetProgramInfoLog(...) BENCH_GL_CALL(glGetProgramInfoLog, __VA_ARGS__)
#define glGetProgramiv(...) BENCH_GL_CALL(glGetProgramiv, __VA_ARGS__)
#define glGetShaderInfoLog(...) BENCH_GL_CALL(glGetShaderInfoLog, __VA_ARGS__)
#define glGetShaderiv(...) BENCH_GL_CALL(glGetShaderiv, __VA_ARGS__)
#define glGetString(...) BENCH_GL_CALL(glGetString, __VA_ARGS__)
#define glGetUniformLocation(...) BENCH_GL_CALL(glGetUniformLocation, __VA_ARGS__)
#define glLinkProgram(...) BENCH_GL_CALL(glLinkProgram, __VA_ARGS__)
#define glPolygonOffset(...) BENCH_GL_CALL(glPolygonOffset, __VA_ARGS__)
#define glReadBuffer(...) BENCH_GL_CALL(glReadBuffer, __VA_ARGS__)
#define glReadPixels(...) BENCH_GL_CALL(glReadPixels, __VA_ARGS__)
#define glRenderbufferStorage(...) BENCH_GL_CALL(glRenderbufferStorage, __VA_ARGS__)
#define glRenderbufferStorageMultisample(...) BENCH_GL_CALL(glRenderbufferStorageMultisample, __VA_ARGS__)
#define glScissor(...) BENCH_GL_CALL(glScissor, __VA_ARGS__)
#define glShaderSource(...) BENCH_GL_CALL(glShaderSource, __VA_ARGS__)
#define glTexImage2D(...) BENCH_GL_CALL(glTexImage2D, __VA_ARGS__)
#define glTexParameteri(...) BENCH_GL_CALL(glTexParameteri, __VA_ARGS__)
#define glUseProgram(...) BENCH_GL_CALL(glUseProgram, __VA_ARGS__)
#define glVertexAttribPointer(...) BENCH_GL_CALL(glVertexAttribPointer, __VA_ARGS__)
#define glViewport(...) BENCH_GL_CALL(glViewport, __VA_ARGS__)
#define glUniform1f(...) BENCH_GL_UNIFORM(glUniform1f, __VA_ARGS__)
#define glUniform1i(...) BENCH_GL_UNIFORM(glUniform1i, __VA_ARGS__)
#define glUniform1iv(...) BENCH_GL_UNIFORM(glUniform1iv, __VA_ARGS__)
#define glUniform2fv(...) BENCH_GL_UNIFORM(glUniform2fv, __VA_ARGS__)
#endif

namespace Fast {

#ifdef USE_OPENGLES2
// GLES 2.0 has no sized colour formats: glTexImage2D requires internalformat to
// equal format. (SGX544 does expose OES_required_internalformat + OES_rgb8_rgba8,
// which would allow the sized tokens, but not every ES2 driver does.) There is
// also a single framebuffer binding point rather than a read/draw pair.
#define GL_RGBA8 GL_RGBA
#define GL_RGB8 GL_RGB
#define GL_READ_FRAMEBUFFER GL_FRAMEBUFFER
#define GL_DRAW_FRAMEBUFFER GL_FRAMEBUFFER
#ifndef GL_DEPTH24_STENCIL8
#define GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8_OES
#endif
#endif

// GLES 2.0 has no combined depth/stencil attachment point, so a packed
// renderbuffer (OES_packed_depth_stencil) is attached to both points instead.
static void AttachDepthStencil(GLuint rbo) {
#ifdef USE_OPENGLES2
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
#else
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
#endif
}

#ifdef USE_OPENGLES2
// GL_EXT_discard_framebuffer. A tile-based deferred renderer resolves the depth/stencil buffer
// out to main memory at the end of every render pass unless it is told the contents are dead.
// Nothing in Fast3D ever reads depth back, so that writeback is pure waste on such a GPU.
//
// The entry point must be resolved at runtime. GLES does not require extension functions to be
// exported, and the PowerVR SGX DDK in particular exports only the core GLES 2.0 symbols and no
// EXT/OES entry points at all, so linking against the name directly would fail to link.
#ifndef GL_COLOR_EXT
#define GL_COLOR_EXT 0x1800
#endif
#ifndef GL_DEPTH_EXT
#define GL_DEPTH_EXT 0x1801
#endif
#ifndef GL_STENCIL_EXT
#define GL_STENCIL_EXT 0x1802
#endif

typedef void(GL_APIENTRYP SohPfnDiscardFramebufferEXT)(GLenum target, GLsizei numAttachments,
                                                       const GLenum* attachments);
static SohPfnDiscardFramebufferEXT sDiscardFramebufferEXT = nullptr;

// glGetString(GL_EXTENSIONS) is one space-delimited string in GLES 2.0 (glGetStringi is ES 3.0).
// Match whole tokens so that a name is not mistaken for the prefix of a longer one.
static bool HasGLExtension(const char* name) {
    const char* exts = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    if (exts == nullptr || name == nullptr) {
        return false;
    }
    const size_t len = strlen(name);
    for (const char* p = strstr(exts, name); p != nullptr; p = strstr(p + len, name)) {
        if ((p == exts || p[-1] == ' ') && (p[len] == ' ' || p[len] == '\0')) {
            return true;
        }
    }
    return false;
}
#endif

// Multisampled renderbuffers are not in GLES 2.0. mMaxMsaaLevel is pinned to 1
// there, which makes every caller single-sampled; this only has to compile.
static void RenderbufferStorageMsaa(GLsizei samples, GLenum format, GLsizei width, GLsizei height) {
#ifdef USE_OPENGLES2
    (void)samples;
    glRenderbufferStorage(GL_RENDERBUFFER, format, width, height);
#else
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, format, width, height);
#endif
}
int GfxRenderingAPIOGL::GetMaxTextureSize() {
    GLint max_texture_size;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
    return max_texture_size;
}

const char* GfxRenderingAPIOGL::GetName() {
    return "OpenGL";
}

GfxClipParameters GfxRenderingAPIOGL::GetClipParameters() {
    return { false, mFrameBuffers[mCurrentFrameBuffer].invertY };
}

static void VertexArraySetAttribs(ShaderProgram* prg) {
    size_t numFloats = prg->numFloats;
    size_t pos = 0;

    for (int i = 0; i < prg->numAttribs; i++) {
        if (prg->attribLocations[i] >= 0) {
            glEnableVertexAttribArray(prg->attribLocations[i]);
            glVertexAttribPointer(prg->attribLocations[i], prg->attribSizes[i], GL_FLOAT, GL_FALSE,
                                  numFloats * sizeof(float), (void*)(pos * sizeof(float)));
        }
        pos += prg->attribSizes[i];
    }
}

void GfxRenderingAPIOGL::SetUniforms(ShaderProgram* prg) const {
    if (mLeanGLState >= 2) {
        if (prg->frameUniformsValid && prg->lastFrameCount == (int)mFrameCount &&
            prg->lastNoiseScale == mCurrentNoiseScale) {
            return;
        }
        prg->frameUniformsValid = true;
        prg->lastFrameCount = (int)mFrameCount;
        prg->lastNoiseScale = mCurrentNoiseScale;
    }
    glUniform1i(prg->frameCountLocation, mFrameCount);
    glUniform1f(prg->noiseScaleLocation, mCurrentNoiseScale);
}

// Mode >= 2: true when a uniform upload would change what the program holds, recording the new
// values as it says so. `valid` is false until the program has received the uniform once.
template <typename T, size_t N> static bool UniformChanged(bool& valid, T (&last)[N], const T (&now)[N]) {
    if (valid && memcmp(last, now, sizeof(last)) == 0) {
        return false;
    }
    valid = true;
    memcpy(last, now, sizeof(last));
    return true;
}

void GfxRenderingAPIOGL::SetPerDrawUniforms() {
    ShaderProgram* prg = mCurrentShaderProgram;
    const bool lean = mLeanGLState >= 2;
    if (prg->usedTextures[0] || prg->usedTextures[1]) {
        const GLint filtering[2] = { textures[mCurrentTextureIds[0]].filtering, textures[mCurrentTextureIds[1]].filtering };
        if (prg->floatTexSize) {
            const GLfloat size[4] = { (GLfloat)textures[mCurrentTextureIds[0]].width,
                                      (GLfloat)textures[mCurrentTextureIds[0]].height,
                                      (GLfloat)textures[mCurrentTextureIds[1]].width,
                                      (GLfloat)textures[mCurrentTextureIds[1]].height };
            // Filtering and size are one group: both or neither, so the validity flag is shared.
            bool v1 = prg->texUniformsValid, v2 = prg->texUniformsValid;
            const bool fChanged = !lean || UniformChanged(v1, prg->lastFiltering, filtering);
            const bool sChanged = !lean || UniformChanged(v2, prg->lastTexSize, size);
            prg->texUniformsValid = lean;
            if (fChanged) {
                glUniform1iv(prg->texture_filtering_location, 2, filtering);
            }
            if (sChanged) {
                glUniform2fv(prg->texture_size_location, 2, size);
            }
        } else {
            const GLint width[2] = { textures[mCurrentTextureIds[0]].width, textures[mCurrentTextureIds[1]].width };
            const GLint height[2] = { textures[mCurrentTextureIds[0]].height, textures[mCurrentTextureIds[1]].height };
            bool v1 = prg->texUniformsValid, v2 = prg->texUniformsValid, v3 = prg->texUniformsValid;
            const bool fChanged = !lean || UniformChanged(v1, prg->lastFiltering, filtering);
            const bool wChanged = !lean || UniformChanged(v2, prg->lastTexWidth, width);
            const bool hChanged = !lean || UniformChanged(v3, prg->lastTexHeight, height);
            prg->texUniformsValid = lean;
            if (fChanged) {
                glUniform1iv(prg->texture_filtering_location, 2, filtering);
            }
            if (wChanged) {
                glUniform1iv(prg->texture_width_location, 2, width);
            }
            if (hChanged) {
                glUniform1iv(prg->texture_height_location, 2, height);
            }
        }
    }

    // Mask dimensions replace textureSize(), which GLSL ES 1.00 does not have.
    if (prg->usedTextures[2] || prg->usedTextures[3]) {
        if (prg->floatTexSize) {
            const GLfloat size[4] = { (GLfloat)textures[mCurrentTextureIds[2]].width,
                                      (GLfloat)textures[mCurrentTextureIds[2]].height,
                                      (GLfloat)textures[mCurrentTextureIds[3]].width,
                                      (GLfloat)textures[mCurrentTextureIds[3]].height };
            bool v = prg->maskUniformsValid;
            const bool changed = !lean || UniformChanged(v, prg->lastMaskSize, size);
            prg->maskUniformsValid = lean;
            if (changed) {
                glUniform2fv(prg->mask_size_location, 2, size);
            }
        } else {
            const GLint width[2] = { textures[mCurrentTextureIds[2]].width, textures[mCurrentTextureIds[3]].width };
            const GLint height[2] = { textures[mCurrentTextureIds[2]].height, textures[mCurrentTextureIds[3]].height };
            bool v1 = prg->maskUniformsValid, v2 = prg->maskUniformsValid;
            const bool wChanged = !lean || UniformChanged(v1, prg->lastMaskWidth, width);
            const bool hChanged = !lean || UniformChanged(v2, prg->lastMaskHeight, height);
            prg->maskUniformsValid = lean;
            if (wChanged) {
                glUniform1iv(prg->mask_width_location, 2, width);
            }
            if (hChanged) {
                glUniform1iv(prg->mask_height_location, 2, height);
            }
        }
    }
}

void GfxRenderingAPIOGL::UnloadShader(ShaderProgram* old_prg) {
    if (mLeanGLState >= 1) {
        // The next LoadShader disables only what the new program does not use.
        if (old_prg != nullptr && old_prg == mLastLoadedShader) {
            mLastLoadedShader = nullptr;
        }
        return;
    }
    if (old_prg != nullptr && old_prg == mLastLoadedShader) {
        for (unsigned int i = 0; i < old_prg->numAttribs; i++) {
            if (old_prg->attribLocations[i] >= 0) {
                glDisableVertexAttribArray(old_prg->attribLocations[i]);
            }
        }
        mLastLoadedShader = nullptr;
    }
}

// Mode >= 1: bring the context's attribute arrays to what `prg` needs, touching only what differs.
// The layout is the same as VertexArraySetAttribs'; programs with the same attribute sizes share
// pointers, and most consecutive programs enable the same locations.
void GfxRenderingAPIOGL::SetVertexAttribsLean(ShaderProgram* prg) {
    // Mode 3 sources vertices from the interpreter's own array; before its first draw the address
    // is not known yet, and DrawTriangles re-applies the attributes once it is.
    const bool client = mLeanGLState >= 3 && mClientVertexBase != nullptr;
    BindArrayBuffer(client ? 0 : mOpenglVbo);
    const uintptr_t base = client ? (uintptr_t)mClientVertexBase : 0;
    const uint16_t stride = (uint16_t)(prg->numFloats * sizeof(float));
    uint32_t want = 0;
    size_t pos = 0;
    for (int i = 0; i < prg->numAttribs; i++) {
        const GLint loc = prg->attribLocations[i];
        const uintptr_t pointer = base + pos * sizeof(float);
        if (loc >= 0 && loc < kMaxShadowAttribs) {
            want |= 1u << loc;
            AttribPointer& p = mAttribPointers[loc];
            if (!p.valid || p.size != prg->attribSizes[i] || p.stride != stride || p.buffer != mBoundArrayBuffer ||
                p.pointer != pointer) {
                glVertexAttribPointer(loc, prg->attribSizes[i], GL_FLOAT, GL_FALSE, stride, (void*)pointer);
                p = { true, prg->attribSizes[i], stride, mBoundArrayBuffer, pointer };
            }
        } else if (loc >= 0) {
            // Beyond the shadow; never expected, but correct if it happens.
            glEnableVertexAttribArray(loc);
            glVertexAttribPointer(loc, prg->attribSizes[i], GL_FLOAT, GL_FALSE, stride, (void*)pointer);
        }
        pos += prg->attribSizes[i];
    }
    for (uint32_t on = want & ~mAttribEnabled; on != 0; on &= on - 1) {
        glEnableVertexAttribArray(__builtin_ctz(on));
    }
    for (uint32_t off = mAttribEnabled & ~want; off != 0; off &= off - 1) {
        glDisableVertexAttribArray(__builtin_ctz(off));
    }
    mAttribEnabled = want;
}

// The shadow no longer describes the context. With disableAll the arrays are put in a known state
// (all disabled) and the program in use re-applied; without it the caller has already left every
// array this backend enabled disabled.
void GfxRenderingAPIOGL::BindArrayBuffer(GLuint buffer) {
    if (!mBoundArrayBufferKnown || mBoundArrayBuffer != buffer) {
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        mBoundArrayBuffer = buffer;
        mBoundArrayBufferKnown = true;
    }
}

void GfxRenderingAPIOGL::ForgetVertexAttribs(bool disableAll) {
    for (auto& p : mAttribPointers) {
        p.valid = false;
    }
    mAttribEnabled = 0;
    mBoundArrayBufferKnown = false;
    if (disableAll) {
        // Modes 0-2 draw from the VBO and mode 0 never binds it itself.
        BindArrayBuffer(mOpenglVbo);
        for (int i = 0; i < std::min(mMaxVertexAttribs, (int)kMaxShadowAttribs); i++) {
            glDisableVertexAttribArray(i);
        }
        if (mLastLoadedShader != nullptr) {
            SetVertexAttribsLean(mLastLoadedShader);
        }
    }
}

void GfxRenderingAPIOGL::LoadShader(ShaderProgram* new_prg) {
    BENCH_COUNT(BENCH_SHADER_LOADS, 1);
    // if (!new_prg) return;
    mCurrentShaderProgram = new_prg;
    if (new_prg != mLastLoadedShader) {
        glUseProgram(new_prg->openglProgramId);
        if (mLeanGLState >= 1) {
            SetVertexAttribsLean(new_prg);
        } else {
            VertexArraySetAttribs(new_prg);
        }
        mLastLoadedShader = new_prg;
    }
    SetUniforms(new_prg);
}

#define RAND_NOISE "((random(vec3(floor(gl_FragCoord.xy * noise_scale), float(frame_count))) + 1.0) / 2.0)"

static const char* shader_item_to_str(uint32_t item, bool with_alpha, bool only_alpha, bool inputs_have_alpha,
                                      bool first_cycle, bool hint_single_element) {
    if (!only_alpha) {
        switch (item) {
            case SHADER_0:
                return with_alpha ? "vec4(0.0, 0.0, 0.0, 0.0)" : "vec3(0.0, 0.0, 0.0)";
            case SHADER_1:
                return with_alpha ? "vec4(1.0, 1.0, 1.0, 1.0)" : "vec3(1.0, 1.0, 1.0)";
            case SHADER_INPUT_1:
                return with_alpha || !inputs_have_alpha ? "vInput1" : "vInput1.rgb";
            case SHADER_INPUT_2:
                return with_alpha || !inputs_have_alpha ? "vInput2" : "vInput2.rgb";
            case SHADER_INPUT_3:
                return with_alpha || !inputs_have_alpha ? "vInput3" : "vInput3.rgb";
            case SHADER_INPUT_4:
                return with_alpha || !inputs_have_alpha ? "vInput4" : "vInput4.rgb";
            case SHADER_TEXEL0:
                return first_cycle ? (with_alpha ? "texVal0" : "texVal0.rgb")
                                   : (with_alpha ? "texVal1" : "texVal1.rgb");
            case SHADER_TEXEL0A:
                return first_cycle
                           ? (hint_single_element ? "texVal0.a"
                                                  : (with_alpha ? "vec4(texVal0.a, texVal0.a, texVal0.a, texVal0.a)"
                                                                : "vec3(texVal0.a, texVal0.a, texVal0.a)"))
                           : (hint_single_element ? "texVal1.a"
                                                  : (with_alpha ? "vec4(texVal1.a, texVal1.a, texVal1.a, texVal1.a)"
                                                                : "vec3(texVal1.a, texVal1.a, texVal1.a)"));
            case SHADER_TEXEL1A:
                return first_cycle
                           ? (hint_single_element ? "texVal1.a"
                                                  : (with_alpha ? "vec4(texVal1.a, texVal1.a, texVal1.a, texVal1.a)"
                                                                : "vec3(texVal1.a, texVal1.a, texVal1.a)"))
                           : (hint_single_element ? "texVal0.a"
                                                  : (with_alpha ? "vec4(texVal0.a, texVal0.a, texVal0.a, texVal0.a)"
                                                                : "vec3(texVal0.a, texVal0.a, texVal0.a)"));
            case SHADER_TEXEL1:
                return first_cycle ? (with_alpha ? "texVal1" : "texVal1.rgb")
                                   : (with_alpha ? "texVal0" : "texVal0.rgb");
            case SHADER_COMBINED:
                return with_alpha ? "texel" : "texel.rgb";
            case SHADER_NOISE:
                return with_alpha ? "vec4(" RAND_NOISE ", " RAND_NOISE ", " RAND_NOISE ", " RAND_NOISE ")"
                                  : "vec3(" RAND_NOISE ", " RAND_NOISE ", " RAND_NOISE ")";
        }
    } else {
        switch (item) {
            case SHADER_0:
                return "0.0";
            case SHADER_1:
                return "1.0";
            case SHADER_INPUT_1:
                return "vInput1.a";
            case SHADER_INPUT_2:
                return "vInput2.a";
            case SHADER_INPUT_3:
                return "vInput3.a";
            case SHADER_INPUT_4:
                return "vInput4.a";
            case SHADER_TEXEL0:
                return first_cycle ? "texVal0.a" : "texVal1.a";
            case SHADER_TEXEL0A:
                return first_cycle ? "texVal0.a" : "texVal1.a";
            case SHADER_TEXEL1A:
                return first_cycle ? "texVal1.a" : "texVal0.a";
            case SHADER_TEXEL1:
                return first_cycle ? "texVal1.a" : "texVal0.a";
            case SHADER_COMBINED:
                return "texel.a";
            case SHADER_NOISE:
                return RAND_NOISE;
        }
    }
    return "";
}

bool get_bool(prism::ContextTypes* value) {
    if (std::holds_alternative<int>(*value)) {
        return std::get<int>(*value) == 1;
    }
    return false;
}

prism::ContextTypes* append_formula(prism::ContextTypes* _, prism::ContextTypes* a_arg, prism::ContextTypes* a_single,
                                    prism::ContextTypes* a_mult, prism::ContextTypes* a_mix,
                                    prism::ContextTypes* a_with_alpha, prism::ContextTypes* a_only_alpha,
                                    prism::ContextTypes* a_alpha, prism::ContextTypes* a_first_cycle) {
    auto c = std::get<prism::MTDArray<int>>(*a_arg);
    bool do_single = get_bool(a_single);
    bool do_multiply = get_bool(a_mult);
    bool do_mix = get_bool(a_mix);
    bool with_alpha = get_bool(a_with_alpha);
    bool only_alpha = get_bool(a_only_alpha);
    bool opt_alpha = get_bool(a_alpha);
    bool first_cycle = get_bool(a_first_cycle);
    std::string out = "";
    if (do_single) {
        out += shader_item_to_str(c.at(only_alpha, 3), with_alpha, only_alpha, opt_alpha, first_cycle, false);
    } else if (do_multiply) {
        out += shader_item_to_str(c.at(only_alpha, 0), with_alpha, only_alpha, opt_alpha, first_cycle, false);
        out += " * ";
        out += shader_item_to_str(c.at(only_alpha, 2), with_alpha, only_alpha, opt_alpha, first_cycle, true);
    } else if (do_mix) {
        out += "mix(";
        out += shader_item_to_str(c.at(only_alpha, 1), with_alpha, only_alpha, opt_alpha, first_cycle, false);
        out += ", ";
        out += shader_item_to_str(c.at(only_alpha, 0), with_alpha, only_alpha, opt_alpha, first_cycle, false);
        out += ", ";
        out += shader_item_to_str(c.at(only_alpha, 2), with_alpha, only_alpha, opt_alpha, first_cycle, true);
        out += ")";
    } else {
        out += "(";
        out += shader_item_to_str(c.at(only_alpha, 0), with_alpha, only_alpha, opt_alpha, first_cycle, false);
        out += " - ";
        out += shader_item_to_str(c.at(only_alpha, 1), with_alpha, only_alpha, opt_alpha, first_cycle, false);
        out += ") * ";
        out += shader_item_to_str(c.at(only_alpha, 2), with_alpha, only_alpha, opt_alpha, first_cycle, true);
        out += " + ";
        out += shader_item_to_str(c.at(only_alpha, 3), with_alpha, only_alpha, opt_alpha, first_cycle, false);
    }
    return new prism::ContextTypes{ out };
}

// --- Combiner range analysis (SHADER_OPT_ELIDE_NOOP_WRAP) --------------------------------------
//
// The N64 colour combiner wraps rather than saturates: a cycle's output is taken modulo the signal
// range, which the shader models with WRAP() before the final clamp. That wrap is not always
// reachable. Fast3D already reduces a combiner whose operands make it degenerate (interpreter.cpp,
// gfx_cc_get_features) to one of three shorter forms, and none of them can leave [0,1] when their
// operands are inside it:
//
//   do_single   -> D                 one operand, unchanged
//   do_multiply -> A * C             product of two values in [0,1]
//   do_mix      -> mix(B, A, C)      interpolation between two values in [0,1]
//
// Only the full (A - B) * C + D form can, reaching [-1, 2]. There the wrap is load-bearing: a
// combiner output of 1.8 must come out as -0.22 and then clamp to black, not saturate to white.
//
// Every operand a cycle can name is itself in [0,1]: the constants, the shade/prim/env inputs, a
// texture sample (three-point filtering interpolates between three samples with weights that sum
// to one, so it cannot overshoot either), and the noise term. The exception is SHADER_COMBINED,
// the previous cycle's output, which is only in [0,1] if that cycle was itself in range - and in
// cycle 0 it names an uninitialised variable, so it never is.
static bool CombinerOperandInRange(int item, bool prevCycleInRange) {
    return item != SHADER_COMBINED || prevCycleInRange;
}

// i selects the channel: 0 colour, 1 alpha. The form tests are in the same order as
// append_formula, which picks the first that matches.
static bool CombinerChannelInRange(const CCFeatures& cc, int c, int i, bool prevCycleInRange) {
    if (cc.do_single[c][i]) {
        return CombinerOperandInRange(cc.c[c][i][3], prevCycleInRange);
    }
    if (cc.do_multiply[c][i]) {
        return CombinerOperandInRange(cc.c[c][i][0], prevCycleInRange) &&
               CombinerOperandInRange(cc.c[c][i][2], prevCycleInRange);
    }
    if (cc.do_mix[c][i]) {
        return CombinerOperandInRange(cc.c[c][i][0], prevCycleInRange) &&
               CombinerOperandInRange(cc.c[c][i][1], prevCycleInRange) &&
               CombinerOperandInRange(cc.c[c][i][2], prevCycleInRange);
    }
    return false;
}

static bool CombinerCycleInRange(const CCFeatures& cc, int c, bool prevCycleInRange) {
    if (!CombinerChannelInRange(cc, c, 0, prevCycleInRange)) {
        return false;
    }
    // Alpha only gets a formula of its own when it differs from the colour one; otherwise the
    // colour formula is emitted as a vec4 and covers both.
    if (cc.opt_alpha && !cc.color_alpha_same[c]) {
        return CombinerChannelInRange(cc, c, 1, prevCycleInRange);
    }
    return true;
}

// Whether the cycle whose wrap is under consideration is provably in range. last=false asks about
// cycle 0, whose wrap sits between the two cycles; last=true about whichever cycle produces the
// final colour.
//
// Cycle 0 has no previous cycle, so SHADER_COMBINED there is out of range by definition. When
// cycle 0 is in range its wrap is elided, which leaves texel in [0,1] for cycle 1 to consume - so
// cycle 0's own verdict is exactly what cycle 1 should be handed as its prevCycleInRange.
static bool CombinerCycleInRangeFor(const CCFeatures& cc, int shaderOpts, bool last) {
    if ((shaderOpts & SHADER_OPT_ELIDE_NOOP_WRAP) == 0) {
        return false;
    }
    const bool cycle0 = CombinerCycleInRange(cc, 0, false);
    if (!last || !cc.opt_2cyc) {
        return cycle0;
    }
    return CombinerCycleInRange(cc, 1, cycle0);
}

std::optional<std::string> opengl_include_fs(const std::string& path) {
    auto init = std::make_shared<Ship::ResourceInitData>();
    init->Type = (uint32_t)Ship::ResourceType::Shader;
    init->ByteOrder = Ship::Endianness::Native;
    init->Format = RESOURCE_FORMAT_BINARY;
    auto res = std::static_pointer_cast<Ship::Shader>(
        Ship::Context::GetInstance()->GetResourceManager()->LoadResource(path, true, init));
    if (res == nullptr) {
        return std::nullopt;
    }
    auto inc = static_cast<std::string*>(res->GetRawPointer());
    return *inc;
}

std::string GfxRenderingAPIOGL::BuildFsShader(const CCFeatures& cc_features, int shaderOpts) {
    const bool cycle0InRange = CombinerCycleInRangeFor(cc_features, shaderOpts, false);
    const bool lastCycleInRange = CombinerCycleInRangeFor(cc_features, shaderOpts, true);

    prism::Processor processor;
    prism::ContextItems mContext = {
        { "o_c", M_ARRAY(cc_features.c, int, 2, 2, 4) },
        { "o_alpha", cc_features.opt_alpha },
        { "o_fog", cc_features.opt_fog },
        { "o_texture_edge", cc_features.opt_texture_edge },
        { "o_noise", cc_features.opt_noise },
        { "o_2cyc", cc_features.opt_2cyc },
        { "o_alpha_threshold", cc_features.opt_alpha_threshold },
        { "o_invisible", cc_features.opt_invisible },
        { "o_grayscale", cc_features.opt_grayscale },
        { "o_textures", M_ARRAY(cc_features.usedTextures, bool, 2) },
        { "o_masks", M_ARRAY(cc_features.used_masks, bool, 2) },
        { "o_blend", M_ARRAY(cc_features.used_blend, bool, 2) },
        { "o_clamp", M_ARRAY(cc_features.clamp, bool, 2, 2) },
        { "o_inputs", cc_features.numInputs },
        { "o_do_mix", M_ARRAY(cc_features.do_mix, bool, 2, 2) },
        { "o_do_single", M_ARRAY(cc_features.do_single, bool, 2, 2) },
        { "o_do_multiply", M_ARRAY(cc_features.do_multiply, bool, 2, 2) },
        { "o_color_alpha_same", M_ARRAY(cc_features.color_alpha_same, bool, 2) },
        { "FILTER_THREE_POINT", FILTER_THREE_POINT },
        { "FILTER_LINEAR", FILTER_LINEAR },
        { "FILTER_NONE", FILTER_NONE },
        { "srgb_mode", mSrgbMode },
        { "SHADER_0", SHADER_0 },
        { "SHADER_INPUT_1", SHADER_INPUT_1 },
        { "SHADER_INPUT_2", SHADER_INPUT_2 },
        { "SHADER_INPUT_3", SHADER_INPUT_3 },
        { "SHADER_INPUT_4", SHADER_INPUT_4 },
        { "SHADER_INPUT_5", SHADER_INPUT_5 },
        { "SHADER_INPUT_6", SHADER_INPUT_6 },
        { "SHADER_INPUT_7", SHADER_INPUT_7 },
        { "SHADER_TEXEL0", SHADER_TEXEL0 },
        { "SHADER_TEXEL0A", SHADER_TEXEL0A },
        { "SHADER_TEXEL1", SHADER_TEXEL1 },
        { "SHADER_TEXEL1A", SHADER_TEXEL1A },
        { "SHADER_1", SHADER_1 },
        { "SHADER_COMBINED", SHADER_COMBINED },
        { "SHADER_NOISE", SHADER_NOISE },
        { "o_three_point_filtering", mCurrentFilterMode == FILTER_THREE_POINT },
        { "o_wrap_mid", !cycle0InRange },
        { "o_wrap_out", !lastCycleInRange },
        { "o_float_tex_size", (shaderOpts & SHADER_OPT_FLOAT_TEX_SIZE) != 0 },
        { "append_formula", (InvokeFunc)append_formula },
#ifdef __APPLE__
        { "GLSL_VERSION", "#version 410 core" },
        { "attr", "in" },
        { "opengles", false },
        { "opengles2", false },
        { "frag_out", true },
        { "core_opengl", true },
        { "texture", "texture" },
        { "vOutColor", "vOutColor" },
#elif defined(USE_OPENGLES2)
        // GLSL ES 1.00 has no `out` in the fragment stage; gl_FragColor is the sink.
        { "GLSL_VERSION", "#version 100\nprecision mediump float;" },
        { "attr", "varying" },
        { "opengles", true },
        { "opengles2", true },
        { "frag_out", false },
        { "core_opengl", false },
        { "texture", "texture2D" },
        { "vOutColor", "gl_FragColor" },
#elif defined(USE_OPENGLES)
        { "GLSL_VERSION", "#version 300 es\nprecision mediump float;" },
        { "attr", "in" },
        { "opengles", true },
        { "opengles2", false },
        { "frag_out", true },
        { "core_opengl", false },
        { "texture", "texture" },
        { "vOutColor", "vOutColor" },
#else
        { "GLSL_VERSION", "#version 130" },
        { "attr", "varying" },
        { "opengles", false },
        { "opengles2", false },
        { "frag_out", false },
        { "core_opengl", false },
        { "texture", "texture2D" },
        { "vOutColor", "gl_FragColor" },
#endif
    };
    processor.populate(mContext);
    auto init = std::make_shared<Ship::ResourceInitData>();
    init->Type = (uint32_t)Ship::ResourceType::Shader;
    init->ByteOrder = Ship::Endianness::Native;
    init->Format = RESOURCE_FORMAT_BINARY;
    auto res = std::static_pointer_cast<Ship::Shader>(Ship::Context::GetInstance()->GetResourceManager()->LoadResource(
        "shaders/opengl/default.shader.fs", true, init));

    if (res == nullptr) {
        SPDLOG_ERROR("Failed to load default fragment shader, missing f3d.o2r?");
        abort();
    }

    auto shader = static_cast<std::string*>(res->GetRawPointer());
    processor.load(*shader);
    processor.bind_include_loader(opengl_include_fs);
    auto result = processor.process();
    // SPDLOG_INFO("=========== FRAGMENT SHADER ============");
    // SPDLOG_INFO(result);
    // SPDLOG_INFO("========================================");
    return result;
}

static size_t numFloats = 0;

static prism::ContextTypes* UpdateFloats(prism::ContextTypes* _, prism::ContextTypes* num) {
    numFloats += std::get<int>(*num);
    return nullptr;
}

static std::string BuildVsShader(const CCFeatures& cc_features) {
    numFloats = 4;
    prism::Processor processor;
    prism::ContextItems mContext = { { "o_textures", M_ARRAY(cc_features.usedTextures, bool, 2) },
                                     { "o_clamp", M_ARRAY(cc_features.clamp, bool, 2, 2) },
                                     { "o_fog", cc_features.opt_fog },
                                     { "o_grayscale", cc_features.opt_grayscale },
                                     { "o_alpha", cc_features.opt_alpha },
                                     { "o_inputs", cc_features.numInputs },
                                     { "update_floats", (InvokeFunc)UpdateFloats },
#ifdef __APPLE__
                                     { "GLSL_VERSION", "#version 410 core" },
                                     { "attr", "in" },
                                     { "out", "out" },
                                     { "opengles", false },
                                     { "opengles2", false }
#elif defined(USE_OPENGLES2)
                                     { "GLSL_VERSION", "#version 100" },
                                     { "attr", "attribute" },
                                     { "out", "varying" },
                                     { "opengles", true },
                                     { "opengles2", true }
#elif defined(USE_OPENGLES)
                                     { "GLSL_VERSION", "#version 300 es" },
                                     { "attr", "in" },
                                     { "out", "out" },
                                     { "opengles", true },
                                     { "opengles2", false }
#else
                                     { "GLSL_VERSION", "#version 110" },
                                     { "attr", "attribute" },
                                     { "out", "varying" },
                                     { "opengles", false },
                                     { "opengles2", false }
#endif
    };
    processor.populate(mContext);

    auto init = std::make_shared<Ship::ResourceInitData>();
    init->Type = (uint32_t)Ship::ResourceType::Shader;
    init->ByteOrder = Ship::Endianness::Native;
    init->Format = RESOURCE_FORMAT_BINARY;
    auto res = std::static_pointer_cast<Ship::Shader>(Ship::Context::GetInstance()->GetResourceManager()->LoadResource(
        "shaders/opengl/default.shader.vs", true, init));

    if (res == nullptr) {
        SPDLOG_ERROR("Failed to load default vertex shader, missing f3d.o2r?");
        abort();
    }

    auto shader = static_cast<std::string*>(res->GetRawPointer());
    processor.load(*shader);
    processor.bind_include_loader(opengl_include_fs);
    auto result = processor.process();
    // SPDLOG_INFO("=========== VERTEX SHADER ============");
    // SPDLOG_INFO(result);
    // SPDLOG_INFO("========================================");
    return result;
}

// A failed shader compile used to abort() with the diagnostics commented out on
// the vertex path, which makes a dialect problem impossible to diagnose on the
// device. Log the driver's info log and the generated source before dying.
static void LogShaderCompileFailure(const char* stage, GLuint shader, const std::string& source, uint64_t shader_id0,
                                    uint32_t shader_id1) {
    GLint max_length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &max_length);
    std::string info_log(max_length > 0 ? max_length : 1, '\0');
    if (max_length > 0) {
        glGetShaderInfoLog(shader, max_length, &max_length, info_log.data());
    }

    SPDLOG_ERROR("{} shader compilation failed (shader_id0=0x{:016x} shader_id1=0x{:08x})", stage, shader_id0,
                 shader_id1);
    SPDLOG_ERROR("info log: {}", info_log.c_str());

    // Numbered, so the line the driver names can actually be found.
    std::string line;
    std::istringstream stream(source);
    int n = 1;
    while (std::getline(stream, line)) {
        SPDLOG_ERROR("{:4d} | {}", n++, line);
    }
}

ShaderProgram* GfxRenderingAPIOGL::CreateAndLoadNewShader(uint64_t shader_id0, uint32_t shader_id1) {
    CCFeatures cc_features;
    gfx_cc_get_features(shader_id0, shader_id1, &cc_features);
    // Read once per program and remembered on it. Changing this CVar needs the shader cache
    // dropped, the same as the filtering mode - see Fast3dWindow::SetTextureFilter.
    //
    // Both bits are on by default. Measured across three fixtures on the Pyra: -3.59% in the
    // Market, -2.40% in Kokiri Forest, and a clean null in Hyrule Field, which is not
    // fragment-bound and does not respond to any fragment-side change. Never negative anywhere,
    // and visually identical against an in-session control at each. See
    // docs/pyra/08-optimisation-opportunities.md 8.18.
    const int shaderOpts = Ship::Context::GetInstance()->GetConsoleVariables()->GetInteger(
        CVAR_SHADER_OPTS, SHADER_OPT_ELIDE_NOOP_WRAP | SHADER_OPT_FLOAT_TEX_SIZE);
    const auto fs_buf = BuildFsShader(cc_features, shaderOpts);
    const auto vs_buf = BuildVsShader(cc_features);
    const GLchar* sources[2] = { vs_buf.data(), fs_buf.data() };
    const GLint lengths[2] = { (GLint)vs_buf.size(), (GLint)fs_buf.size() };
    GLint success;

    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &sources[0], &lengths[0]);
    glCompileShader(vertex_shader);
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        LogShaderCompileFailure("vertex", vertex_shader, vs_buf, shader_id0, shader_id1);
        abort();
    }

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &sources[1], &lengths[1]);
    glCompileShader(fragment_shader);
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        LogShaderCompileFailure("fragment", fragment_shader, fs_buf, shader_id0, shader_id1);
        abort();
    }

    GLuint shader_program = glCreateProgram();
    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);
    glLinkProgram(shader_program);

    size_t cnt = 0;

    struct ShaderProgram* prg = &mShaderProgramPool[std::make_pair(shader_id0, shader_id1)];
    prg->attribLocations[cnt] = glGetAttribLocation(shader_program, "aVtxPos");
    prg->attribSizes[cnt] = 4;
    ++cnt;

    for (int i = 0; i < 2; i++) {
        if (cc_features.usedTextures[i]) {
            char name[32];
            snprintf(name, sizeof(name), "aTexCoord%d", i);
            prg->attribLocations[cnt] = glGetAttribLocation(shader_program, name);
            prg->attribSizes[cnt] = 2;
            ++cnt;

            for (int j = 0; j < 2; j++) {
                if (cc_features.clamp[i][j]) {
                    snprintf(name, sizeof(name), "aTexClamp%s%d", j == 0 ? "S" : "T", i);
                    prg->attribLocations[cnt] = glGetAttribLocation(shader_program, name);
                    prg->attribSizes[cnt] = 1;
                    ++cnt;
                }
            }
        }
    }

    if (cc_features.opt_fog) {
        prg->attribLocations[cnt] = glGetAttribLocation(shader_program, "aFog");
        prg->attribSizes[cnt] = 4;
        ++cnt;
    }

    if (cc_features.opt_grayscale) {
        prg->attribLocations[cnt] = glGetAttribLocation(shader_program, "aGrayscaleColor");
        prg->attribSizes[cnt] = 4;
        ++cnt;
    }

    for (int i = 0; i < cc_features.numInputs; i++) {
        char name[16];
        snprintf(name, sizeof(name), "aInput%d", i + 1);
        prg->attribLocations[cnt] = glGetAttribLocation(shader_program, name);
        prg->attribSizes[cnt] = cc_features.opt_alpha ? 4 : 3;
        ++cnt;
    }

    prg->openglProgramId = shader_program;
    prg->numInputs = cc_features.numInputs;
    prg->usedTextures[0] = cc_features.usedTextures[0];
    prg->usedTextures[1] = cc_features.usedTextures[1];
    prg->usedTextures[2] = cc_features.used_masks[0];
    prg->usedTextures[3] = cc_features.used_masks[1];
    prg->usedTextures[4] = cc_features.used_blend[0];
    prg->usedTextures[5] = cc_features.used_blend[1];
    prg->numFloats = numFloats;
    prg->numAttribs = cnt;

    prg->frameCountLocation = glGetUniformLocation(shader_program, "frame_count");
    prg->noiseScaleLocation = glGetUniformLocation(shader_program, "noise_scale");
    prg->texture_width_location = glGetUniformLocation(shader_program, "texture_width");
    prg->texture_height_location = glGetUniformLocation(shader_program, "texture_height");
    prg->texture_filtering_location = glGetUniformLocation(shader_program, "texture_filtering");
    prg->mask_width_location = glGetUniformLocation(shader_program, "mask_width");
    prg->mask_height_location = glGetUniformLocation(shader_program, "mask_height");
    prg->texture_size_location = glGetUniformLocation(shader_program, "texture_size");
    prg->mask_size_location = glGetUniformLocation(shader_program, "mask_size");
    prg->floatTexSize = (shaderOpts & SHADER_OPT_FLOAT_TEX_SIZE) != 0;

    // The shader source lives in soh.o2r, so a device can be running this binary against an older
    // archive. The old template declares texture_width, which the new one never does when float
    // sizes are asked for; finding it means the archive predates the change. Uploading vec2s to a
    // program with no vec2 uniforms would silently leave every texture size unset, so fall back to
    // the layout the shader actually has rather than render a frame with garbage sizes in it.
    if (prg->floatTexSize && prg->texture_width_location != -1) {
        SPDLOG_WARN("soh.o2r is older than this binary: default.shader.fs has no vec2 size "
                    "uniforms. Ignoring SHADER_OPT_FLOAT_TEX_SIZE.");
        prg->floatTexSize = false;
    }

    // What share of the variants a scene actually uses qualify for the elision is not knowable
    // from the code - it depends on which combiners the game sets - so say so per variant. This
    // is how 8.18 established that Hyrule Field elides 25 of its 26 variants and still gains
    // nothing, which is what ruled out "the optimisation misses these shaders" and left "this
    // frame is not fragment-bound". Debug level because the elision is now on by default and
    // this is one line per variant per scene load.
    if (shaderOpts & SHADER_OPT_ELIDE_NOOP_WRAP) {
        SPDLOG_DEBUG("shader {:016x}/{:08x}: 2cyc={} wrap_mid={} wrap_out={}", shader_id0, shader_id1,
                     cc_features.opt_2cyc, !CombinerCycleInRangeFor(cc_features, shaderOpts, false),
                     !CombinerCycleInRangeFor(cc_features, shaderOpts, true));
    }

    LoadShader(prg);

    if (cc_features.usedTextures[0]) {
        GLint sampler_location = glGetUniformLocation(shader_program, "uTex0");
        glUniform1i(sampler_location, 0);
    }
    if (cc_features.usedTextures[1]) {
        GLint sampler_location = glGetUniformLocation(shader_program, "uTex1");
        glUniform1i(sampler_location, 1);
    }
    if (cc_features.used_masks[0]) {
        GLint sampler_location = glGetUniformLocation(shader_program, "uTexMask0");
        glUniform1i(sampler_location, 2);
    }
    if (cc_features.used_masks[1]) {
        GLint sampler_location = glGetUniformLocation(shader_program, "uTexMask1");
        glUniform1i(sampler_location, 3);
    }
    if (cc_features.used_blend[0]) {
        GLint sampler_location = glGetUniformLocation(shader_program, "uTexBlend0");
        glUniform1i(sampler_location, 4);
    }
    if (cc_features.used_blend[1]) {
        GLint sampler_location = glGetUniformLocation(shader_program, "uTexBlend1");
        glUniform1i(sampler_location, 5);
    }

    return prg;
}

void GfxRenderingAPIOGL::ClearShaderCache() {
    // Texture filtering is compiled into the shader source (o_three_point_filtering) but is not
    // part of the variant key, so changing it at runtime requires throwing the cache away. The
    // caller is responsible for also dropping every ShaderProgram* held elsewhere - see
    // Interpreter::ResetShaderCache, which owns the colour-combiner pool that caches them.
    for (auto& entry : mShaderProgramPool) {
        for (unsigned int i = 0; i < entry.second.numAttribs; i++) {
            if (entry.second.attribLocations[i] >= 0) {
                glDisableVertexAttribArray(entry.second.attribLocations[i]);
            }
        }
        glDeleteProgram(entry.second.openglProgramId);
    }
    mShaderProgramPool.clear();
    mCurrentShaderProgram = nullptr;
    mLastLoadedShader = nullptr;
    glUseProgram(0);
    ForgetVertexAttribs(false);
}

struct ShaderProgram* GfxRenderingAPIOGL::LookupShader(uint64_t shader_id0, uint32_t shader_id1) {
    auto it = mShaderProgramPool.find(std::make_pair(shader_id0, shader_id1));
    return it == mShaderProgramPool.end() ? nullptr : &it->second;
}

void GfxRenderingAPIOGL::ShaderGetInfo(struct ShaderProgram* prg, uint8_t* numInputs, bool usedTextures[2]) {
    *numInputs = prg->numInputs;
    usedTextures[0] = prg->usedTextures[0];
    usedTextures[1] = prg->usedTextures[1];
}

GLuint GfxRenderingAPIOGL::NewTexture() {
    GLuint ret;
    glGenTextures(1, &ret);
    textures.resize(std::max(textures.size(), (size_t)ret + 1));
    return ret;
}

void GfxRenderingAPIOGL::DeleteTexture(uint32_t texID) {
    glDeleteTextures(1, &texID);
}

void GfxRenderingAPIOGL::SelectTexture(int tile, GLuint texture_id) {
    if (mLastActiveTexture != tile) {
        mLastActiveTexture = tile;
        glActiveTexture(GL_TEXTURE0 + tile);
    }
    if (mLastBoundTextures[tile] != texture_id) {
        mLastBoundTextures[tile] = texture_id;
        glBindTexture(GL_TEXTURE_2D, texture_id);
    }
    mCurrentTextureIds[tile] = texture_id;
    mCurrentTile = tile;
}

void GfxRenderingAPIOGL::UploadTexture(const uint8_t* rgba32_buf, uint32_t width, uint32_t height) {
    BENCH_COUNT(BENCH_TEX_UPLOADS, 1);
    BENCH_COUNT(BENCH_TEX_UPLOAD_KB, (width * height * 4) / 1024);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba32_buf);
    textures[mCurrentTextureIds[mCurrentTile]].width = width;
    textures[mCurrentTextureIds[mCurrentTile]].height = height;
}

#if defined(USE_OPENGLES) && !defined(USE_OPENGLES2)
#define GL_MIRROR_CLAMP_TO_EDGE 0x8743
#endif

static uint32_t gfx_cm_to_opengl(uint32_t val) {
    switch (val) {
        case G_TX_NOMIRROR | G_TX_CLAMP:
            return GL_CLAMP_TO_EDGE;
        case G_TX_MIRROR | G_TX_WRAP:
            return GL_MIRRORED_REPEAT;
        case G_TX_MIRROR | G_TX_CLAMP:
#ifdef USE_OPENGLES2
            // GL_MIRROR_CLAMP_TO_EDGE has no GLES 2.0 equivalent and no common
            // extension provides it (PowerVR SGX544 does not). Passing the
            // desktop enum anyway yields GL_INVALID_ENUM and silently leaves the
            // default GL_REPEAT, which is the worst of the options. Mirrored
            // repeat matches N64 mirror-then-clamp exactly for coordinates
            // inside the first mirrored period, which is the common case, and
            // degrades gracefully outside it.
            return GL_MIRRORED_REPEAT;
#else
            return GL_MIRROR_CLAMP_TO_EDGE;
#endif
        case G_TX_NOMIRROR | G_TX_WRAP:
            return GL_REPEAT;
    }
    return 0;
}

void GfxRenderingAPIOGL::SetSamplerParameters(int tile, bool linear_filter, uint32_t cms, uint32_t cmt) {
    if (mLastActiveTexture != tile) {
        mLastActiveTexture = tile;
        glActiveTexture(GL_TEXTURE0 + tile);
    }
    const GLint filter = linear_filter && mCurrentFilterMode == FILTER_LINEAR ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    textures[mCurrentTextureIds[tile]].filtering = !linear_filter ? FILTER_LINEAR : FILTER_THREE_POINT;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gfx_cm_to_opengl(cms));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gfx_cm_to_opengl(cmt));
}

void GfxRenderingAPIOGL::SetDepthTestAndMask(bool depth_test, bool z_upd) {
    mCurrentDepthTest = depth_test;
    mCurrentDepthMask = z_upd;
}

void GfxRenderingAPIOGL::SetZmodeDecal(bool zmode_decal) {
    mCurrentZmodeDecal = zmode_decal;
}

void GfxRenderingAPIOGL::SetViewport(int x, int y, int width, int height) {
    glViewport(x, y, width, height);
}

void GfxRenderingAPIOGL::SetScissor(int x, int y, int width, int height) {
    glScissor(x, y, width, height);
}

void GfxRenderingAPIOGL::SetUseAlpha(bool use_alpha) {
    int8_t val = use_alpha ? 1 : 0;
    if (mLastBlendEnabled != val) {
        mLastBlendEnabled = val;
        if (use_alpha) {
            glEnable(GL_BLEND);
        } else {
            glDisable(GL_BLEND);
        }
    }
}

void GfxRenderingAPIOGL::DrawTriangles(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris) {
    BENCH_COUNT(BENCH_DRAWS, 1);
    BENCH_COUNT(BENCH_TRIS, buf_vbo_num_tris);
    if (mCurrentDepthTest != mLastDepthTest || mCurrentDepthMask != mLastDepthMask) {
        mLastDepthTest = mCurrentDepthTest;
        mLastDepthMask = mCurrentDepthMask;

        if (mCurrentDepthTest || mLastDepthMask) {
            glEnable(GL_DEPTH_TEST);
            glDepthMask(mLastDepthMask ? GL_TRUE : GL_FALSE);
            glDepthFunc(mCurrentDepthTest ? (mCurrentZmodeDecal ? GL_LEQUAL : GL_LESS) : GL_ALWAYS);
        } else {
            glDisable(GL_DEPTH_TEST);
        }
    }

    if (mCurrentZmodeDecal != mLastZmodeDecal) {
        mLastZmodeDecal = mCurrentZmodeDecal;
        if (mCurrentZmodeDecal) {
            // SSDB = SlopeScaledDepthBias 120 leads to -2 at 240p which is the same as N64 mode which has very little
            // fighting
            const int n64modeFactor = 120;
            const int noVanishFactor = 100;
            GLfloat SSDB = -2;
            switch (Ship::Context::GetInstance()->GetConsoleVariables()->GetInteger(CVAR_Z_FIGHTING_MODE, 0)) {
                // scaled z-fighting (N64 mode like)
                case 1:
                    if (mFrameBuffers.size() >
                        mCurrentFrameBuffer) { // safety check for vector size can probably be removed
                        SSDB = -1.0f * (GLfloat)mFrameBuffers[mCurrentFrameBuffer].height / n64modeFactor;
                    }
                    break;
                // no vanishing paths
                case 2:
                    if (mFrameBuffers.size() >
                        mCurrentFrameBuffer) { // safety check for vector size can probably be removed
                        SSDB = -1.0f * (GLfloat)mFrameBuffers[mCurrentFrameBuffer].height / noVanishFactor;
                    }
                    break;
                // disabled
                case 0:
                default:
                    SSDB = -2;
            }
            glPolygonOffset(SSDB, -2);
            glEnable(GL_POLYGON_OFFSET_FILL);
        } else {
            glPolygonOffset(0, 0);
            glDisable(GL_POLYGON_OFFSET_FILL);
        }
    }

    SetPerDrawUniforms();

    // printf("flushing %d tris\n", buf_vbo_num_tris);
    // On the PowerVR SGX544, orphaning the VBO with one glBufferData per draw looked harmless in a
    // microbenchmark (us per draw: this path 68, client-side arrays 58, a glBufferSubData ring
    // buffer 221), but in the real game each call makes the driver allocate device memory, and
    // client-side arrays (LeanGLState 3, the GLES2 default) are -13.4% of the Market frame and
    // -13.6% of render-thread CPU in real play. Never use a glBufferSubData ring on this driver.
    // See docs/pyra/05-phase0-device-measurements.md and docs/pyra/08-optimisation-opportunities.md
    // sections 8.6, 8.24 and 8.25.
    if (mLeanGLState < 3) {
        BENCH_COUNT(BENCH_VBO_UPLOADS, 1);
        BENCH_COUNT(BENCH_VBO_BYTES, sizeof(float) * buf_vbo_len);
    }
    if (mLeanGLState >= 3) {
        // Client-side arrays: the driver copies the vertices at the draw, from the interpreter's
        // array, which does not move - so after the first draw the pointers never need resetting.
        if (mClientVertexBase != buf_vbo) {
            mClientVertexBase = buf_vbo;
        }
        SetVertexAttribsLean(mCurrentShaderProgram);
    } else {
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * buf_vbo_len, buf_vbo, GL_STREAM_DRAW);
    }
    glDrawArrays(GL_TRIANGLES, 0, 3 * buf_vbo_num_tris);
}

void GfxRenderingAPIOGL::Init() {
#if !defined(__linux__) && !defined(__OpenBSD__)
    glewInit();
#endif

    glGenBuffers(1, &mOpenglVbo);
    glBindBuffer(GL_ARRAY_BUFFER, mOpenglVbo);
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &mMaxVertexAttribs);

#if defined(__APPLE__) || (defined(USE_OPENGLES) && !defined(USE_OPENGLES2))
    // GLES 2.0 has no core VAOs. None is needed: the attribute pointers are
    // respecified on every shader load anyway.
    glGenVertexArrays(1, &mOpenglVao);
    glBindVertexArray(mOpenglVao);
#endif

#ifndef USE_OPENGLES // not supported on gles
    glEnable(GL_DEPTH_CLAMP);
#endif
    glDepthFunc(GL_LEQUAL);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    mFrameBuffers.resize(1); // for the default screen buffer

    glGenRenderbuffers(1, &mPixelDepthRb);
    glBindRenderbuffer(GL_RENDERBUFFER, mPixelDepthRb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 1, 1);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glGenFramebuffers(1, &mPixelDepthFb);
    glBindFramebuffer(GL_FRAMEBUFFER, mPixelDepthFb);
    AttachDepthStencil(mPixelDepthRb);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    mPixelDepthRbSize = 1;

#ifdef USE_OPENGLES2
    // Multisampled renderbuffers are not core in GLES 2.0.
    mMaxMsaaLevel = 1;

    if (HasGLExtension("GL_EXT_discard_framebuffer")) {
        sDiscardFramebufferEXT =
            reinterpret_cast<SohPfnDiscardFramebufferEXT>(SDL_GL_GetProcAddress("glDiscardFramebufferEXT"));
    }
    SPDLOG_INFO("GL_EXT_discard_framebuffer: {}", sDiscardFramebufferEXT != nullptr ? "in use" : "unavailable");
#else
    glGetIntegerv(GL_MAX_SAMPLES, &mMaxMsaaLevel);
#endif
}

void GfxRenderingAPIOGL::OnResize() {
}

void GfxRenderingAPIOGL::StartFrame() {
    mFrameCount++;
#ifdef USE_OPENGLES2
    // Client-side vertex arrays (mode 3) are legal in GLES2 but not in a desktop core profile.
    constexpr int kLeanGLStateDefault = 3;
#else
    constexpr int kLeanGLStateDefault = 0;
#endif
    const int lean = Ship::Context::GetInstance()->GetConsoleVariables()->GetInteger("gSettings.LeanGLState",
                                                                                     kLeanGLStateDefault);
    // Re-establish the attribute shadow every frame rather than trust that nothing else in the
    // context (ImGui draws between frames) left the arrays exactly as it found them; and on any
    // change of mode, since mode 0 does not maintain the shadow.
    if (lean >= 1 || lean != mLeanGLState) {
        mLeanGLState = lean;
        ForgetVertexAttribs(true);
    }
    mLeanGLState = lean;
}

void GfxRenderingAPIOGL::DiscardDepthStencil() {
#ifdef USE_OPENGLES2
    if (sDiscardFramebufferEXT == nullptr) {
        return;
    }

    // The attachment tokens differ between the default framebuffer and a framebuffer object, and
    // passing the wrong set raises GL_INVALID_ENUM rather than being ignored. mCurrentFrameBuffer
    // is an index into mFrameBuffers, where entry 0 is the default framebuffer (fbo name 0).
    if (mCurrentFrameBuffer == 0) {
        const GLenum attachments[] = { GL_DEPTH_EXT, GL_STENCIL_EXT };
        sDiscardFramebufferEXT(GL_FRAMEBUFFER, 2, attachments);
    } else {
        const GLenum attachments[] = { GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT };
        sDiscardFramebufferEXT(GL_FRAMEBUFFER, 2, attachments);
    }
#endif
}

void GfxRenderingAPIOGL::EndFrame() {
    // The frame is finished and the buffer swap is next, so the default framebuffer's depth and
    // stencil are dead. Colour is left alone - it is what gets presented.
    DiscardDepthStencil();
#ifdef SOH_BENCHMARK
    if (gBenchFinish) {
        const auto finishStart = std::chrono::steady_clock::now();
        glFinish();
        gBenchFinishMicros += (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(
                                  std::chrono::steady_clock::now() - finishStart)
                                  .count();
    }
    if (gBenchDumpPath[0] != '\0') {
        DumpFramebuffer(gBenchDumpFb, gBenchDumpPath);
        gBenchDumpPath[0] = '\0';
    }
#endif
    glFlush();
}

#ifdef SOH_BENCHMARK
void GfxRenderingAPIOGL::DumpFramebuffer(int fb_id, const char* path) {
    const FramebufferOGL& fb = mFrameBuffers[fb_id];
    std::vector<uint8_t> rgba((size_t)fb.width * fb.height * 4);
    glBindFramebuffer(GL_FRAMEBUFFER, fb.fbo);
    glReadPixels(0, 0, fb.width, fb.height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glBindFramebuffer(GL_FRAMEBUFFER, mFrameBuffers[mCurrentFrameBuffer].fbo);
    FILE* f = fopen(path, "wb");
    if (f == nullptr) {
        SPDLOG_ERROR("Benchmark: cannot write frame dump {}", path);
        return;
    }
    fprintf(f, "P6\n%u %u\n255\n", fb.width, fb.height);
    // GL's rows run bottom-up; a PPM's run top-down.
    std::vector<uint8_t> row((size_t)fb.width * 3);
    for (int y = (int)fb.height - 1; y >= 0; y--) {
        const uint8_t* src = &rgba[(size_t)y * fb.width * 4];
        for (uint32_t x = 0; x < fb.width; x++) {
            row[x * 3 + 0] = src[x * 4 + 0];
            row[x * 3 + 1] = src[x * 4 + 1];
            row[x * 3 + 2] = src[x * 4 + 2];
        }
        fwrite(row.data(), 1, row.size(), f);
    }
    fclose(f);
}
#endif

void GfxRenderingAPIOGL::FinishRender() {
}

int GfxRenderingAPIOGL::CreateFramebuffer() {
    GLuint clrbuf;
    glGenTextures(1, &clrbuf);
    glBindTexture(GL_TEXTURE_2D, clrbuf);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    GLuint clrbufMsaa;
    glGenRenderbuffers(1, &clrbufMsaa);

    GLuint rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 1, 1);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    GLuint fbo;
    glGenFramebuffers(1, &fbo);

    size_t i = mFrameBuffers.size();
    mFrameBuffers.resize(i + 1);

    mFrameBuffers[i].fbo = fbo;
    mFrameBuffers[i].clrbuf = clrbuf;
    mFrameBuffers[i].clrbufMsaa = clrbufMsaa;
    mFrameBuffers[i].rbo = rbo;

    return i;
}

void GfxRenderingAPIOGL::UpdateFramebufferParameters(int fb_id, uint32_t width, uint32_t height, uint32_t msaa_level,
                                                     bool opengl_invertY, bool render_target, bool has_depth_buffer,
                                                     bool can_extract_depth) {
    FramebufferOGL& fb = mFrameBuffers[fb_id];

    width = std::max(width, 1U);
    height = std::max(height, 1U);
    msaa_level = std::min(msaa_level, (uint32_t)mMaxMsaaLevel);

    glBindFramebuffer(GL_FRAMEBUFFER, fb.fbo);

    if (fb_id != 0) {
        if (fb.width != width || fb.height != height || fb.msaa_level != msaa_level) {
            BENCH_COUNT(BENCH_FB_REALLOCS, 1);
            if (msaa_level <= 1) {
                glBindTexture(GL_TEXTURE_2D, fb.clrbuf);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
                glBindTexture(GL_TEXTURE_2D, 0);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb.clrbuf, 0);
            } else {
                glBindRenderbuffer(GL_RENDERBUFFER, fb.clrbufMsaa);
                RenderbufferStorageMsaa(msaa_level, GL_RGB8, width, height);
                glBindRenderbuffer(GL_RENDERBUFFER, 0);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, fb.clrbufMsaa);
            }
        }

        if (has_depth_buffer &&
            (fb.width != width || fb.height != height || fb.msaa_level != msaa_level || !fb.has_depth_buffer)) {
            glBindRenderbuffer(GL_RENDERBUFFER, fb.rbo);
            if (msaa_level <= 1) {
                glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
            } else {
                RenderbufferStorageMsaa(msaa_level, GL_DEPTH24_STENCIL8, width, height);
            }
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }

        if (!fb.has_depth_buffer && has_depth_buffer) {
            AttachDepthStencil(fb.rbo);
        } else if (fb.has_depth_buffer && !has_depth_buffer) {
            AttachDepthStencil(0);
        }
    }

    fb.width = width;
    fb.height = height;
    fb.has_depth_buffer = has_depth_buffer;
    fb.msaa_level = msaa_level;
    fb.invertY = opengl_invertY;
}

void GfxRenderingAPIOGL::StartDrawToFramebuffer(int fb_id, float noise_scale) {
    BENCH_COUNT(BENCH_FB_BINDS, 1);
    FramebufferOGL& fb = mFrameBuffers[fb_id];

    if (noise_scale != 0.0f) {
        mCurrentNoiseScale = 1.0f / noise_scale;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, fb.fbo);
    mCurrentFrameBuffer = fb_id;
}

void GfxRenderingAPIOGL::ClearFramebuffer(bool color, bool depth) {
    BENCH_COUNT(BENCH_CLEARS, 1);
    if (mLastScissorEnabled != 0) {
        mLastScissorEnabled = 0;
        glDisable(GL_SCISSOR_TEST);
    }
    glDepthMask(GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear((color ? GL_COLOR_BUFFER_BIT : 0) | (depth ? GL_DEPTH_BUFFER_BIT : 0));
    glDepthMask(mCurrentDepthMask ? GL_TRUE : GL_FALSE);
    if (mLastScissorEnabled != 1) {
        mLastScissorEnabled = 1;
        glEnable(GL_SCISSOR_TEST);
    }
}

#ifdef USE_OPENGLES2
// GLES 2.0 has no glBlitFramebuffer (and SGX544 offers neither NV_ nor
// ANGLE_framebuffer_blit), so a framebuffer copy is done by drawing the source
// colour attachment over the destination as a textured quad.
void GfxRenderingAPIOGL::EnsureBlitProgram() {
    if (mBlitProgram != 0) {
        return;
    }

    static const char* vsSrc = "#version 100\n"
                               "attribute vec2 aPos;\n"
                               "attribute vec2 aUv;\n"
                               "varying vec2 vUv;\n"
                               "void main() { vUv = aUv; gl_Position = vec4(aPos, 0.0, 1.0); }\n";
    static const char* fsSrc = "#version 100\n"
                               "precision mediump float;\n"
                               "uniform sampler2D uTex;\n"
                               "varying vec2 vUv;\n"
                               "void main() { gl_FragColor = texture2D(uTex, vUv); }\n";

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsSrc, nullptr);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsSrc, nullptr);
    glCompileShader(fs);

    mBlitProgram = glCreateProgram();
    glAttachShader(mBlitProgram, vs);
    glAttachShader(mBlitProgram, fs);
    glBindAttribLocation(mBlitProgram, 0, "aPos");
    glBindAttribLocation(mBlitProgram, 1, "aUv");
    glLinkProgram(mBlitProgram);

    GLint linked = 0;
    glGetProgramiv(mBlitProgram, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[512] = {};
        glGetProgramInfoLog(mBlitProgram, sizeof(log) - 1, nullptr, log);
        SPDLOG_ERROR("GLES2 framebuffer blit program failed to link: {}", log);
    }

    mBlitTexLocation = glGetUniformLocation(mBlitProgram, "uTex");
    glDeleteShader(vs);
    glDeleteShader(fs);
}

void GfxRenderingAPIOGL::BlitFramebufferAsQuad(GLuint dstFbo, GLuint srcTex, float u0, float v0, float u1, float v1,
                                               int dstX0, int dstY0, int dstX1, int dstY1) {
    EnsureBlitProgram();

    const GLfloat quad[] = {
        -1.0f, -1.0f, u0, v0, 1.0f, -1.0f, u1, v0, -1.0f, 1.0f, u0, v1, 1.0f, 1.0f, u1, v1,
    };

    glBindFramebuffer(GL_FRAMEBUFFER, dstFbo);
    glViewport(dstX0, dstY0, dstX1 - dstX0, dstY1 - dstY0);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    // Depth writes are still live with the depth test off, and would otherwise
    // stamp the quad into the destination depth buffer.
    glDepthMask(GL_FALSE);

    glUseProgram(mBlitProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, srcTex);
    glUniform1i(mBlitTexLocation, 0);

    glBindBuffer(GL_ARRAY_BUFFER, mOpenglVbo);
    BENCH_COUNT(BENCH_VBO_UPLOADS, 1);
    BENCH_COUNT(BENCH_VBO_BYTES, sizeof(quad));
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);

    // Every piece of GL state this renderer caches has just been invalidated.
    mAttribEnabled &= ~3u;
    mAttribPointers[0].valid = false;
    mAttribPointers[1].valid = false;
    mBoundArrayBuffer = mOpenglVbo;
    mBoundArrayBufferKnown = true;
    mLastLoadedShader = nullptr;
    mLastActiveTexture = -1;
    mLastBlendEnabled = -1;
    mLastScissorEnabled = -1;
    mLastDepthTest = -1;
    mLastDepthMask = -1;
    mLastZmodeDecal = -1;
    for (auto& tex : mLastBoundTextures) {
        tex = 0;
    }
}
#endif

void GfxRenderingAPIOGL::ResolveMSAAColorBuffer(int fb_id_target, int fb_id_source) {
    FramebufferOGL& fb_dst = mFrameBuffers[fb_id_target];
    FramebufferOGL& fb_src = mFrameBuffers[fb_id_source];
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb_dst.fbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fb_src.fbo);

    // Disabled for blit
    if (mLastScissorEnabled != 0) {
        mLastScissorEnabled = 0;
        glDisable(GL_SCISSOR_TEST);
    }

#ifdef USE_OPENGLES2
    // Multisampling is unavailable here (mMaxMsaaLevel is pinned to 1), so this
    // is a straight copy.
    BlitFramebufferAsQuad(fb_dst.fbo, fb_src.clrbuf, 0.0f, 0.0f, 1.0f, 1.0f, 0, 0, fb_dst.width, fb_dst.height);
#else
    glBlitFramebuffer(0, 0, fb_src.width, fb_src.height, 0, 0, fb_dst.width, fb_dst.height, GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);
#endif
    glBindFramebuffer(GL_FRAMEBUFFER, mCurrentFrameBuffer);

    if (mLastScissorEnabled != 1) {
        mLastScissorEnabled = 1;
        glEnable(GL_SCISSOR_TEST);
    }
}

void* GfxRenderingAPIOGL::GetFramebufferTextureId(int fb_id) {
    return (void*)(uintptr_t)mFrameBuffers[fb_id].clrbuf;
}

void GfxRenderingAPIOGL::SelectTextureFb(int fb_id) {
    // glDisable(GL_DEPTH_TEST);
    int tile = 0;
    SelectTexture(tile, mFrameBuffers[fb_id].clrbuf);
}

void GfxRenderingAPIOGL::CopyFramebuffer(int fb_dst_id, int fb_src_id, int srcX0, int srcY0, int srcX1, int srcY1,
                                         int dstX0, int dstY0, int dstX1, int dstY1) {
    BENCH_COUNT(BENCH_FB_COPIES, 1);
    if (fb_dst_id >= (int)mFrameBuffers.size() || fb_src_id >= (int)mFrameBuffers.size()) {
        return;
    }

    FramebufferOGL src = mFrameBuffers[fb_src_id];
    const FramebufferOGL& dst = mFrameBuffers[fb_dst_id];

    // Adjust y values for non-inverted source frame buffers because opengl uses bottom left for origin
    if (!src.invertY) {
        int temp = srcY1 - srcY0;
        srcY1 = src.height - srcY0;
        srcY0 = srcY1 - temp;
    }

    // Flip the y values
    if (src.invertY != dst.invertY) {
        std::swap(srcY0, srcY1);
    }

    // Disabled for blit
    if (mLastScissorEnabled != 0) {
        mLastScissorEnabled = 0;
        glDisable(GL_SCISSOR_TEST);
    }

    // For msaa enabled buffers we can't perform a scaled blit to a simple sample buffer
    // First do an unscaled blit to a msaa resolved buffer
    if (src.height != dst.height && src.width != dst.width && src.msaa_level > 1) {
        // Start with the main buffer (0) as the msaa resolved buffer
        int fb_resolve_id = 0;
        FramebufferOGL fb_resolve = mFrameBuffers[fb_resolve_id];

        // If the size doesn't match our source, then we need to use our separate color msaa resolved buffer (2)
        if (fb_resolve.height != src.height || fb_resolve.width != src.width) {
            fb_resolve_id = 2;
            fb_resolve = mFrameBuffers[fb_resolve_id];
        }

#ifdef USE_OPENGLES2
        BlitFramebufferAsQuad(fb_resolve.fbo, src.clrbuf, 0.0f, 0.0f, 1.0f, 1.0f, 0, 0, src.width, src.height);
#else
        glBindFramebuffer(GL_READ_FRAMEBUFFER, src.fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb_resolve.fbo);

        glBlitFramebuffer(0, 0, src.width, src.height, 0, 0, src.width, src.height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
#endif

        // Switch source buffer to the resolved sample
        fb_src_id = fb_resolve_id;
        src = fb_resolve;
    }

#ifdef USE_OPENGLES2
    // glReadBuffer does not exist either; sampling the colour attachment as a
    // texture is the equivalent, and the source rectangle becomes texture
    // coordinates. A y flip is expressed by v0 > v1.
    const float invW = src.width > 0 ? 1.0f / (float)src.width : 0.0f;
    const float invH = src.height > 0 ? 1.0f / (float)src.height : 0.0f;
    BlitFramebufferAsQuad(dst.fbo, src.clrbuf, srcX0 * invW, srcY0 * invH, srcX1 * invW, srcY1 * invH, dstX0, dstY0,
                          dstX1, dstY1);
    glBindFramebuffer(GL_FRAMEBUFFER, mFrameBuffers[mCurrentFrameBuffer].fbo);
#else
    glBindFramebuffer(GL_READ_FRAMEBUFFER, src.fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst.fbo);

    // The 0 buffer is a double buffer so we need to choose the back to avoid imgui elements
    if (fb_src_id == 0) {
        glReadBuffer(GL_BACK);
    } else {
        glReadBuffer(GL_COLOR_ATTACHMENT0);
    }

    glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, mFrameBuffers[mCurrentFrameBuffer].fbo);

    glReadBuffer(GL_BACK);
#endif

    if (mLastScissorEnabled != 1) {
        mLastScissorEnabled = 1;
        glEnable(GL_SCISSOR_TEST);
    }
}

void GfxRenderingAPIOGL::ReadFramebufferToCPU(int fb_id, uint32_t width, uint32_t height, uint16_t* rgba16_buf) {
    BENCH_COUNT(BENCH_FB_READBACKS, 1);
    if (fb_id >= (int)mFrameBuffers.size()) {
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, mFrameBuffers[fb_id].fbo);
#ifdef USE_OPENGLES2
    // GLES 2.0 guarantees exactly one readback pair - GL_RGBA/GL_UNSIGNED_BYTE -
    // plus one implementation-defined pair. GL_UNSIGNED_SHORT_5_5_5_1 is not
    // guaranteed and is rejected on PowerVR SGX544, which reports
    // GL_BGRA_EXT/GL_UNSIGNED_BYTE as its preferred pair. Read the guaranteed
    // format and repack to RGBA5551 on the CPU.
    {
        std::vector<uint8_t> rgba8((size_t)width * height * 4);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba8.data());
        for (size_t i = 0; i < (size_t)width * height; i++) {
            const uint8_t r = rgba8[i * 4 + 0];
            const uint8_t g = rgba8[i * 4 + 1];
            const uint8_t b = rgba8[i * 4 + 2];
            const uint8_t a = rgba8[i * 4 + 3];
            rgba16_buf[i] = (uint16_t)(((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | (a >> 7));
        }
    }
#else
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, (void*)rgba16_buf);
#endif
    glBindFramebuffer(GL_FRAMEBUFFER, mFrameBuffers[mCurrentFrameBuffer].fbo);
}

std::unordered_map<std::pair<float, float>, uint16_t, hash_pair_ff>
GfxRenderingAPIOGL::GetPixelDepth(int fb_id, const std::set<std::pair<float, float>>& coordinates) {
    std::unordered_map<std::pair<float, float>, uint16_t, hash_pair_ff> res;

    FramebufferOGL& fb = mFrameBuffers[fb_id];

    // When looking up one value and the framebuffer is single-sampled, we can read pixels directly
    // Otherwise we need to blit first to a new buffer then read it
    if (coordinates.size() == 1 && fb.msaa_level <= 1) {
        uint32_t depth_stencil_value = 0;
        glBindFramebuffer(GL_FRAMEBUFFER, fb.fbo);
        int x = coordinates.begin()->first;
        int y = coordinates.begin()->second;
#ifndef USE_OPENGLES // not supported on gles. Runs fine without it, but this may cause issues
        glReadPixels(x, fb.invertY ? fb.height - y : y, 1, 1, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8,
                     &depth_stencil_value);
#endif
        res.emplace(*coordinates.begin(), (depth_stencil_value >> 18) << 2);
    } else {
        if (mPixelDepthRbSize < coordinates.size()) {
            // Resizing a renderbuffer seems broken with Intel's driver, so recreate one instead.
            glBindFramebuffer(GL_FRAMEBUFFER, mPixelDepthFb);
            glDeleteRenderbuffers(1, &mPixelDepthRb);
            glGenRenderbuffers(1, &mPixelDepthRb);
            glBindRenderbuffer(GL_RENDERBUFFER, mPixelDepthRb);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, coordinates.size(), 1);
            AttachDepthStencil(mPixelDepthRb);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);

            mPixelDepthRbSize = coordinates.size();
        }

        // This blit exists only to stage depth for the glReadPixels below, which
        // no GLES version supports for GL_DEPTH_STENCIL. GLES 2.0 also has no
        // glBlitFramebuffer, so the staging is skipped and the zeroed values are
        // returned, matching what the GLES 3 path already produces here.
#ifndef USE_OPENGLES2
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fb.fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mPixelDepthFb);

        glDisable(GL_SCISSOR_TEST); // needed for the blit operation

        {
            size_t i = 0;
            for (const auto& coord : coordinates) {
                int x = coord.first;
                int y = coord.second;
                if (fb.invertY) {
                    y = fb.height - y;
                }
                glBlitFramebuffer(x, y, x + 1, y + 1, i, 0, i + 1, 1, GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT,
                                  GL_NEAREST);
                ++i;
            }
        }

        glBindFramebuffer(GL_READ_FRAMEBUFFER, mPixelDepthFb);
#endif
        std::vector<uint32_t> depth_stencil_values(coordinates.size());
#ifndef USE_OPENGLES // not supported on gles. Runs fine without it, but this may cause issues
        glReadPixels(0, 0, coordinates.size(), 1, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, depth_stencil_values.data());
#endif
        {
            size_t i = 0;
            for (const auto& coord : coordinates) {
                res.emplace(coord, (depth_stencil_values[i++] >> 18) << 2);
            }
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, mCurrentFrameBuffer);

    return res;
}

void GfxRenderingAPIOGL::SetTextureFilter(FilteringMode mode) {
    gfx_texture_cache_clear();
    mCurrentFilterMode = mode;
}

FilteringMode GfxRenderingAPIOGL::GetTextureFilter() {
    return mCurrentFilterMode;
}

void GfxRenderingAPIOGL::SetSrgbMode() {
    mSrgbMode = true;
}

ImTextureID GfxRenderingAPIOGL::GetTextureById(int id) {
    return reinterpret_cast<ImTextureID>(id);
}
} // namespace Fast
#endif

#pragma clang diagnostic pop
