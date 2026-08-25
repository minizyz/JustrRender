/*
 * JustrRender - FSR 1.0 Implementation
 * AMD FidelityFX Super Resolution 1.0 for OpenGL ES 3.0.
 * EASU (Edge Adaptive Spatial Upsampling) + RCAS (Robust Contrast Adaptive Sharpening).
 */

#include "fsr_render.h"
#include <android/log.h>
#include <stdlib.h>
#include <string.h>

#define LOG_TAG "JustrRender-FSR"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static const char *QUAD_VERT =
    "#version 300 es\n"
    "layout(location=0) in vec2 aPos;\n"
    "layout(location=1) in vec2 aTex;\n"
    "out vec2 vTex;\n"
    "void main(){vTex=aTex;gl_Position=vec4(aPos,0.0,1.0);}\n";

static const char *EASU_FRAG =
    "#version 300 es\n"
    "precision highp float;\n"
    "in vec2 vTex;\n"
    "out vec4 fragColor;\n"
    "uniform sampler2D uTex;\n"
    "uniform vec2 uInputSize;\n"
    "vec3 samp(vec2 uv){return texture(uTex,clamp(uv,vec2(0.0),vec2(1.0))).rgb;}\n"
    "void main(){\n"
    "  vec2 texel=1.0/uInputSize;\n"
    "  vec2 pos=vTex*uInputSize-0.5;\n"
    "  vec2 base=floor(pos);vec2 frac=pos-base;\n"
    "  vec2 uv0=(base+0.5)*texel;\n"
    "  vec3 c00=samp(uv0+vec2(-1,-1)*texel),c10=samp(uv0+vec2(0,-1)*texel),c20=samp(uv0+vec2(1,-1)*texel);\n"
    "  vec3 c01=samp(uv0+vec2(-1,0)*texel),c11=samp(uv0),c21=samp(uv0+vec2(1,0)*texel);\n"
    "  vec3 c02=samp(uv0+vec2(-1,1)*texel),c12=samp(uv0+vec2(0,1)*texel),c22=samp(uv0+vec2(1,1)*texel);\n"
    "  float L(vec3 c){return dot(c,vec3(0.2126,0.7152,0.0722));}\n"
    "  float l00=L(c00),l10=L(c10),l20=L(c20),l01=L(c01),l11=L(c11),l21=L(c21),l02=L(c02),l12=L(c12),l22=L(c22);\n"
    "  float eH=abs(l01-l21)+abs(l00-l20)*0.5+abs(l02-l22)*0.5;\n"
    "  float eV=abs(l10-l12)+abs(l00-l02)*0.5+abs(l20-l22)*0.5;\n"
    "  float eD=abs(l00-l22)+abs(l20-l02);\n"
    "  vec3 result;\n"
    "  if(eH<eV&&eH<eD){\n"
    "    vec3 t=mix(c10,c11,frac.y),b=mix(c11,c12,frac.y);result=mix(t,b,frac.y);\n"
    "    vec3 l=mix(c01,c11,frac.x),r=mix(c11,c21,frac.x);result=mix(result,mix(l,r,frac.x),0.3);\n"
    "  } else if(eV<eH&&eV<eD){\n"
    "    vec3 l=mix(c01,c11,frac.x),r=mix(c11,c21,frac.x);result=mix(l,r,frac.x);\n"
    "    vec3 t=mix(c10,c11,frac.y),b=mix(c11,c12,frac.y);result=mix(result,mix(t,b,frac.y),0.3);\n"
    "  } else {\n"
    "    vec3 t=mix(mix(c00,c10,frac.x),mix(c01,c11,frac.x),frac.y);\n"
    "    vec3 b=mix(mix(c01,c11,frac.x),mix(c02,c12,frac.x),frac.y);\n"
    "    result=mix(t,b,frac.y);\n"
    "  }\n"
    "  vec3 col0=mix(c01,c11,smoothstep(0.0,1.0,frac.x));\n"
    "  vec3 col1=mix(c11,c21,smoothstep(0.0,1.0,frac.x));\n"
    "  vec3 bilinear=mix(col0,col1,smoothstep(0.0,1.0,frac.y));\n"
    "  fragColor=vec4(mix(result,bilinear,0.4),1.0);\n"
    "}\n";

