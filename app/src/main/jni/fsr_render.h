/*
 * JustrRender - FSR 1.0 Super Resolution Implementation
 *
 * AMD FidelityFX Super Resolution 1.0 for OpenGL ES.
 * Provides spatial upscaling (EASU) and contrast-adaptive
 * sharpening (RCAS) to boost framerate with minimal quality loss.
 */

#ifndef JUSTR_FSR_RENDER_H
#define JUSTR_FSR_RENDER_H

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    JUSTR_FSR_OFF = 0,
    JUSTR_FSR_ULTRA_QUALITY = 1,
    JUSTR_FSR_QUALITY = 2,
    JUSTR_FSR_BALANCED = 3,
    JUSTR_FSR_PERFORMANCE = 4,
    JUSTR_FSR_CUSTOM = 5,
} justr_fsr_mode_t;

typedef struct {
    bool initialized;
    justr_fsr_mode_t mode;
    float scale_factor;
    float sharpening;

    GLuint input_fbo;
    GLuint input_texture;
    int input_width;
    int input_height;

    GLuint output_fbo;
    GLuint output_texture;
    int output_width;
    int output_height;

    GLuint easu_program;
    GLuint rcas_program;

    GLuint quad_vao;
    GLuint quad_vbo;

    GLint easu_tex_loc;
    GLint easu_viewport_loc;
    GLint easu_input_size_loc;
    GLint rcas_tex_loc;
    GLint rcas_sharp_loc;
    GLint rcas_viewport_loc;
} justr_fsr_context_t;

bool justr_fsr_init(justr_fsr_context_t *ctx, int output_width, int output_height);
void justr_fsr_terminate(justr_fsr_context_t *ctx);

void justr_fsr_set_mode(justr_fsr_context_t *ctx, justr_fsr_mode_t mode);
void justr_fsr_set_custom_scale(justr_fsr_context_t *ctx, float scale);
void justr_fsr_set_sharpening(justr_fsr_context_t *ctx, float amount);
void justr_fsr_set_output_size(justr_fsr_context_t *ctx, int width, int height);

bool justr_fsr_begin(justr_fsr_context_t *ctx);
void justr_fsr_end(justr_fsr_context_t *ctx);

void justr_fsr_get_input_size(justr_fsr_context_t *ctx, int *width, int *height);
void justr_fsr_get_output_size(justr_fsr_context_t *ctx, int *width, int *height);
float justr_fsr_get_scale_factor(justr_fsr_mode_t mode);
const char *justr_fsr_get_mode_name(justr_fsr_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif
