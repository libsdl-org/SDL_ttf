/*
  testgltext:  An example of using the SDL_ttf GL text engine with OpenGL.
  Copyright (C) 2025 Ivan Vecera <ivan@cera.cz>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>

#ifdef HAVE_OPENGL

#define SDL_OPENGL_1_FUNCTION_TYPEDEFS
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>

#define SDL_MATH_3D_IMPLEMENTATION
#include "testgputext/SDL_math3d.h"

typedef struct
{
    PFNGLATTACHSHADERPROC AttachShader;
    PFNGLBINDBUFFERPROC BindBuffer;
    PFNGLBINDTEXTUREPROC BindTexture;
    PFNGLBLENDFUNCPROC BlendFunc;
    PFNGLBUFFERDATAPROC BufferData;
    PFNGLCLEARPROC Clear;
    PFNGLCLEARCOLORPROC ClearColor;
    PFNGLCOMPILESHADERPROC CompileShader;
    PFNGLCREATEPROGRAMPROC CreateProgram;
    PFNGLCREATESHADERPROC CreateShader;
    PFNGLDELETEBUFFERSPROC DeleteBuffers;
    PFNGLDELETEPROGRAMPROC DeleteProgram;
    PFNGLDELETESHADERPROC DeleteShader;
    PFNGLDISABLEVERTEXATTRIBARRAYPROC DisableVertexAttribArray;
    PFNGLDRAWELEMENTSPROC DrawElements;
    PFNGLENABLEPROC Enable;
    PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray;
    PFNGLGENBUFFERSPROC GenBuffers;
    PFNGLGETATTRIBLOCATIONPROC GetAttribLocation;
    PFNGLGETINTEGERVPROC GetIntegerv;
    PFNGLGETPROGRAMINFOLOGPROC GetProgramInfoLog;
    PFNGLGETPROGRAMIVPROC GetProgramiv;
    PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog;
    PFNGLGETSHADERIVPROC GetShaderiv;
    PFNGLGETUNIFORMLOCATIONPROC GetUniformLocation;
    PFNGLLINKPROGRAMPROC LinkProgram;
    PFNGLSHADERSOURCEPROC ShaderSource;
    PFNGLUNIFORM1IPROC Uniform1i;
    PFNGLUNIFORM4FPROC Uniform4f;
    PFNGLUNIFORMMATRIX4FVPROC UniformMatrix4fv;
    PFNGLUSEPROGRAMPROC UseProgram;
    PFNGLVERTEXATTRIBPOINTERPROC VertexAttribPointer;
} GL_API;

static GL_API gl;

static bool Init_GL_API(GL_API *gl)
{
#define LOAD(name)                                                                \
    gl->name = (typeof(gl->name))SDL_GL_GetProcAddress("gl" #name);               \
    if (!gl->name) {                                                              \
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load gl%s", #name); \
        return false;                                                             \
    }
    LOAD(AttachShader)
    LOAD(BindBuffer)
    LOAD(BindTexture)
    LOAD(BlendFunc)
    LOAD(BufferData)
    LOAD(Clear)
    LOAD(ClearColor)
    LOAD(CompileShader)
    LOAD(CreateProgram)
    LOAD(CreateShader)
    LOAD(DeleteBuffers)
    LOAD(DeleteProgram)
    LOAD(DeleteShader)
    LOAD(DisableVertexAttribArray)
    LOAD(DrawElements)
    LOAD(Enable)
    LOAD(EnableVertexAttribArray)
    LOAD(GenBuffers)
    LOAD(GetAttribLocation)
    LOAD(GetIntegerv)
    LOAD(GetProgramInfoLog)
    LOAD(GetProgramiv)
    LOAD(GetShaderInfoLog)
    LOAD(GetShaderiv)
    LOAD(GetUniformLocation)
    LOAD(LinkProgram)
    LOAD(ShaderSource)
    LOAD(Uniform1i)
    LOAD(Uniform4f)
    LOAD(UniformMatrix4fv)
    LOAD(UseProgram)
    LOAD(VertexAttribPointer)
#undef LOAD
    return true;
}

/* Shader sources */

static const char *vertex_shader_src =
    "attribute vec2 a_position;\n"
    "attribute vec2 a_texcoord;\n"
    "uniform mat4 u_projection;\n"
    "uniform mat4 u_model;\n"
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "    v_texcoord = a_texcoord;\n"
    "    gl_Position = u_projection * u_model * vec4(a_position, 0.0, 1.0);\n"
    "}\n";