static const char *RCAS_FRAG =
    "#version 300 es\n"
    "precision highp float;\n"
    "in vec2 vTex;\n"
    "out vec4 fragColor;\n"
    "uniform sampler2D uTex;\n"
    "uniform float uSharp;\n"
    "uniform vec2 uViewport;\n"
    "vec3 samp(vec2 uv){return texture(uTex,clamp(uv,vec2(0.0),vec2(1.0))).rgb;}\n"
    "void main(){\n"
    "  vec2 texel=1.0/uViewport;\n"
    "  vec3 c=samp(vTex);\n"
    "  vec3 n=samp(vTex+vec2(0,-texel.y)),s=samp(vTex+vec2(0,texel.y));\n"
    "  vec3 e=samp(vTex+vec2(texel.x,0)),w=samp(vTex+vec2(-texel.x,0));\n"
    "  float L(vec3 col){return dot(col,vec3(0.2126,0.7152,0.0722));}\n"
    "  float lc=L(c),ln=L(n),ls=L(s),le=L(e),lw=L(w);\n"
    "  float lmin=min(lc,min(min(ln,ls),min(le,lw)));\n"
    "  float lmax=max(lc,max(max(ln,ls),max(le,lw)));\n"
    "  float contrast=lmax-lmin;\n"
    "  if(contrast<0.001){fragColor=vec4(c,1.0);return;}\n"
    "  float avg=(ln+ls+le+lw)*0.25;\n"
    "  float dMin=min(min(ln,ls),min(le,lw))-lc;\n"
    "  float dMax=max(max(ln,ls),max(le,lw))-lc;\n"
    "  float d=(abs(dMin)<abs(dMax))?dMin:dMax;\n"
    "  float wScale=clamp(1.0-abs(d)/contrast,0.0,1.0);\n"
    "  float amount=uSharp*2.0*wScale*0.5;\n"
    "  vec3 result=clamp(c+(c-avg)*amount,0.0,1.0);\n"
    "  fragColor=vec4(result,1.0);\n"
    "}\n";

static GLuint compile_shader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint status;
    glGetShaderiv(s, GL_COMPILE_STATUS, &status);
    if (!status) {
        GLint len; glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
        char *log = malloc(len); glGetShaderInfoLog(s, len, NULL, log);
        LOGE("Shader error: %s", log); free(log);
        glDeleteShader(s); return 0;
    }
    return s;
}

static GLuint create_program(const char *vs, const char *fs) {
    GLuint v = compile_shader(GL_VERTEX_SHADER, vs);
    if (!v) return 0;
    GLuint f = compile_shader(GL_FRAGMENT_SHADER, fs);
    if (!f) { glDeleteShader(v); return 0; }
    GLuint p = glCreateProgram();
    glAttachShader(p, v); glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v); glDeleteShader(f);
    GLint status; glGetProgramiv(p, GL_LINK_STATUS, &status);
    if (!status) {
        GLint len; glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
        char *log = malloc(len); glGetProgramInfoLog(p, len, NULL, log);
        LOGE("Link error: %s", log); free(log);
        glDeleteProgram(p); return 0;
    }
    return p;
}

static const float QUAD[] = {
    -1,-1,0,0, 1,-1,1,0, -1,1,0,1, 1,1,1,1
};

