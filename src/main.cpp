/**
 * @file main.cpp
 * @author Jacopo Maltagliati
 * @brief Smoke Propagation Simulation using Cellular Automatas
 *
 * @copyright Copyright (c) 2023-2024 Jacopo Maltagliati.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl2.h"
#include "imgui.h"

// Project Includes
#include "sim.hpp"

struct Layout {
    uint rows, cols;
    char* data;
    Layout(const std::string& path) {
        std::ifstream ifs;
        ifs.open(path, std::ifstream::in);
        if (!ifs.is_open()) {
            throw std::runtime_error(
                "An I/O error occurred while opening the file for reading.");
        }
        std::string tmp, line;
        uint rows = 0, cols = 0, cols_prev = 0;
        while (std::getline(ifs, line)) {
            rows++;
            cols = 0;
            for (auto c : line) {
                if (c < '/' || c > ':') {
                    ifs.close();
                    std::stringstream ss;
                    ss << "Invalid character \'" << c << "\' (" << (int)c
                       << ") detected at " << rows << ", " << cols << ".";
                    throw std::runtime_error(ss.str().c_str());
                }
                cols++;
                tmp.push_back(c);
            }
            if (cols != cols_prev && rows > 1) {
                ifs.close();
                throw std::runtime_error(
                    "Each row must have the same number of columns.");
            }
            cols_prev = cols;
        }
        ifs.close();
        if (rows <= 0 || cols <= 0) {
            throw std::runtime_error("The layout must not be empty.");
        }
        if (rows > 512 || cols > 512) {
            throw std::runtime_error(
                "The layout must not exceed a size of 512x512 cells.");
        }
        this->rows = rows;
        this->cols = cols;
        this->data = (char*)malloc(rows * cols);
        std::memcpy(this->data, tmp.c_str(), rows * cols);
    }
    ~Layout() { free(this->data); }
};

constexpr ImVec4 __ui_clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
constexpr int __ui_board_zoom_default = 10;

int main(void) {
    GLuint board_texture_id = 0;
    SDL_GLContext main_gl_context;
    SDL_Window* main_window;
    SDL_WindowFlags main_window_flags;
    Sim* simulation = nullptr;
    bool ui_done = false;
    int ui_emitter_pos[2] = {0, 0};
    char ui_layout_path[512] = "../layouts/default.txt";
    std::string ui_status_msg = "Ready.";
    int ui_board_zoom = __ui_board_zoom_default;
    int ui_breakpoint = 0;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) !=
        0) {
        throw std::runtime_error(SDL_GetError());
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    main_window_flags =
        (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                          SDL_WINDOW_ALLOW_HIGHDPI);
    main_window =
        SDL_CreateWindow("Smoke Propagation Simulator", SDL_WINDOWPOS_CENTERED,
                         SDL_WINDOWPOS_CENTERED, 1280, 720, main_window_flags);
    main_gl_context = SDL_GL_CreateContext(main_window);
    SDL_GL_MakeCurrent(main_window, main_gl_context);
    SDL_GL_SetSwapInterval(1);  // Enable vsync
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForOpenGL(main_window, main_gl_context);
    ImGui_ImplOpenGL3_Init();

    while (!ui_done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) ui_done = true;
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(main_window))
                ui_done = true;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(main_window);
        ImGui::NewFrame();

        {
            ImGui::SetNextWindowSize(ImVec2(430, 240));
            ImGui::Begin(
                "Set-up Window", nullptr,
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
            ImGui::SetNextWindowSize(ImVec2(640, 480));
            ImGui::InputText("Layout File Path", ui_layout_path,
                             IM_ARRAYSIZE(ui_layout_path));
            ImGui::InputInt2("Emitter Coordinates", &ui_emitter_pos[0]);

            if (ImGui::Button("New Simulation")) {
                delete simulation;
                try {
                    auto layout = new Layout(ui_layout_path);
                    simulation =
                        new Sim(layout->cols, layout->rows, layout->data,
                                ui_emitter_pos[0], ui_emitter_pos[1]);
                    delete layout;
                    // Draw first frame regardless of simulation status
                    simulation->board->toTexture(&board_texture_id);
                    ui_status_msg = "Simulation initialized.";
                } catch (std::runtime_error e) {
                    ui_status_msg = e.what();
                    simulation = nullptr;
                }
            }

            ImGui::Separator();
            ImGui::TextWrapped("%s", ui_status_msg.c_str());

            if (simulation != nullptr) {
                ImGui::Begin("Simulation Window", nullptr,
                             ImGuiWindowFlags_NoCollapse |
                                 ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_HorizontalScrollbar);
                // Simulation Controls
                if (simulation->isRunning()) {
                    if (ImGui::Button("Stop")) {
                        ui_status_msg = "Simulation stopped.";
                        simulation->stop();
                    }
                } else {
                    if (ImGui::Button("Start")) {
                        ui_status_msg = "Simulation running.";
                        simulation->start();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Step")) {
                        simulation->step();
                    }
                }
                ImGui::SameLine();
                ImGui::SliderInt("Tick Rate", &simulation->tick_rate, 1, 50);
                if (ImGui::CollapsingHeader("Advanced")) {
                    ImGui::InputInt("Breakpoint", &ui_breakpoint);
                    ImGui::SliderFloat("Emission Rate",
                                       &simulation->emitter_rate, .0f, 1.0f);
                    ImGui::SliderFloat("Escape Rate", &simulation->escape_rate,
                                       .0f, 1.0f);
                    ImGui::Checkbox("Use Precalculated Weights",
                                    &simulation->use_precalc_weights);
                }
                ImGui::Separator();
                // Simulation Cycle
                try {
                    simulation->cycle();
                } catch (std::runtime_error e) {
                    simulation->stop();
                    ui_status_msg = e.what();
                }

                if (simulation->isRunning()) {
                    // Update Display
                    simulation->board->toTexture(&board_texture_id);
                    // Check if we've reached a breakpoint
                    if (ui_breakpoint > 0) {
                        ui_breakpoint--;
                        if (ui_breakpoint == 0) {
                            ui_status_msg = "Breakpoint reached.";
                            simulation->stop();
                        }
                    }
                }
                if (ui_breakpoint < 0) {
                    ui_breakpoint = 0;
                }

                ImGui::Image(
                    (void*)(intptr_t)board_texture_id,
                    ImVec2(simulation->board->getWidth() * ui_board_zoom,
                           simulation->board->getHeight() * ui_board_zoom),
                    ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1),
                    ImVec4(0.302, 0.365, 0.325, 1));
                ImGui::Text("Ticks: %d", simulation->getTicks());
                ImGui::Separator();
                // UI Controls
                ImGui::SliderInt("##zoom", &ui_board_zoom, 1, 20);
                ImGui::SameLine();
                if (ImGui::Button("10x")) {
                    ui_board_zoom = __ui_board_zoom_default;
                }
                ImGui::SameLine();
                ImGui::Text("Zoom");
                ImGui::End();
            }

            if (ImGui::CollapsingHeader("Debug Information")) {
                ImGui::Text("ImGui v%s (%d)", IMGUI_VERSION, IMGUI_VERSION_NUM);
                ImGui::Text("%.3f ms/frame (%.1f FPS)",
                            1000.0f / ImGui::GetIO().Framerate,
                            ImGui::GetIO().Framerate);
            }
            if (ImGui::CollapsingHeader("About")) {
                ImGui::TextWrapped(
                    "Smoke Propagation Simulator \"smokey\"\n"
                    "Copyright (c) 2023-2024 Jacopo Maltagliati\n"
                    "Released under the Apache-2.0 license.\n\n"
                    "Dear ImGui v1.90\n"
                    "Copyright (c) 2014-2022 Omar Cornut\n"
                    "Released under the MIT license.\n");
            }
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x,
                   (int)ImGui::GetIO().DisplaySize.y);
        glClearColor(__ui_clear_color.x * __ui_clear_color.w,
                     __ui_clear_color.y * __ui_clear_color.w,
                     __ui_clear_color.z * __ui_clear_color.w,
                     __ui_clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(main_window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(main_gl_context);
    SDL_DestroyWindow(main_window);
    SDL_Quit();
    exit(EXIT_SUCCESS);
}
