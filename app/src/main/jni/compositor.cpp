//
// Copyright 2011 Tero Saarni
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// my current understanding of all this is that a compositor will render each application's frame buffer,
// and a window manager such as KDE or GNOME or I3,
// will work WITH the compositor retrieving information about windows and their position,
// then draw boarders around those windows and implement either stacking or tiling like functionality
// depending on the windowing system type and assumably send information back to the compositor
// such as updates on window changes
// for example if the window is minimized or its position changes,
// the compositor will then redraw itself as it sees fit according to the received information

// X uses a buffer to draw from, and this buffer must have a supported pixel format,
// and applications which write to this buffer must obtain a handle to this buffer,
// then convert their pixel data to match the buffer's pixel format then write to buffer

//  A client can choose to use any pixel format the server understand.
//  Buffer allocation is nowadays done by the client, and handle passed to the server,
//  but the opposite also exists.

// all of this is fundamentally incompatible with networking, because it relies on shared memory

// in networking case the pixel data is sent to the server, along with control messages,
// in shared memory case it is stored directly in the buffer,
// and control messages get sent to the server from the client

#include <cstdint>
#include <jni.h>
#include <android/native_window.h> // requires ndk r5 or newer
#include <android/native_window_jni.h> // requires ndk r5 or newer
#include <pthread.h>
#include <vector>
#include <android/log.h>

#include "logger.h"
#include "GLIS.h"
#include "GLIS_COMMANDS.h"

#define LOG_TAG "EglSample"

class GLIS_CLASS CompositorMain;

char *executableDir;

extern "C" JNIEXPORT void JNICALL Java_glnative_example_NativeView_nativeSetSurface(JNIEnv* jenv,
                                                                                    jclass type,
                                                                                    jobject surface)
{
    if (surface != nullptr) {
        CompositorMain.native_window = ANativeWindow_fromSurface(jenv, surface);
        LOG_INFO("Got window %p", CompositorMain.native_window);
        LOG_INFO("waiting for Compositor to initialize");
        while (SYNC_STATE != STATE.initialized) {}
        LOG_INFO("requesting SERVER startup");
        SYNC_STATE = STATE.request_startup;
    } else {
        CompositorMain.server.shutdownServer();
        SYNC_STATE = STATE.request_shutdown;
        LOG_INFO("requesting SERVER shutdown");
        while (SYNC_STATE != STATE.response_shutdown) {}
        LOG_INFO("SERVER has shutdown");
        LOG_INFO("Releasing window");
        ANativeWindow_release(CompositorMain.native_window);
        CompositorMain.native_window = nullptr;
    }
}

const char *vertexSource = R"glsl( #version 320 es
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec2 aTexCoord;

out vec4 ourColor;
out vec2 TexCoord;

void main()
{
    gl_Position = vec4(aPos, 1.0);
    ourColor = vec4(aColor, 1.0);
    TexCoord = aTexCoord;
}
)glsl";

const char *fragmentSource = R"glsl( #version 320 es
out highp vec4 FragColor;

in highp vec4 ourColor;
in highp vec2 TexCoord;

uniform sampler2D texture1;
//uniform sampler2D texture2;

void main()
{
    FragColor = texture(texture1, TexCoord);
/*
    FragColor = mix(
        texture(texture1, TexCoord), // texture 1
        texture(texture2, TexCoord), // texture 2
        0.2 // interpolation,
        // If the third value is 0.0 it returns the first input
        // If it's 1.0 it returns the second input value.
        // A value of 0.2 will return 80% of the first input color and 20% of the second input color
        // resulting in a mixture of both our textures.
    );
*/
}
)glsl";