static const char *fragment_shader_src =
    "#ifdef GL_ES\n"
    "#extension GL_OES_standard_derivatives : enable\n"
    "precision mediump float;\n"
    "#endif\n"
    "uniform sampler2D u_texture;\n"
    "uniform vec4 u_text_color;\n"
    "uniform vec4 u_outline_color;\n"
    "uniform bool u_atlas;\n"
    "uniform bool u_sdf;\n"
    "varying vec2 v_texcoord;\n"

    "void main() {\n"
    "    // Atlas & SDF calculations\n"
    "    vec4 texel = texture2D(u_texture, v_texcoord);\n"
    "    float d = texel.a;\n"
    "    float w = fwidth(d);\n"

    "    // SDF outer edge of the outline vs. inner text body\n"
    "    float outer = smoothstep(0.50 - w, 0.50 + w, d);\n"
    "    float inner = smoothstep(0.55 - w, 0.55 + w, d);\n"

    "    // Color composition for SDF rendering\n"
    "    vec4 color1 = mix(u_outline_color, u_text_color, inner);\n"
    "    color1.a *= outer;\n"

    "    // Select between regular texturing and SDF rendering\n"
    "    color1 = mix(u_text_color * texel, color1, float(u_sdf));\n"

    "    // Procedural AA rectangle\n"
    "    vec2 dd = (0.5 - abs(v_texcoord - 0.5)) / fwidth(v_texcoord);\n"
    "    float aa = clamp(min(dd.x, dd.y), 0.0, 1.0);\n"
    "    vec4 color2 = vec4(u_text_color.rgb, u_text_color.a * aa);\n"

    "    // Final branchless selection\n"
    "    gl_FragColor = mix(color2, color1, float(u_atlas));\n"
    "}\n";