static void create_quad(justr_fsr_context_t *ctx) {
    glGenVertexArrays(1, &ctx->quad_vao);
    glGenBuffers(1, &ctx->quad_vbo);
    glBindVertexArray(ctx->quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, ctx->quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(QUAD), QUAD, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
    glBindVertexArray(0);
}

static bool create_rt(GLuint *fbo, GLuint *tex, int w, int h) {
    glGenTextures(1, tex);
    glBindTexture(GL_TEXTURE_2D, *tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *tex, 0);
    GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (st != GL_FRAMEBUFFER_COMPLETE) {
        LOGE("FBO incomplete 0x%x", st);
        glDeleteFramebuffers(1, fbo); glDeleteTextures(1, tex);
        *fbo = 0; *tex = 0; return false;
    }
    return true;
}

static void destroy_rt(GLuint *fbo, GLuint *tex) {
    if (*fbo) { glDeleteFramebuffers(1, fbo); *fbo = 0; }
    if (*tex) { glDeleteTextures(1, tex); *tex = 0; }
}

static void realloc_rts(justr_fsr_context_t *ctx) {
    if (ctx->output_width <= 0 || ctx->output_height <= 0) return;
    ctx->input_width = (int)(ctx->output_width / ctx->scale_factor);
    ctx->input_height = (int)(ctx->output_height / ctx->scale_factor);
    LOGI("FSR realloc: %dx%d -> %dx%d (%.2fx)",
         ctx->input_width, ctx->input_height,
         ctx->output_width, ctx->output_height, ctx->scale_factor);
    destroy_rt(&ctx->input_fbo, &ctx->input_texture);
    if (!create_rt(&ctx->input_fbo, &ctx->input_texture, ctx->input_width, ctx->input_height)) return;
    destroy_rt(&ctx->output_fbo, &ctx->output_texture);
    if (ctx->scale_factor > 1.01f)
        create_rt(&ctx->output_fbo, &ctx->output_texture, ctx->output_width, ctx->output_height);
}

float justr_fsr_get_scale_factor(justr_fsr_mode_t mode) {
    switch (mode) {
        case JUSTR_FSR_ULTRA_QUALITY: return 1.3f;
        case JUSTR_FSR_QUALITY: return 1.5f;
        case JUSTR_FSR_BALANCED: return 1.7f;
        case JUSTR_FSR_PERFORMANCE: return 2.0f;
        default: return 1.0f;
    }
}

const char *justr_fsr_get_mode_name(justr_fsr_mode_t mode) {
    switch (mode) {
        case JUSTR_FSR_OFF: return "Off";
        case JUSTR_FSR_ULTRA_QUALITY: return "Ultra Quality";
        case JUSTR_FSR_QUALITY: return "Quality";
        case JUSTR_FSR_BALANCED: return "Balanced";
        case JUSTR_FSR_PERFORMANCE: return "Performance";
        case JUSTR_FSR_CUSTOM: return "Custom";
        default: return "Unknown";
    }
}

bool justr_fsr_init(justr_fsr_context_t *ctx, int ow, int oh) {
    if (!ctx) return false;
    if (ctx->initialized) justr_fsr_terminate(ctx);
    memset(ctx, 0, sizeof(*ctx));
    ctx->mode = JUSTR_FSR_OFF;
    ctx->scale_factor = 1.0f;
    ctx->sharpening = 0.5f;
    ctx->output_width = ow;
    ctx->output_height = oh;

    ctx->easu_program = create_program(QUAD_VERT, EASU_FRAG);
    if (!ctx->easu_program) return false;
    ctx->rcas_program = create_program(QUAD_VERT, RCAS_FRAG);
    if (!ctx->rcas_program) { glDeleteProgram(ctx->easu_program); ctx->easu_program = 0; return false; }

    ctx->easu_tex_loc = glGetUniformLocation(ctx->easu_program, "uTex");
    ctx->easu_input_size_loc = glGetUniformLocation(ctx->easu_program, "uInputSize");
    ctx->rcas_tex_loc = glGetUniformLocation(ctx->rcas_program, "uTex");
    ctx->rcas_sharp_loc = glGetUniformLocation(ctx->rcas_program, "uSharp");
    ctx->rcas_viewport_loc = glGetUniformLocation(ctx->rcas_program, "uViewport");

    create_quad(ctx);
    ctx->initialized = true;
    LOGI("FSR initialized (%dx%d)", ow, oh);
    return true;
}

void justr_fsr_terminate(justr_fsr_context_t *ctx) {
    if (!ctx || !ctx->initialized) return;
    glDeleteVertexArrays(1, &ctx->quad_vao);
    glDeleteBuffers(1, &ctx->quad_vbo);
    if (ctx->easu_program) glDeleteProgram(ctx->easu_program);
    if (ctx->rcas_program) glDeleteProgram(ctx->rcas_program);
    destroy_rt(&ctx->input_fbo, &ctx->input_texture);
    destroy_rt(&ctx->output_fbo, &ctx->output_texture);
    ctx->initialized = false;
    LOGI("FSR terminated");
}

void justr_fsr_set_mode(justr_fsr_context_t *ctx, justr_fsr_mode_t mode) {
    if (!ctx || ctx->mode == mode) return;
    ctx->mode = mode;
    if (mode != JUSTR_FSR_CUSTOM) ctx->scale_factor = justr_fsr_get_scale_factor(mode);
    if (ctx->initialized && mode != JUSTR_FSR_OFF) realloc_rts(ctx);
}

void justr_fsr_set_custom_scale(justr_fsr_context_t *ctx, float s) {
    if (!ctx) return;
    ctx->scale_factor = s; ctx->mode = JUSTR_FSR_CUSTOM;
    if (ctx->initialized) realloc_rts(ctx);
}

void justr_fsr_set_sharpening(justr_fsr_context_t *ctx, float a) {
    if (!ctx) return;
    ctx->sharpening = a < 0 ? 0 : (a > 1 ? 1 : a);
}

void justr_fsr_set_output_size(justr_fsr_context_t *ctx, int w, int h) {
    if (!ctx || (ctx->output_width == w && ctx->output_height == h)) return;
    ctx->output_width = w; ctx->output_height = h;
    if (ctx->initialized && ctx->mode != JUSTR_FSR_OFF) realloc_rts(ctx);
}

bool justr_fsr_begin(justr_fsr_context_t *ctx) {
    if (!ctx || !ctx->initialized || ctx->mode == JUSTR_FSR_OFF) return false;
    if (!ctx->input_fbo) { realloc_rts(ctx); if (!ctx->input_fbo) return false; }
    glBindFramebuffer(GL_FRAMEBUFFER, ctx->input_fbo);
    glViewport(0, 0, ctx->input_width, ctx->input_height);
    return true;
}

void justr_fsr_end(justr_fsr_context_t *ctx) {
    if (!ctx || !ctx->initialized || ctx->mode == JUSTR_FSR_OFF) return;
    glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND); glDisable(GL_CULL_FACE);

    if (ctx->output_fbo) {
        glBindFramebuffer(GL_FRAMEBUFFER, ctx->output_fbo);
        glViewport(0, 0, ctx->output_width, ctx->output_height);
        glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(ctx->easu_program);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, ctx->input_texture);
        glUniform1i(ctx->easu_tex_loc, 0);
        glUniform2f(ctx->easu_input_size_loc, (float)ctx->input_width, (float)ctx->input_height);
        glBindVertexArray(ctx->quad_vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, ctx->output_width, ctx->output_height);
    glUseProgram(ctx->rcas_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ctx->output_fbo ? ctx->output_texture : ctx->input_texture);
    glUniform1i(ctx->rcas_tex_loc, 0);
    glUniform1f(ctx->rcas_sharp_loc, ctx->sharpening);
    glUniform2f(ctx->rcas_viewport_loc, (float)ctx->output_width, (float)ctx->output_height);
    glBindVertexArray(ctx->quad_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    glUseProgram(0);
}

void justr_fsr_get_input_size(justr_fsr_context_t *ctx, int *w, int *h) {
    if (!ctx) return;
    if (w) *w = ctx->input_width;
    if (h) *h = ctx->input_height;
}

void justr_fsr_get_output_size(justr_fsr_context_t *ctx, int *w, int *h) {
    if (!ctx) return;
    if (w) *w = ctx->output_width;
    if (h) *h = ctx->output_height;
}
