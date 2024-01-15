/**
 * @file sim.hpp
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

#pragma once

#include <GL/gl.h>

#include <stdexcept>

#include "imgui.h"

typedef unsigned char uchar;

constexpr GLuint palette[] = {0x5D432CFF, 0xEFEF80FF, 0xDFDF80FF, 0xCFCF80FF,
                              0xBFBF80FF, 0xAFAF80FF, 0x9F9F80FF, 0x8F8F80FF,
                              0x7F7F80FF, 0x6F6F80FF, 0x5F5F80FF, 0x0000FFFF};

constexpr GLuint wall_color = 0x4D5D53FF;     // Seal Gray
constexpr GLuint floor_color = 0xFFFFFFFF;    // White
constexpr GLuint escape_color = 0x0000FFFF;   // Blue
constexpr GLuint emitter_color = 0xFF0000FF;  // Red

enum Dir { North, South, West, East };
constexpr Dir directions[4] = {Dir::North, Dir::South, Dir::West, Dir::East};

class Sim {
   private:
    struct Cell {
        enum Type { Wall, Floor, Emitter, Escape };
        Cell::Type type = Type::Floor;
        char cost = 0;
        uint row = 0, col = 0;
        float omega_in = 0.f, omega_out = 0.f;
        float density = 0.f, outtake = 0.f, intake = 0.f;
    };

    class Board {
       private:
        uint width;
        uint height;
        GLuint* pixmap;
        Cell* cells;

       public:
        Board(uint width, uint height, const char* const layout) {
            this->width = width;
            this->height = height;
            this->pixmap = (GLuint*)malloc(width * height * sizeof(GLuint));
            this->cells = (Cell*)malloc(width * height * sizeof(Cell));
            for (uint row = 0; row < this->height; row++) {
                for (uint col = 0; col < this->width; col++) {
                    size_t idx = row * this->width + col;
                    this->cells[idx].row = row;
                    this->cells[idx].col = col;
                    /* \0123456789:
                     * Add -0x30 -> Obtain values in range [-1, 10], where 0-9
                     * is floor height, -1 is a wall, and 10 is an opening. Add
                     * 1 to obtain a valid palette index. */
                    // printf("%c", layout[idx]);
                    int cost = layout[idx] - 0x30;
                    if (cost <= -1) {
                        cost = -1;
                        this->cells[idx].type = Cell::Type::Wall;
                        this->pixmap[idx] = wall_color;
                    } else if (cost >= 10) {
                        cost = 10;
                        this->cells[idx].type = Cell::Type::Escape;
                        this->pixmap[idx] = escape_color;
                    } else {
                        this->cells[idx].type = Cell::Type::Floor;
                        this->pixmap[idx] = floor_color;
                    }
                    this->cells[idx].cost = cost;
                    this->cells[idx].density = 0;
                    // this->pixmap[idx] = palette[cost + 1];
                }
                // printf("\n");
            }
        };
        ~Board() {
            free(this->pixmap);
            free(this->cells);
        }
        uint getWidth() { return this->width; }
        uint getHeight() { return this->height; }
        void toTexture(GLuint* texture) {
            GLuint temp;
            glGenTextures(1, &temp);
            glBindTexture(GL_TEXTURE_2D, temp);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->width, this->height,
                         0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, this->pixmap);
            if (temp == 0)
                throw std::runtime_error("Failed to create texture.");
            *texture = temp;
        }
        Cell* cellAt(uint row, uint col) {
            if (row < 0 || row >= this->height || col < 0 ||
                col >= this->width) {
                return nullptr;
            }
            return &this->cells[row * this->width + col];
        }
        Cell* cellAt(Dir dir, uint row, uint col) {
            switch (dir) {
                case Dir::North:
                    return this->cellAt(row - 1, col);
                case Dir::South:
                    return this->cellAt(row + 1, col);
                case Dir::West:
                    return this->cellAt(row, col - 1);
                case Dir::East:
                    return this->cellAt(row, col + 1);
            }
        }
        void writePixel(uint row, uint col, GLubyte r, GLubyte g, GLubyte b) {
            GLuint rgba = 0x00;
            rgba |= (r << 24);
            rgba |= (g << 16);
            rgba |= (b << 8);
            rgba |= 0xFF;  // a
            this->pixmap[row * this->width + col] = rgba;
        }
        void writePixel(uint row, uint col, GLuint rgba) {
            this->pixmap[row * this->width + col] = rgba;
        }
    };
    enum State { Stop, Run, Step };
    State state = Sim::State::Stop;
    unsigned int ticks = 0;

   public:
    int tick_rate = 1;
    float emitter_rate = 1.f;
    float escape_rate = 1.f;
    bool use_precalc_weights = false;
    Board* board;
    Sim(uint board_width, uint board_height, const char* const layout,
        uint emitter_row, uint emitter_col) {
        this->board = new Board(board_width, board_height, layout);
        {
            // Emitter placement
            Cell* emitter = this->board->cellAt(emitter_row, emitter_col);
            if (emitter == nullptr) {
                throw std::runtime_error("Emitter coordinates out of bounds.");
            } else if (emitter->cost < 0 || emitter->cost > 9) {
                delete this->board;
                throw std::runtime_error("Emitter not on floor tile.");
            }
            emitter->type = Cell::Type::Emitter;
            emitter->density = 1.f;
        }
        this->board->writePixel(emitter_row, emitter_col, emitter_color);
        /* Calculate how many inputs and outputs a cell has. This in turn
         * will affect the propagation rate. */
        for (uint row = 0; row < this->board->getHeight(); row++) {
            for (uint col = 0; col < this->board->getWidth(); col++) {
                ushort ins = 0, outs = 0;
                for (auto dir : directions) {
                    Cell* adj = this->board->cellAt(dir, row, col);
                    if (adj == nullptr) {
                        continue;
                    } else {
                        switch (adj->type) {
                            case Cell::Wall:
                                break;
                            case Cell::Floor:
                                ins++;
                                outs++;
                                break;
                            case Cell::Emitter:
                                ins++;
                                break;
                            case Cell::Escape:
                                outs++;
                                break;
                        }
                    }
                }
                Cell* cur = this->board->cellAt(row, col);
                if (ins == 0)
                    cur->omega_in = .0f;
                else
                    cur->omega_in = 1.f / (float)ins;
                if (outs == 0)
                    cur->omega_out = .0f;
                else
                    cur->omega_out = 1.f / (float)outs;
                /* printf("%d,%d] iw:%f, ow:%f\n", row, col, cur->in_w,
                       cur->out_w); */
            }
        }
    }
    ~Sim() { delete this->board; }
    void start() { state = Sim::State::Run; }
    void stop() { state = Sim::State::Stop; }
    bool isRunning() {
        if (this->state == Sim::State::Run) return true;
        return false;
    }
    unsigned int getTicks() { return this->ticks; }
    void cycle() {
        static int frame_skip_counter = this->tick_rate;
        static float emitter_rate = this->emitter_rate;
        static float escape_rate = this->escape_rate;
        static bool use_precalc_weights = this->use_precalc_weights;
        /* Check if we're running: if so, decrement the Frame Skip Counter until
         * we reach zero, then run the update cycle. We're assuming this is
         * executed once per frame. */
        if (this->state == Sim::State::Stop ||
            (this->state == Sim::State::Run && (--frame_skip_counter) != 0))
            return;
        for (uint row = 0; row < this->board->getHeight(); row++) {
            for (uint col = 0; col < this->board->getWidth(); col++) {
                Cell* cur = this->board->cellAt(row, col);
                if (cur == nullptr) {
                    throw std::runtime_error(
                        "Unexpected missing Cell in valid location.");
                }
                static uchar l;
                static float cur_omega_out, cur_omega_in, adj_omega_out,
                    adj_omega_in;
                if (use_precalc_weights) {
                    // Use precalculated weights
                    cur_omega_out = cur->omega_out;
                    cur_omega_in = cur->omega_in;
                } else {
                    // Use 1/4 weights independently from the number of
                    // neighbors
                    cur_omega_out = .25f;
                    cur_omega_in = .25f;
                }
                cur->intake = .0f;
                cur->outtake = .0f;
                switch (cur->type) {
                    case Cell::Wall:
                        break;
                    case Cell::Floor:
                        for (auto dir : directions) {
                            Cell* adj = this->board->cellAt(dir, row, col);
                            if (adj == nullptr) continue;
                            if (use_precalc_weights) {
                                adj_omega_out = adj->omega_out;
                                adj_omega_in = adj->omega_in;
                            } else {
                                adj_omega_out = .25f;
                                adj_omega_in = .25f;
                            }
                            adj->intake = .0f;
                            adj->outtake = .0f;
                            switch (adj->type) {
                                case Cell::Wall:
                                    break;
                                case Cell::Floor:
                                    adj->intake = std::min(
                                        cur_omega_out * cur->density,
                                        adj_omega_in * (1 - adj->density));
                                    adj->outtake = std::min(
                                        adj_omega_out * adj->density,
                                        cur_omega_in * (1 - cur->density));
                                    break;
                                case Cell::Emitter:
                                    adj->outtake =
                                        emitter_rate *
                                        std::min(
                                            adj_omega_out * adj->density,
                                            cur_omega_in * (1 - cur->density));
                                    break;
                                case Cell::Escape:
                                    adj->intake = escape_rate * cur_omega_out *
                                                  cur->density;
                                    break;
                            }
                            cur->intake += adj->outtake;
                            cur->outtake += adj->intake;
                        }
                        cur->density += cur->intake - cur->outtake;
                        l = 255 - (255 * cur->density);
                        /*printf("s:%f, delta:%f\n", cur->smoke,
                               cur->intake - cur->outtake);*/
                        this->board->writePixel(row, col, l, l, l);
                        break;
                    case Cell::Emitter:
                        l = 255 * emitter_rate;
                        this->board->writePixel(row, col, l, 255 - l, 255 - l);
                        break;
                    case Cell::Escape:
                        l = 255 * escape_rate;
                        this->board->writePixel(row, col, 255 - l, 255 - l, l);
                        break;
                }
            }
        }
        this->ticks++;
        // Avoid resetting the FSC if we're single-stepping.
        if (this->state == Sim::State::Run)
            frame_skip_counter = this->tick_rate;
        emitter_rate = this->emitter_rate;
        escape_rate = this->escape_rate;
        use_precalc_weights = this->use_precalc_weights;
    }
    void step() {
        this->state = Sim::State::Step;
        this->cycle();
        this->state = Sim::State::Stop;
    }
};