int COMPOSITORMAIN__() {
    LOG_INFO("called COMPOSITORMAIN__()");
    system(std::string(std::string("chmod -R 777 ") + executableDir).c_str());
    char *exe =
        const_cast<char *>(std::string(
            std::string(executableDir) + "/Arch/arm64-v8a/MYPRIVATEAPP").c_str());
    char *args[2] = {exe, 0};
//    GLIS_FORK(args[0], args);
    char *exe2 =
        const_cast<char *>(std::string(
            std::string(executableDir) + "/Arch/arm64-v8a/MovingWindows").c_str());
    char *args2[2] = {exe2, 0};
    GLIS_FORK(args2[0], args2);

    SYNC_STATE = STATE.initialized;
    while (SYNC_STATE != STATE.request_startup);
    LOG_INFO("starting up");
    SYNC_STATE = STATE.response_starting_up;
    LOG_INFO("initializing main Compositor");
    if (GLIS_setupOnScreenRendering(CompositorMain)) {
        if (IPC == IPC_MODE.shared_memory) {
            assert(GLIS_shared_memory_malloc(GLIS_INTERNAL_SHARED_MEMORY_TEXTURE_DATA,
                                             sizeof(size_t) +
                                             sizeof(int8_t) +
                                             (sizeof(GLuint) * CompositorMain.height *
                                              CompositorMain.width)));
            assert(GLIS_shared_memory_malloc(GLIS_INTERNAL_SHARED_MEMORY_PARAMETER,
                                             sizeof(size_t) + sizeof(int8_t) +
                                             (sizeof(int8_t) * 4)));
        }
        CompositorMain.server.startServer(SERVER_START_REPLY_MANUALLY);
        LOG_INFO("initialized main Compositor");
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
        // set up vertex data (and buffer(s)) and configure vertex attributes
        // ------------------------------------------------------------------
        GLIS_set_conversion_origin(GLIS_CONVERSION_ORIGIN_BOTTOM_LEFT);
        LOG_INFO("Using Shader program");
        GLIS_error_to_string_exec_GL(glUseProgram(shaderProgram));
        GLIS_error_to_string_exec_GL(glClearColor(0.0F, 0.0F, 1.0F, 1.0F));
        GLIS_error_to_string_exec_GL(glClear(GL_COLOR_BUFFER_BIT));
        SERVER_LOG_TRANSFER_INFO = true;
        SYNC_STATE = STATE.response_started_up;
        LOG_INFO("started up");
        struct Client_Window {
            int x;
            int y;
            int w;
            int h;
            GLuint TEXTURE;
        };
        GLIS_error_to_string_exec_GL(glClearColor(0.0F, 0.0F, 1.0F, 1.0F));
        GLIS_error_to_string_exec_GL(glClear(GL_COLOR_BUFFER_BIT));
        GLIS_error_to_string_exec_EGL(
            eglSwapBuffers(CompositorMain.display, CompositorMain.surface));
        GLIS_Sync_GPU();
        double program_start = now_ms();
        while(SYNC_STATE != STATE.request_shutdown) {
            double loop_start = now_ms();
            bool redraw = false;
            bool accepted = false;
            serializer in;
            serializer out;
            int command = -1;
            if (IPC == IPC_MODE.socket) {
                LOG_INFO_SERVER("%swaiting for connection", CompositorMain.server.TAG);
                if (CompositorMain.server.socket_accept()) {
                    LOG_INFO_SERVER("%sconnection obtained", CompositorMain.server.TAG);
                    CompositorMain.server.socket_get_serial(in);
                } else {
                    LOG_ERROR_SERVER("%sfailed to obtain a connection", CompositorMain.server.TAG);
                    goto draw;
                }
            } else if (IPC == IPC_MODE.shared_memory) {
                if (!CompositorMain.server.socket_accept_non_blocking()) {
                    if (CompositorMain.server.internaldata->server_should_close) continue;
                    if (GLIS_INTERNAL_SHARED_MEMORY_PARAMETER.reference_count != 0) {
                        LOG_INFO("reference_count != 0 , waiting for parameter");
                        double start = now_ms();
                        GLIS_shared_memory_read(GLIS_INTERNAL_SHARED_MEMORY_PARAMETER, in);
                        double end = now_ms();
                        LOG_INFO("read parameters in %G milliseconds", end - start);
                    } else continue;
                } else {
                    double start = now_ms();
                    CompositorMain.server.socket_get_serial(in);
                    double end = now_ms();
                    LOG_INFO("read serial in %G milliseconds", end - start);
                }
            }
            in.get<int>(&command);
            if (IPC == IPC_MODE.socket)
                LOG_INFO_SERVER("%scommand: %d (%s)",
                                CompositorMain.server.TAG, command,
                                GLIS_command_to_string(command));
            else
                LOG_INFO("command: %d (%s)", command, GLIS_command_to_string(command));
            if (command == GLIS_SERVER_COMMANDS.new_window) {
                redraw = true;
                int *win;
                assert(in.get_raw_pointer<int>(&win) == 4); // must have 4 indexes
                struct Client_Window *x = new struct Client_Window;
                x->x = win[0];
                x->y = win[1];
                x->w = win[2];
                x->h = win[3];
                size_t id = CompositorMain.KERNEL.table->findObject(
                    CompositorMain.KERNEL.newObject(0, 0, x));
                if (IPC == IPC_MODE.socket) {
                    LOG_INFO_SERVER("%swindow %zu: %d,%d,%d,%d",
                                    CompositorMain.server.TAG, id, win[0], win[1], win[2], win[3]);
                    if (SERVER_LOG_TRANSFER_INFO)
                        LOG_INFO_SERVER("%ssending id %zu", CompositorMain.server.TAG, id);
                } else {
                    LOG_INFO("window %zu: %d,%d,%d,%d", id, win[0], win[1], win[2], win[3]);
                    LOG_INFO("sending id %zu", id);
                }
                out.add<int>(id);
                if (IPC == IPC_MODE.socket) CompositorMain.server.socket_put_serial(out);
                else if (IPC == IPC_MODE.shared_memory)
                    GLIS_shared_memory_write(GLIS_INTERNAL_SHARED_MEMORY_PARAMETER, out);
                redraw = true;
            } else if (command == GLIS_SERVER_COMMANDS.modify_window) {
                redraw = true;
                size_t window_id;
                in.get<size_t>(&window_id);
                int *win;
                assert(in.get_raw_pointer<int>(&win) == 4); // must have 4 indexes
                assert(win != 0);
                assert(win != nullptr);
                assert(window_id >= 0);
                assert(CompositorMain.KERNEL.table->table[window_id] != nullptr);
                struct Client_Window *c = reinterpret_cast<Client_Window *>(
                    CompositorMain.KERNEL.table->table[window_id]->resource
                );
                c->x = win[0];
                c->y = win[1];
                c->w = win[2];
                c->h = win[3];
            } else if (command == GLIS_SERVER_COMMANDS.close_window) {
                redraw = true;
                size_t window_id;
                in.get<size_t>(&window_id);
                CompositorMain.KERNEL.table->DELETE(window_id);
            } else if (command == GLIS_SERVER_COMMANDS.texture) {
                redraw = true;
                size_t Client_id;
                in.get<size_t>(&Client_id);
                if (IPC == IPC_MODE.socket) {
                    if (SERVER_LOG_TRANSFER_INFO)
                        LOG_INFO_SERVER("%sreceived id: %zu", CompositorMain.server.TAG, Client_id);
                } else {
                    LOG_INFO("received id: %zu", Client_id);
                }
                GLint *tex_dimens;
                assert(in.get_raw_pointer<GLint>(&tex_dimens) == 2);
                if (IPC == IPC_MODE.socket) {
                    if (SERVER_LOG_TRANSFER_INFO)
                        LOG_INFO_SERVER("%sreceived w: %d, h: %d",
                                        CompositorMain.server.TAG, tex_dimens[0], tex_dimens[1]);
                } else {
                    LOG_INFO("received w: %d, h: %d", tex_dimens[0], tex_dimens[1]);
                }
                struct Client_Window *CW = static_cast<Client_Window *>(
                    CompositorMain.KERNEL.table->table[Client_id]->resource);
                GLIS_error_to_string_exec_GL(
                    glGenTextures(1, &CW->TEXTURE));
                GLIS_error_to_string_exec_GL(
                    glBindTexture(GL_TEXTURE_2D, CW->TEXTURE));
                GLuint *texdata = nullptr;
                if (IPC == IPC_MODE.shared_memory) {
                    LOG_INFO("reading texture");
                    GLIS_shared_memory_read_texture(GLIS_INTERNAL_SHARED_MEMORY_TEXTURE_DATA,
                                                    reinterpret_cast<int8_t **>(&texdata));
                    LOG_INFO("read texture");
                } else if (IPC == IPC_MODE.socket) {
                    in.get_raw_pointer<GLuint>(&texdata);
                }
                GLIS_error_to_string_exec_GL(
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_dimens[0], tex_dimens[1], 0,
                                 GL_RGBA, GL_UNSIGNED_BYTE, texdata)
                );
                if (texdata != nullptr) free(texdata);
                GLIS_error_to_string_exec_GL(glGenerateMipmap(GL_TEXTURE_2D));
                GLIS_error_to_string_exec_GL(
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                                    GL_NEAREST));
                GLIS_error_to_string_exec_GL(
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                    GL_LINEAR));
                GLIS_error_to_string_exec_GL(
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                                    GL_CLAMP_TO_BORDER));
                GLIS_error_to_string_exec_GL(
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                                    GL_CLAMP_TO_BORDER));
                GLIS_error_to_string_exec_GL(glBindTexture(GL_TEXTURE_2D, 0));
            } else if (command == GLIS_SERVER_COMMANDS.shm_texture) {
                double start = now_ms();
                assert(ashmem_valid(GLIS_INTERNAL_SHARED_MEMORY_TEXTURE_DATA.fd));
                LOG_INFO_SERVER("GLIS_INTERNAL_SHARED_MEMORY_PARAMETER.reference_count = %zu",
                                GLIS_INTERNAL_SHARED_MEMORY_TEXTURE_DATA.reference_count);
                GLIS_shared_memory_increase_reference(GLIS_INTERNAL_SHARED_MEMORY_TEXTURE_DATA);
                out.add<size_t>(GLIS_INTERNAL_SHARED_MEMORY_TEXTURE_DATA.size);
                out.add<size_t>(GLIS_INTERNAL_SHARED_MEMORY_TEXTURE_DATA.reference_count);
                if (IPC == IPC_MODE.socket) {
                    if (SERVER_LOG_TRANSFER_INFO)
                        LOG_INFO_SERVER("%ssending id %d, sise: %zu",
                                        CompositorMain.server.TAG,
                                        GLIS_INTERNAL_SHARED_MEMORY_TEXTURE_DATA.fd,
                                        GLIS_INTERNAL_SHARED_MEMORY_TEXTURE_DATA.size);
                } else {
                    LOG_INFO("sending id %d, sise: %zu",
                             GLIS_INTERNAL_SHARED_MEMORY_TEXTURE_DATA.fd,
                             GLIS_INTERNAL_SHARED_MEMORY_TEXTURE_DATA.size);
                }
                CompositorMain.server.socket_put_fd(GLIS_INTERNAL_SHARED_MEMORY_TEXTURE_DATA.fd);
                CompositorMain.server.socket_put_serial(out);
                LOG_INFO_SERVER("GLIS_INTERNAL_SHARED_MEMORY_PARAMETER.reference_count = %zu",
                                GLIS_INTERNAL_SHARED_MEMORY_TEXTURE_DATA.reference_count);
                double end = now_ms();
                LOG_INFO("send texture file descriptor in %G milliseconds", end - start);
            } else if (command == GLIS_SERVER_COMMANDS.shm_params) {
                double start = now_ms();
                assert(ashmem_valid(GLIS_INTERNAL_SHARED_MEMORY_PARAMETER.fd));
                LOG_INFO_SERVER("GLIS_INTERNAL_SHARED_MEMORY_PARAMETER.reference_count = %zu",
                                GLIS_INTERNAL_SHARED_MEMORY_PARAMETER.reference_count);
                GLIS_shared_memory_increase_reference(GLIS_INTERNAL_SHARED_MEMORY_PARAMETER);
                out.add<size_t>(GLIS_INTERNAL_SHARED_MEMORY_PARAMETER.size);
                out.add<size_t>(GLIS_INTERNAL_SHARED_MEMORY_PARAMETER.reference_count);
                if (IPC == IPC_MODE.socket) {
                    if (SERVER_LOG_TRANSFER_INFO)
                        LOG_INFO_SERVER("%ssending id %d, sise: %zu",
                                        CompositorMain.server.TAG,
                                        GLIS_INTERNAL_SHARED_MEMORY_PARAMETER.fd,
                                        GLIS_INTERNAL_SHARED_MEMORY_PARAMETER.size);
                } else {
                    LOG_INFO("sending id %d, sise: %zu", GLIS_INTERNAL_SHARED_MEMORY_PARAMETER.fd,
                             GLIS_INTERNAL_SHARED_MEMORY_PARAMETER.size);
                }
                CompositorMain.server.socket_put_fd(GLIS_INTERNAL_SHARED_MEMORY_PARAMETER.fd);
                CompositorMain.server.socket_put_serial(out);
                LOG_INFO_SERVER("GLIS_INTERNAL_SHARED_MEMORY_PARAMETER.reference_count = %zu",
                                GLIS_INTERNAL_SHARED_MEMORY_PARAMETER.reference_count);
                double end = now_ms();
                LOG_INFO("send parameters file descriptor in %G milliseconds", end - start);
            } else if (command == GLIS_SERVER_COMMANDS.new_connection) {
                struct pa {
                    size_t table_id;

                    class GLIS_shared_memory *GLIS_INTERNAL_SHARED_MEMORY_PARAMETER;
                } p;
                p.GLIS_INTERNAL_SHARED_MEMORY_PARAMETER = &GLIS_INTERNAL_SHARED_MEMORY_PARAMETER;
                char *s = SERVER_allocate_new_server(SERVER_START_REPLY_MANUALLY, p.table_id);
                long t; // unused
                int e = pthread_create(&t, nullptr, KEEP_ALIVE_MAIN_NOTIFIER, &p);
                if (e != 0)
                    LOG_ERROR("pthread_create(): errno: %d (%s) | return: %d (%s)", errno,
                              strerror(errno), e,
                              strerror(e));
                else
                    LOG_INFO("KEEP_ALIVE_MAIN_NOTIFIER thread successfully started");
                out.add_pointer<char>(s, 107);
                CompositorMain.server.socket_put_serial(out);
            }
            if (IPC == IPC_MODE.socket) assert(CompositorMain.server.socket_unaccept());
            LOG_INFO("CLIENT has uploaded");
            goto draw;
            draw:
            if (redraw) {
                double start = now_ms();
                LOG_INFO("rendering");
                GLIS_error_to_string_exec_GL(glClearColor(0.0F, 0.0F, 1.0F, 1.0F));
                GLIS_error_to_string_exec_GL(glClear(GL_COLOR_BUFFER_BIT));
                int page = 1;
                size_t index = 0;
                size_t page_size = CompositorMain.KERNEL.table->page_size;
                int drawn = 0;
                double startK = now_ms();
                for (; page <= CompositorMain.KERNEL.table->Page.count(); page++) {
                    index = ((page_size * page) - page_size);
                    for (; index < page_size * page; index++)
                        if (CompositorMain.KERNEL.table->table[index] != nullptr) {
                            struct Client_Window *CW = static_cast<Client_Window *>(CompositorMain.KERNEL.table->table[index]->resource);
                            struct Client_Window *CWT = static_cast<Client_Window *>(CompositorMain.KERNEL.table->table[0]->resource);
                            double startR = now_ms();
                            GLIS_draw_rectangle<GLint>(GL_TEXTURE0,
                                                       CWT->TEXTURE,
                                                       0, CW->x,
                                                       CW->y, CW->w, CW->h,
                                                       CompositorMain.width, CompositorMain.height);
                            drawn++;
                            double endR = now_ms();
                        }
                }
                double endK = now_ms();
                LOG_INFO("Drawn %d %s in %G milliseconds", drawn,
                         drawn == 1 ? "window" : "windows", endK - startK);
                GLIS_Sync_GPU();
                GLIS_error_to_string_exec_EGL(
                    eglSwapBuffers(CompositorMain.display, CompositorMain.surface));
                GLIS_Sync_GPU();
                double end = now_ms();
                LOG_INFO("rendered in %G milliseconds", end - start);
                LOG_INFO("since loop start: %G milliseconds", end - loop_start);
                LOG_INFO("since start: %G milliseconds", end - program_start);
            }
        }
        SYNC_STATE = STATE.response_shutting_down;
        LOG_INFO("shutting down");

        // clean up
        LOG_INFO("Cleaning up");
        GLIS_error_to_string_exec_GL(glDeleteProgram(shaderProgram));
        GLIS_error_to_string_exec_GL(glDeleteShader(fragmentShader));
        GLIS_error_to_string_exec_GL(glDeleteShader(vertexShader));
        GLIS_destroy_GLIS(CompositorMain);
        LOG_INFO("Destroyed main Compositor GLIS");
        LOG_INFO("Cleaned up");
        LOG_INFO("shut down");
        SYNC_STATE = STATE.response_shutdown;
    } else LOG_ERROR("failed to initialize main Compositor");
    return 0;
}

