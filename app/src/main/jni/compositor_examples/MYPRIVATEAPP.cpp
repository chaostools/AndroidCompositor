//
// Created by konek on 8/14/2019.
//

#include "../GLIS.h"

const char *vertexSource = R"glsl( #version 320 es
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec2 aTexCoord;

out vec3 ourColor;
out vec2 TexCoord;

void main()
{
    gl_Position = vec4(aPos, 1.0);
    ourColor = aColor;
    TexCoord = aTexCoord;
}
)glsl";

const char *fragmentSource = R"glsl( #version 320 es
out highp vec4 FragColor;
in highp vec3 ourColor;

void main()
{
    FragColor = vec4(ourColor, 1.0);
}
)glsl";

class GLIS_CLASS G;
int main() {
    int W = 1000;
    int H = 1000;
    if (GLIS_setupOffScreenRendering(G, W, H)) {
        GLIS_error_to_string();
        // create a new texture
        GLuint FB;
        GLuint RB;
        GLuint renderedTexture;
        GLIS_texture_buffer(FB, RB, renderedTexture, W, H);

        GLuint shaderProgram;
        GLuint vertexShader;
        GLuint fragmentShader;
        vertexShader = GLIS_createShader(GL_VERTEX_SHADER, vertexSource);
        fragmentShader = GLIS_createShader(GL_FRAGMENT_SHADER, fragmentSource);
        LOG_INFO("Creating Shader program");
        shaderProgram = GLIS_error_to_string_exec_GL(glCreateProgram());
        LOG_INFO("Attaching vertex Shader to program");
        GLIS_error_to_string_exec_GL(glAttachShader(shaderProgram, vertexShader));
        LOG_INFO("Attaching fragment Shader to program");
        GLIS_error_to_string_exec_GL(glAttachShader(shaderProgram, fragmentShader));
        LOG_INFO("Linking Shader program");
        GLIS_error_to_string_exec_GL(glLinkProgram(shaderProgram));
        LOG_INFO("Validating Shader program");
        GLboolean ProgramIsValid = GLIS_validate_program(shaderProgram);
        assert(ProgramIsValid == GL_TRUE);

        LOG_INFO("Using Shader program");
        GLIS_error_to_string_exec_GL(glUseProgram(shaderProgram));
//        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
//        glClear(GL_COLOR);
        GLIS_draw_rectangle<GLint>(GL_TEXTURE0, renderedTexture, 0, 0, 0, W, H, W, H);

        double program_start = now_ms();
        LOG_INFO("creating 21 windows");
        LOG_INFO("creating window %d", 0);
        size_t win_id1 = GLIS_new_window(0, 0, W, H);
        LOG_INFO("window id: %zu", win_id1);
        SERVER_LOG_TRANSFER_INFO = true;
        GLIS_upload_texture(G, win_id1, renderedTexture, W, H);
        LOG_INFO("created window %d", 0);
        for (int i = 0; i < 20; i++) {
            LOG_INFO("creating window %d", i + 1);
            int s = 50;
            size_t win_id2 = GLIS_new_window(i * s, i * s, s, s);
            LOG_INFO("window id: %zu", win_id2);
            GLIS_upload_texture_resize(G, win_id2, renderedTexture, W, H, s, s);
            LOG_INFO("created window %d", i + 1);
        }
        double end = now_ms();
        LOG_INFO("created 21 windows in %G milliseconds", end - program_start);

        LOG_INFO("Cleaning up");
        GLIS_error_to_string_exec_GL(glDeleteProgram(shaderProgram));
        GLIS_error_to_string_exec_GL(glDeleteShader(fragmentShader));
        GLIS_error_to_string_exec_GL(glDeleteShader(vertexShader));
        GLIS_error_to_string_exec_GL(glDeleteTextures(1, &renderedTexture));
        GLIS_error_to_string_exec_GL(glDeleteRenderbuffers(1, &RB));
        GLIS_error_to_string_exec_GL(glDeleteFramebuffers(1, &FB));
        GLIS_destroy_GLIS(G);
        LOG_INFO("Destroyed sub Compositor GLIS");
        LOG_INFO("Cleaned up");
    }
    return 0;
}