static GLuint CompileShader(GLenum type, const char *source)
{
    GLuint shader = gl.CreateShader(type);
    GLint status;

    gl.ShaderSource(shader, 1, &source, NULL);
    gl.CompileShader(shader);
    gl.GetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        char log[512];
        gl.GetShaderInfoLog(shader, sizeof(log), NULL, log);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Shader compile error: %s", log);
        gl.DeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint CreateProgram(const char *vert_src, const char *frag_src)
{
    GLuint vert = CompileShader(GL_VERTEX_SHADER, vert_src);
    if (!vert) {
        return 0;
    }

    GLuint frag = CompileShader(GL_FRAGMENT_SHADER, frag_src);
    if (!frag) {
        gl.DeleteShader(vert);
        return 0;
    }

    GLuint program = gl.CreateProgram();
    GLint status;

    gl.AttachShader(program, vert);
    gl.AttachShader(program, frag);
    gl.LinkProgram(program);
    gl.DeleteShader(vert);
    gl.DeleteShader(frag);

    gl.GetProgramiv(program, GL_LINK_STATUS, &status);
    if (!status) {
        char log[512];
        gl.GetProgramInfoLog(program, sizeof(log), NULL, log);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Program link error: %s", log);
        gl.DeleteProgram(program);
        return 0;
    }
    return program;
}

typedef struct
{
    GLuint program;
    GLint a_position;
    GLint a_texcoord;
    GLint u_projection;
    GLint u_model;
    GLint u_text_color;
    GLint u_outline_color;
    GLint u_texture;
    GLint u_atlas;
    GLint u_sdf;
} ShaderProgram;

static bool InitProgram(ShaderProgram *sp, const char *vert_src, const char *frag_src)
{
    sp->program = CreateProgram(vert_src, frag_src);
    if (!sp->program) {
        return false;
    }
    sp->a_position = gl.GetAttribLocation(sp->program, "a_position");
    sp->a_texcoord = gl.GetAttribLocation(sp->program, "a_texcoord");
    sp->u_projection = gl.GetUniformLocation(sp->program, "u_projection");
    sp->u_model = gl.GetUniformLocation(sp->program, "u_model");
    sp->u_text_color = gl.GetUniformLocation(sp->program, "u_text_color");
    sp->u_outline_color = gl.GetUniformLocation(sp->program, "u_outline_color");
    sp->u_texture = gl.GetUniformLocation(sp->program, "u_texture");
    sp->u_atlas = gl.GetUniformLocation(sp->program, "u_atlas");
    sp->u_sdf = gl.GetUniformLocation(sp->program, "u_sdf");
    return true;
}

static void DrawText(ShaderProgram *sp, bool use_sdf, GLuint vbo, GLuint ebo,
                     TTF_GLAtlasDrawSequence *sequence,
                     SDL_Mat4X4 *projection, SDL_Mat4X4 *model,
                     SDL_FColor *color, SDL_FColor *out_color)
{
    gl.UseProgram(sp->program);
    gl.UniformMatrix4fv(sp->u_projection, 1, GL_FALSE, &projection->m[0][0]);
    gl.UniformMatrix4fv(sp->u_model, 1, GL_FALSE, &model->m[0][0]);
    gl.Uniform4f(sp->u_text_color, color->r, color->g, color->b, color->a);
    gl.Uniform4f(sp->u_outline_color, out_color->r, out_color->g, out_color->b, out_color->a);
    gl.Uniform1i(sp->u_sdf, use_sdf);

    for (TTF_GLAtlasDrawSequence *seq = sequence; seq; seq = seq->next) {
        gl.Uniform1i(sp->u_atlas, seq->atlas_texture != 0);

        if (seq->atlas_texture) {
            gl.BindTexture(GL_TEXTURE_2D, seq->atlas_texture);
            gl.Uniform1i(sp->u_texture, 0);
        }

        gl.BindBuffer(GL_ARRAY_BUFFER, vbo);
        gl.BufferData(GL_ARRAY_BUFFER, seq->num_vertices * sizeof(TTF_GLAtlasDrawVertex), seq->vertices, GL_STREAM_DRAW);

        gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        gl.BufferData(GL_ELEMENT_ARRAY_BUFFER, seq->num_indices * sizeof(Uint16), seq->indices, GL_STREAM_DRAW);

        GLsizei stride = sizeof(TTF_GLAtlasDrawVertex);
        gl.EnableVertexAttribArray(sp->a_position);
        gl.VertexAttribPointer(sp->a_position, 2, GL_FLOAT, GL_FALSE, stride, (void *)0);
        gl.EnableVertexAttribArray(sp->a_texcoord);
        gl.VertexAttribPointer(sp->a_texcoord, 2, GL_FLOAT, GL_FALSE, stride, (void *)sizeof(SDL_FPoint));

        gl.DrawElements(GL_TRIANGLES, seq->num_indices, GL_UNSIGNED_SHORT, NULL);

        gl.DisableVertexAttribArray(sp->a_position);
        gl.DisableVertexAttribArray(sp->a_texcoord);
    }
}

int main(int argc, char *argv[])
{
    const char *font_filename = NULL;
    bool use_SDF = false;
    bool use_GLES = false;
    int style;

    (void)argc;
    for (int i = 1; argv[i]; ++i) {
        if (SDL_strcasecmp(argv[i], "--sdf") == 0) {
            use_SDF = true;
        } else if (SDL_strcasecmp(argv[i], "--gles") == 0) {
            use_GLES = true;
        } else if (*argv[i] == '-') {
            break;
        } else {
            font_filename = argv[i];
            break;
        }
    }
    if (!font_filename) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Usage: testgltext [--sdf] [--gles] FONT_FILENAME");
        return 2;
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    if (use_GLES) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    }

    SDL_Window *window = SDL_CreateWindow("GL text test", 800, 600, SDL_WINDOW_OPENGL);
    if (!window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_GL_CreateContext failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Log("Using OpenGL%s", use_GLES ? " ES" : "");

    if (!Init_GL_API(&gl)) {
        SDL_GL_DestroyContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    ShaderProgram prog;
    if (!InitProgram(&prog, vertex_shader_src, fragment_shader_src)) {
        SDL_GL_DestroyContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (!SDL_GL_SetSwapInterval(-1)) {
        SDL_GL_SetSwapInterval(1);
    }

    GLuint vbo, ebo;
    gl.GenBuffers(1, &vbo);
    gl.GenBuffers(1, &ebo);

    gl.Enable(GL_BLEND);
    gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (!TTF_Init()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "TTF_Init failed: %s", SDL_GetError());
        SDL_GL_DestroyContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    TTF_Font *font = TTF_OpenFont(font_filename, 50);
    if (!font) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "TTF_OpenFont failed: %s", SDL_GetError());
        TTF_Quit();
        SDL_GL_DestroyContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Log("SDF %s", use_SDF ? "enabled" : "disabled");
    TTF_SetFontSDF(font, use_SDF);
    TTF_SetFontWrapAlignment(font, TTF_HORIZONTAL_ALIGN_CENTER);

    TTF_TextEngine *engine = TTF_CreateGLTextEngine();
    if (!engine) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "TTF_CreateGLTextEngine failed: %s", SDL_GetError());
        TTF_CloseFont(font);
        TTF_Quit();
        SDL_GL_DestroyContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    char str[] = "     \nSDL is cool";
    TTF_Text *text = TTF_CreateText(engine, font, str, 0);
    if (!text) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "TTF_CreateText failed: %s", SDL_GetError());
        TTF_DestroyGLTextEngine(engine);
        TTF_CloseFont(font);
        TTF_Quit();
        SDL_GL_DestroyContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Mat4X4 projection = SDL_MatrixPerspective(SDL_PI_F / 2.0f, 800.0f / 600.0f, 0.1f, 100.0f);

    SDL_FColor color = { 1.0f, 1.0f, 0.0f, 1.0f };
    SDL_FColor out_color = { 0.0f, 0.0f, 0.0f, 1.0f };
    bool running = true;
    Uint64 last_ticks = 0;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_KEY_UP:
                switch (event.key.key) {
                case SDLK_S:
                    style = TTF_GetFontStyle(font);
                    if (style & TTF_STYLE_STRIKETHROUGH) {
                        style &= ~TTF_STYLE_STRIKETHROUGH;
                    } else {
                        style |= TTF_STYLE_STRIKETHROUGH;
                    }
                    TTF_SetFontStyle(font, style);
                    break;

                case SDLK_U:
                    style = TTF_GetFontStyle(font);
                    if (style & TTF_STYLE_UNDERLINE) {
                        style &= ~TTF_STYLE_UNDERLINE;
                    } else {
                        style |= TTF_STYLE_UNDERLINE;
                    }
                    TTF_SetFontStyle(font, style);
                    break;

                case SDLK_ESCAPE:
                    running = false;
                    break;

                default:
                    break;
                }
                break;
            case SDL_EVENT_QUIT:
                running = false;
                break;
            }
        }

        Uint64 cur_ticks = SDL_GetTicks();
        if (cur_ticks - last_ticks >= 250) {
            for (int i = 0; i < 5; i++) {
                str[i] = 65 + SDL_rand(26);
            }
            TTF_SetTextString(text, str, 0);
            last_ticks = cur_ticks;
        }

        int tw, th;
        TTF_GetTextSize(text, &tw, &th);

        float rot_angle = SDL_fmodf(SDL_GetTicks() / 1000.0f, 2 * SDL_PI_F);

        SDL_Mat4X4 model = SDL_MatrixIdentity();
        model = SDL_MatrixMultiply(model, SDL_MatrixTranslation((SDL_Vec3){ 0.0f, 0.0f, -80.0f }));
        model = SDL_MatrixMultiply(model, SDL_MatrixScaling((SDL_Vec3){ 0.3f, 0.3f, 0.3f }));
        model = SDL_MatrixMultiply(model, SDL_MatrixRotationY(rot_angle));
        model = SDL_MatrixMultiply(model, SDL_MatrixTranslation((SDL_Vec3){ -tw / 2.0f, th / 2.0f, 0.0f }));

        gl.ClearColor(0.3f, 0.4f, 0.5f, 1.0f);
        gl.Clear(GL_COLOR_BUFFER_BIT);

        TTF_GLAtlasDrawSequence *sequence = TTF_GetGLTextDrawData(text);
        DrawText(&prog, use_SDF, vbo, ebo, sequence, &projection, &model, &color, &out_color);

        SDL_GL_SwapWindow(window);
    }

    gl.DeleteBuffers(1, &vbo);
    gl.DeleteBuffers(1, &ebo);
    gl.DeleteProgram(prog.program);

    TTF_DestroyText(text);
    TTF_DestroyGLTextEngine(engine);
    TTF_CloseFont(font);
    TTF_Quit();

    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

#else /* HAVE_OPENGL */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    SDL_Log("No OpenGL support on this system");
    return 1;
}

#endif /* HAVE_OPENGL */