void * COMPOSITORMAIN(void * arg) {
    int * ret = new int;
    LOG_INFO("calling COMPOSITORMAIN__()");
    *ret = COMPOSITORMAIN__();
    return ret;
}

long COMPOSITORMAIN_threadId;
extern "C" JNIEXPORT void JNICALL Java_glnative_example_NativeView_nativeOnStart(JNIEnv* jenv,
                                                                                 jclass type,
                                                                                 jstring
                                                                                 ExecutablesDir) {
    jboolean val;
    const char *a = jenv->GetStringUTFChars(ExecutablesDir, &val);
    size_t len = (strlen(a) + 1) * sizeof(char);
    executableDir = static_cast<char *>(malloc(len));
    memcpy(executableDir, a, len);
    jenv->ReleaseStringUTFChars(ExecutablesDir, a);
    LOG_INFO("starting main Compositor");
    int e = pthread_create(&COMPOSITORMAIN_threadId, nullptr, COMPOSITORMAIN, nullptr);
    if (e != 0)
        LOG_ERROR("pthread_create(): errno: %d (%s) | return: %d (%s)", errno, strerror(errno), e,
                  strerror(e));
    else
        LOG_INFO("Compositor thread successfully started");
}

extern "C" JNIEXPORT void JNICALL Java_glnative_example_NativeView_nativeOnStop(JNIEnv* jenv,
                                                                                 jclass type) {
    LOG_INFO("waiting for main Compositor to stop");
    int * ret;
    int e = pthread_join(COMPOSITORMAIN_threadId, reinterpret_cast<void **>(&ret));
    if (e != 0)
        LOG_ERROR("pthread_join(): errno: %d (%s) | return: %d (%s)", errno, strerror(errno), e,
                  strerror(e));
    else
        LOG_INFO("main Compositor has stopped: return code: %d", *ret);
    delete ret;
}