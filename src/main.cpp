#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <algorithm>
#include <map>
#include <functional>
#include <vector>
#include <iomanip>

#include "world.h"
#include "entity/player.h"
#include "options.h"
#include "renderer/block_type.h"
#include "physics/hit.h"
#include "models/all_models.h"
#include "text_renderer.h"
#include "audio.h"
#include "renderer/post_processor.h"
#include "inventory_ui.h"
#include "renderer/ui_quad_renderer.h"
#include "inventory.h"

// --- КОНСТАНТЫ ---
const std::string GAME_TITLE = "MC-CPP";

// Глобальные переменные
int SCR_WIDTH = 1366;
int SCR_HEIGHT = 768;
World* world_ptr = nullptr;
Player* player_ptr = nullptr;
TextRenderer* text_renderer = nullptr;
PostProcessor* post_processor = nullptr;
InventoryUI* inventory_ui = nullptr;
UiQuadRenderer* quad_renderer = nullptr;

bool mouse_captured = false;
bool is_fullscreen = false;
int windowed_x = 100, windowed_y = 100, windowed_w = 1366, windowed_h = 768;
bool show_f3 = false;
bool in_menu = true;

int get_selected_block_id() {
    if (!player_ptr) return 0;
    const InventorySlot& slot = player_ptr->inventory.slot(player_ptr->hotbar_selected);
    return slot.id;
}

bool consume_selected_block() {
    if (!player_ptr) return false;
    InventorySlot& slot = player_ptr->inventory.slot(player_ptr->hotbar_selected);
    if (slot.empty()) return false;
    slot.count -= 1;
    if (slot.count <= 0) slot.clear();
    return true;
}

// --- CROSSHAIR (INVERTED & THICK) ---
GLuint crosshairVAO, crosshairVBO;
GLuint uiShaderProgram;

// --- HEALTH BAR (TRIANGLES) ---
GLuint triangleVAO = 0, triangleVBO = 0;
Shader* uiTriangleShader = nullptr;

// Шейдер для курсора
const char* crosshairVertexShader = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
void main() {
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
}
)";
const char* crosshairFragmentShader = R"(
#version 330 core
out vec4 FragColor;
void main() {
    FragColor = vec4(1.0, 1.0, 1.0, 1.0); // Белый цвет
}
)";

void init_crosshair() {
    unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &crosshairVertexShader, NULL);
    glCompileShader(vs);
    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &crosshairFragmentShader, NULL);
    glCompileShader(fs);
    uiShaderProgram = glCreateProgram();
    glAttachShader(uiShaderProgram, vs);
    glAttachShader(uiShaderProgram, fs);
    glLinkProgram(uiShaderProgram);
    glDeleteShader(vs);
    glDeleteShader(fs);

    glGenVertexArrays(1, &crosshairVAO);
    glGenBuffers(1, &crosshairVBO);
}

void update_crosshair_mesh(int w, int h) {
    float ratio = (float)w / (float)h;

    // Настройки размера (жирный курсор)
    float length = 0.025f;
    float thickness = 0.003f; // Толщина

    float lX = length / ratio;
    float lY = length;
    float tX = thickness / ratio;
    float tY = thickness;

    float vertices[] = {
        // Horizontal
        -lX, -tY,  lX, -tY,  -lX, tY,
        -lX,  tY,  lX, -tY,   lX, tY,
        // Vertical Top
        -tX,  tY,  tX,  tY,  -tX, lY,
        -tX,  lY,  tX,  tY,   tX, lY,
        // Vertical Bottom
        -tX, -lY,  tX, -lY,  -tX, -tY,
        -tX, -tY,  tX, -lY,   tX, -tY
    };

    glBindVertexArray(crosshairVAO);
    glBindBuffer(GL_ARRAY_BUFFER, crosshairVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
}

void draw_crosshair() {
    glUseProgram(uiShaderProgram);
    glBindVertexArray(crosshairVAO);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO); // Инверсия
    glDrawArrays(GL_TRIANGLES, 0, 18);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
}

// --- HEALTH BAR (TRIANGLES) ---
void init_ui_resources() {
    uiTriangleShader = new Shader("assets/shaders/ui/triangle_vert.glsl", "assets/shaders/ui/triangle_frag.glsl");
    if (!uiTriangleShader->valid()) {
        std::cout << "Failed to load triangle UI shader." << std::endl;
        delete uiTriangleShader;
        uiTriangleShader = nullptr;
        return;
    }

    float quadVertices[] = {
        // Pos        // UV
        0.0f, 1.0f,   0.0f, 1.0f,
        0.0f, 0.0f,   0.0f, 0.0f,
        1.0f, 0.0f,   1.0f, 0.0f,

        0.0f, 1.0f,   0.0f, 1.0f,
        1.0f, 0.0f,   1.0f, 0.0f,
        1.0f, 1.0f,   1.0f, 1.0f
    };

    glGenVertexArrays(1, &triangleVAO);
    glGenBuffers(1, &triangleVBO);
    glBindVertexArray(triangleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, triangleVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
}

void draw_health_bar() {
    if (!uiTriangleShader || !uiTriangleShader->valid() || !player_ptr || triangleVAO == 0) return;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    uiTriangleShader->use();
    uiTriangleShader->setFloat(uiTriangleShader->find_uniform("u_Time"), static_cast<float>(glfwGetTime()));

    int offsetLoc = uiTriangleShader->find_uniform("u_Offset");
    int scaleLoc = uiTriangleShader->find_uniform("u_Scale");
    int fullnessLoc = uiTriangleShader->find_uniform("u_Fullness");

    glBindVertexArray(triangleVAO);

    float startX = -0.95f;
    float startY = -0.90f;
    float size = 0.07f;
    float spacing = size * 1.15f;
    float aspect = static_cast<float>(SCR_WIDTH) / static_cast<float>(SCR_HEIGHT);
    glm::vec2 scaleVec(size / aspect, size);

    int maxHearts = static_cast<int>(player_ptr->max_health);
    float currentHealth = player_ptr->health;

    for (int i = 0; i < maxHearts; i++) {
        float offsetX = startX + (i * spacing / aspect);
        uiTriangleShader->setVec2(offsetLoc, glm::vec2(offsetX, startY));
        uiTriangleShader->setVec2(scaleLoc, scaleVec);

        float status = (currentHealth >= static_cast<float>(i + 1)) ? 1.0f : 0.0f;
        uiTriangleShader->setFloat(fullnessLoc, status);

        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    glBindVertexArray(0);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}

// --- F3 DEBUG INFO ---
void draw_f3_screen(float fps) {
    if (!show_f3 || !text_renderer) return;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    glm::ivec3 cpos = world_ptr->get_chunk_pos(player_ptr->position);
    glm::ivec3 lpos = world_ptr->get_local_pos(player_ptr->position);
    glm::ivec3 ipos = glm::round(player_ptr->position);

    // Заголовок (Жирный)
    std::stringstream ss;
    ss << GAME_TITLE << " (" << (int)fps << " fps)";
    std::string titleLine = ss.str();

    // Остальные строки
    std::vector<std::string> lines;
    ss.str("");

    ss << "C: " << world_ptr->visible_chunks.size() << "/" << world_ptr->chunks.size()
    << ". F: 0. O: 0. E: 0";
    lines.push_back(ss.str()); ss.str("");

    ss << "E: 0/0. B: 0. I: 0";
    lines.push_back(ss.str()); ss.str("");

    lines.push_back("");

    ss << std::fixed << std::setprecision(3)
    << "XYZ: " << player_ptr->position.x << " / " << player_ptr->position.y << " / " << player_ptr->position.z;
    lines.push_back(ss.str()); ss.str("");

    ss << "Block: " << ipos.x << " " << ipos.y << " " << ipos.z;
    lines.push_back(ss.str()); ss.str("");

    ss << "Chunk: " << lpos.x << " " << lpos.y << " " << lpos.z << " in " << cpos.x << " " << cpos.y << " " << cpos.z;
    lines.push_back(ss.str()); ss.str("");

    int bl = world_ptr->get_light(ipos);
    int sl = world_ptr->get_skylight(ipos);
    ss << "Light: " << std::max(bl, sl) << " (" << sl << " sky, " << bl << " block)";
    lines.push_back(ss.str()); ss.str("");

    ss << "Input: captured=" << (mouse_captured ? "1" : "0")
       << " rot=(" << player_ptr->rotation.x << "," << player_ptr->rotation.y << ")"
       << " input=(" << player_ptr->input.x << "," << player_ptr->input.y << "," << player_ptr->input.z << ")";
    lines.push_back(ss.str()); ss.str("");

    float y = 10.0f;
    float lineHeight = 20.0f;
    float scale = 1.0f;

    // Рисуем заголовок (Жирный = Рисуем дважды со смещением 1px)
    // Тень
    text_renderer->RenderText(titleLine, 6.0f, y + 2.0f, scale, glm::vec3(0.24f));
    // Жирность (смещение +1px по X)
    text_renderer->RenderText(titleLine, 5.0f, y, scale, glm::vec3(1.0f));
    // Основной
    text_renderer->RenderText(titleLine, 4.0f, y, scale, glm::vec3(1.0f));
    y += lineHeight;

    for (const auto& line : lines) {
        text_renderer->RenderText(line, 6.0f, y + 2.0f, scale, glm::vec3(0.24f)); // Тень
        text_renderer->RenderText(line, 4.0f, y, scale, glm::vec3(0.87f)); // Светло-серый
        y += lineHeight;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

// --- HELPERS & PARSERS ---
GLFWmonitor* get_current_monitor(GLFWwindow *window) {
    int nmonitors, i; int wx, wy, ww, wh; int mx, my, mw, mh; int overlap, bestoverlap;
    GLFWmonitor *bestmonitor; GLFWmonitor **monitors; const GLFWvidmode *mode;
    bestoverlap = 0; bestmonitor = NULL;
    glfwGetWindowPos(window, &wx, &wy); glfwGetWindowSize(window, &ww, &wh);
    monitors = glfwGetMonitors(&nmonitors);
    for (i = 0; i < nmonitors; i++) {
        mode = glfwGetVideoMode(monitors[i]);
        glfwGetMonitorPos(monitors[i], &mx, &my);
        mw = mode->width; mh = mode->height;
        overlap = std::max(0, std::min(wx + ww, mx + mw) - std::max(wx, mx)) * std::max(0, std::min(wy + wh, my + mh) - std::max(wy, my));
        if (bestoverlap < overlap) { bestoverlap = overlap; bestmonitor = monitors[i]; }
    }
    return bestmonitor;
}
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n"); if (std::string::npos == first) return "";
    size_t last = str.find_last_not_of(" \t\r\n"); return str.substr(first, (last - first + 1));
}
std::string remove_quotes(std::string str) {
    str = trim(str); if (str.size() >= 2 && str.front() == '"' && str.back() == '"') return str.substr(1, str.size() - 2); return str;
}
void load_blocks(World& world, TextureManager& tm) {
    std::map<std::string, std::function<ModelData()>> model_registry = {
        {"models.cube", Models::Cube::get_model},
        {"models.plant", Models::Plant::get_model},
        {"models.liquid", Models::Liquid::get_model},
        {"models.leaves", Models::Leaves::get_model},
        {"models.glass", Models::Glass::get_model},
        {"models.slab", Models::Slab::get_model},
        {"models.torch", Models::Torch::get_model},
        {"models.stairs", Models::Stairs::get_model},
        {"models.flat", Models::Flat::get_model},
        {"models.soil", Models::Soil::get_model},
        {"models.crop", Models::Crop::get_model},
        {"models.cactus", Models::Cactus::get_model},
        // Привязываем новые модели
        {"models.ladder", Models::Ladder::get_model},
        {"models.door", Models::Door::get_model},
        // Остальные фоллбэки
        {"models.fire", Models::Plant::get_model},
        {"models.sign", Models::Plant::get_model},
        {"models.lever", Models::Plant::get_model},
        {"models.pressure_plate", Models::Slab::get_model},
        {"models.button", Models::Slab::get_model},
        {"models.snow", Models::Slab::get_model},
        {"models.tinted_glass", Models::Glass::get_model},
        {"models.sign_post", Models::Plant::get_model}
    };

    std::ifstream file("data/blocks.mccpp"); std::string line; world.block_types.push_back(nullptr);
    while (std::getline(file, line)) {
        line = trim(line); if (line.empty() || line[0] == '#' || line[0] == '\n') continue;
        size_t colon_pos = line.find(':'); if (colon_pos == std::string::npos) continue;
        int number = std::stoi(trim(line.substr(0, colon_pos)));
        std::string props_str = trim(line.substr(colon_pos + 1));
        std::string name = "Unknown"; ModelData model = Models::Cube::get_model();
        std::map<std::string, std::string> texture = {{"all", "unknown"}};
        std::stringstream ss(props_str); std::string prop;
        while (std::getline(ss, prop, ',')) {
            prop = trim(prop); if (prop.empty()) continue;
            size_t space_pos = prop.find(' ');
            std::string key = (space_pos == std::string::npos) ? prop : prop.substr(0, space_pos);
            std::string value = (space_pos == std::string::npos) ? "" : trim(prop.substr(space_pos + 1));
            if (key == "sameas") {
                int id = std::stoi(value);
                if (id < world.block_types.size() && world.block_types[id]) {
                    BlockType* b = world.block_types[id]; name = b->name; texture = b->block_face_textures; model = b->model;
                }
            } else if (key == "name") name = remove_quotes(value);
            else if (key.find("texture.") == 0) texture[key.substr(8)] = value;
            else if (key == "model") if (model_registry.count(value)) model = model_registry[value]();
        }
        if (number >= world.block_types.size()) world.block_types.resize(number + 1, nullptr);
        world.block_types[number] = new BlockType(&tm, name, texture, model);
    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height); SCR_WIDTH = width; SCR_HEIGHT = height;
    if(player_ptr) { player_ptr->view_width=width; player_ptr->view_height=height; }
    if(text_renderer) text_renderer->SetScreenSize(width, height);
    if(post_processor) post_processor->resize(width, height);
    if(quad_renderer) quad_renderer->set_screen_size(width, height);
    if(inventory_ui) inventory_ui->set_screen_size(width, height);
    update_crosshair_mesh(width, height);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (in_menu) return;
    if(!mouse_captured) return;
    static double lastX = 400, lastY = 300; static bool firstMouse = true;
    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
    double xoffset = xpos - lastX; double yoffset = lastY - ypos; lastX = xpos; lastY = ypos;
    if(player_ptr) {
        player_ptr->rotation.x += xoffset * 0.002; player_ptr->rotation.y += yoffset * 0.002;
        if (player_ptr->rotation.y > 1.57f) player_ptr->rotation.y = 1.57f;
        if (player_ptr->rotation.y < -1.57f) player_ptr->rotation.y = -1.57f;

        static double last_log = 0.0;
        double now = glfwGetTime();
        if (now - last_log > 0.5) {
            std::cout << "[mouse] dx=" << xoffset << " dy=" << yoffset
                      << " rot=(" << player_ptr->rotation.x << "," << player_ptr->rotation.y << ")\n";
            last_log = now;
        }
    }
}

// --- SCROLL CALLBACK (прокрутка хотбара колесиком) ---
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    if (in_menu) return;
    // Если инвентарь открыт, не переключаем хотбар
    if (inventory_ui && inventory_ui->is_open()) return;
    if (!player_ptr) return;

    // yoffset > 0 = прокрутка вверх (влево по хотбару)
    // yoffset < 0 = прокрутка вниз (вправо по хотбару)
    int delta = 0;
    if (yoffset > 0) delta = -1;  // Прокрутка вверх = влево
    else if (yoffset < 0) delta = 1;  // Прокрутка вниз = вправо

    if (delta != 0) {
        int new_slot = player_ptr->hotbar_selected + delta;
        // Wrap around
        if (new_slot < 0) new_slot = Inventory::HOTBAR_SIZE - 1;
        if (new_slot >= Inventory::HOTBAR_SIZE) new_slot = 0;
        player_ptr->hotbar_selected = new_slot;
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (in_menu) return;
    if (inventory_ui && inventory_ui->is_open()) {
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        // Передаём модификаторы (mods) для поддержки Shift+Click
        inventory_ui->handle_mouse_button(button, action, mx, my, mods);
        return;
    }
    if(action == GLFW_PRESS) {
        if(!mouse_captured) {
            mouse_captured = true; glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        } else {
            HitRay ray(world_ptr, player_ptr->rotation, player_ptr->interpolated_position + glm::vec3(0, player_ptr->eyelevel, 0));
            auto cb = [&](glm::ivec3 curr, glm::ivec3 next) {
                if(button == GLFW_MOUSE_BUTTON_LEFT) {
                    world_ptr->break_block(next, true);
                } else if(button == GLFW_MOUSE_BUTTON_RIGHT) {
                    int block_id = get_selected_block_id();
                    if (block_id > 0 && world_ptr->try_set_block(curr, block_id, player_ptr->collider)) {
                        consume_selected_block();
                    }
                } else if(button == GLFW_MOUSE_BUTTON_MIDDLE) {
                    int picked = world_ptr->get_block_number(next);
                    if (picked > 0 && player_ptr) {
                        InventorySlot& slot = player_ptr->inventory.slot(player_ptr->hotbar_selected);
                        slot.id = picked;
                        slot.count = Inventory::MAX_STACK;
                        slot.metadata = 0;
                    }
                }
            };
                while(ray.distance < 5.0f) if(ray.step(cb)) break;
        }
    }
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (in_menu) {
        if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, true);
        return;
    }

    auto close_inventory = [&]() {
        if (inventory_ui && inventory_ui->is_open()) {
            inventory_ui->close(true);
            mouse_captured = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    };

    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_ESCAPE) {
            if (inventory_ui && inventory_ui->is_open()) {
                close_inventory();
            } else {
                mouse_captured = false;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        }
        if (key == GLFW_KEY_E || key == GLFW_KEY_I) {
            if (inventory_ui) {
                if (inventory_ui->is_open()) {
                    close_inventory();
                } else {
                    inventory_ui->open();
                    mouse_captured = false;
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                }
            }
        }
        if (inventory_ui && inventory_ui->is_open()) return;
        if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9 && player_ptr) player_ptr->hotbar_selected = key - GLFW_KEY_1;
        if (key == GLFW_KEY_F && player_ptr) player_ptr->flying = !player_ptr->flying;
        if (key == GLFW_KEY_F3) show_f3 = !show_f3;

        // --- СОХРАНЕНИЕ НА КЛАВИШУ O ---
        if (key == GLFW_KEY_O) {
            if (world_ptr && world_ptr->save_system) {
                std::cout << "Saving world..." << std::endl;
                world_ptr->save_system->save();
                std::cout << "Saved!" << std::endl;
            }
        }
        // -------------------------------

        if (key == GLFW_KEY_F11) {
            is_fullscreen = !is_fullscreen;
            if (is_fullscreen) {
                glfwGetWindowPos(window, &windowed_x, &windowed_y); glfwGetWindowSize(window, &windowed_w, &windowed_h);
                GLFWmonitor* monitor = get_current_monitor(window); const GLFWvidmode* mode = glfwGetVideoMode(monitor);
                glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
            } else {
                glfwSetWindowMonitor(window, nullptr, windowed_x, windowed_y, windowed_w, windowed_h, 0);
            }
        }
    }
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, GAME_TITLE.c_str(), NULL, NULL);

    // --- LOAD ICON ---
    {
        int width, height, channels;
        unsigned char* pixels = stbi_load("assets/Icon.png", &width, &height, &channels, 4);
        if (pixels) {
            GLFWimage images[1];
            images[0].width = width;
            images[0].height = height;
            images[0].pixels = pixels;
            glfwSetWindowIcon(window, 1, images);
            stbi_image_free(pixels);
        } else {
            std::cout << "Failed to load window icon." << std::endl;
        }
    }
    // -----------------

    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetScrollCallback(window, scroll_callback);  // Прокрутка хотбара колесиком

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    init_crosshair();
    update_crosshair_mesh(SCR_WIDTH, SCR_HEIGHT);
    init_ui_resources();

    text_renderer = new TextRenderer(SCR_WIDTH, SCR_HEIGHT);
    text_renderer->Load("assets/font.ttf", 24);
    post_processor = new PostProcessor(SCR_WIDTH, SCR_HEIGHT);

    float menu_volume = Audio::GetVolume();
    double menu_prev = glfwGetTime();
    while (!glfwWindowShouldClose(window) && in_menu) {
        double menu_now = glfwGetTime();
        float menu_dt = static_cast<float>(menu_now - menu_prev);
        menu_prev = menu_now;

        glfwPollEvents();

        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            menu_volume = std::clamp(menu_volume - menu_dt * 0.5f, 0.0f, 1.0f);
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            menu_volume = std::clamp(menu_volume + menu_dt * 0.5f, 0.0f, 1.0f);
        if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            in_menu = false;
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        glClearColor(0.06f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        int percent = static_cast<int>(menu_volume * 100.0f + 0.5f);
        const int barSegments = 20;
        int filledSegments = static_cast<int>(menu_volume * barSegments + 0.5f);
        std::string bar = "[" + std::string(filledSegments, '=') + std::string(barSegments - filledSegments, ' ') + "]";

        text_renderer->RenderText(GAME_TITLE, 40.0f, 80.0f, 1.6f, glm::vec3(1.0f));
        text_renderer->RenderText("Music volume: " + std::to_string(percent) + "%", 40.0f, 130.0f, 1.0f, glm::vec3(0.9f));
        text_renderer->RenderText(bar, 40.0f, 170.0f, 1.0f, glm::vec3(0.8f));
        text_renderer->RenderText("A/D or Left/Right - adjust", 40.0f, 220.0f, 0.9f, glm::vec3(0.75f));
        text_renderer->RenderText("Enter/Space - start, Esc - quit", 40.0f, 250.0f, 0.9f, glm::vec3(0.75f));

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);

        glfwSwapBuffers(window);
    }

    if (glfwWindowShouldClose(window)) {
        delete post_processor;
        glfwTerminate();
        return 0;
    }

    Audio::SetVolume(menu_volume);
    in_menu = false;

    Shader shader("assets/shaders/colored_lighting/vert.glsl", "assets/shaders/colored_lighting/frag.glsl");
    TextureManager tm(16, 16, 256);

    shader.use();
    int uTextureLoc = shader.find_uniform("u_TextureArraySampler");
    shader.setInt(uTextureLoc, 0);

    // Capture mouse by default in-game to ensure input works without a first click
    mouse_captured = true;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    World world(&shader, &tm, nullptr);
    Player player(&world, &shader, SCR_WIDTH, SCR_HEIGHT);
    world.player = &player;
    world_ptr = &world;
    player_ptr = &player;

    quad_renderer = new UiQuadRenderer();
    if (quad_renderer) quad_renderer->set_screen_size(SCR_WIDTH, SCR_HEIGHT);
    inventory_ui = new InventoryUI(&player, &world, text_renderer, world.get_preview_renderer(), quad_renderer);
    if (inventory_ui) inventory_ui->set_screen_size(SCR_WIDTH, SCR_HEIGHT);

    shader.use();

    load_blocks(world, tm);
    tm.generate_mipmaps();

    world.save_system = new Save(&world);
    world.save_system->load();

    bool inventory_empty = true;
    for (const auto& slot : player.inventory.data()) {
        if (!slot.empty()) { inventory_empty = false; break; }
    }
    if (inventory_empty) {
        player.inventory.add_item(1, 64);
        player.inventory.add_item(2, 64);
        player.inventory.add_item(3, 32);
        player.inventory.add_item(4, 16);
    }

    Audio::Init();

    double lastTime = glfwGetTime();
    int frames = 0;
    float fps_display = 0.0f;
    double fps_timer = 0.0;

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        double dt = now - lastTime;
        lastTime = now;
        if (dt > 0.1) dt = 0.1; // avoid giant steps if paused

        // Keep cursor capture state enforced (e.g., after alt-tab)
        if (mouse_captured) glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        else glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

        Audio::Update(static_cast<float>(dt)); // Обновление музыки
        if (world.save_system) {
            world.save_system->update_streaming(player.position);
            world.save_system->stream_next(1); // Постепенно подгружаем чанки без длинного старта
        }

        bool inventory_open = inventory_ui && inventory_ui->is_open();
        float sim_dt = inventory_open ? 0.0f : static_cast<float>(dt);

        player.input = glm::vec3(0);
        bool forward = false;
        if(glfwGetKey(window, GLFW_KEY_W)==GLFW_PRESS) { player.input.z += 1; forward = true; }
        if(glfwGetKey(window, GLFW_KEY_S)==GLFW_PRESS) player.input.z -= 1;
        if(glfwGetKey(window, GLFW_KEY_A)==GLFW_PRESS) player.input.x -= 1;
        if(glfwGetKey(window, GLFW_KEY_D)==GLFW_PRESS) player.input.x += 1;
        if(glfwGetKey(window, GLFW_KEY_SPACE)==GLFW_PRESS) player.input.y += 1;
        if(glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)==GLFW_PRESS && player.flying) player.input.y -= 1;
        if (inventory_open) player.input = glm::vec3(0);
        bool ctrl = (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL)==GLFW_PRESS);
        player.handle_input_sprint(ctrl, forward);

        player.update(sim_dt);
        if (!inventory_open) {
            world.tick(sim_dt);
        }

        // Обновляем матрицы камеры и список видимых чанков до рендера теней
        // ВАЖНО: делаем use() основного шейдера до update_matrices,
        // чтобы u_MVPMatrix/u_ViewMatrix записывались в правильную программу.
        shader.use();
        player.update_matrices(1.0f);
        world.prepare_rendering();
        world.render_shadows();

        float daylight_factor = world.get_daylight_factor();
        glClearColor(0.5f * daylight_factor, 0.8f * daylight_factor, 1.0f * daylight_factor, 1.0f);

        post_processor->beginRender();
        shader.use();

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, tm.texture_array);

        world.draw();

        glm::ivec3 headPos = glm::round(player.interpolated_position + glm::vec3(0, player.eyelevel, 0));
        int headBlock = world.get_block_number(headPos);
        bool isUnderwater = (headBlock == 8 || headBlock == 9);

        post_processor->endRenderAndDraw(isUnderwater, static_cast<float>(now));

        double cursor_x = 0.0, cursor_y = 0.0;
        glfwGetCursorPos(window, &cursor_x, &cursor_y);

        if (!(inventory_ui && inventory_ui->is_open())) {
            draw_crosshair();
        }
        draw_health_bar();
        if (inventory_ui) {
            inventory_ui->render(cursor_x, cursor_y);
        }
        draw_f3_screen(fps_display);

        glfwSwapBuffers(window);
        glfwPollEvents();
        frames++;

        if (glfwGetTime() - fps_timer > 1.0) {
            fps_display = frames;
            frames = 0;
            fps_timer = glfwGetTime();
            // Debug input state once per second
            int w_state = glfwGetKey(window, GLFW_KEY_W);
            int a_state = glfwGetKey(window, GLFW_KEY_A);
            int s_state = glfwGetKey(window, GLFW_KEY_S);
            int d_state = glfwGetKey(window, GLFW_KEY_D);
            std::cout << "[input] captured=" << mouse_captured
                      << " W:" << w_state << " A:" << a_state << " S:" << s_state << " D:" << d_state
                      << " rot=(" << player.rotation.x << "," << player.rotation.y << ")"
                      << " pos=(" << player.position.x << "," << player.position.y << "," << player.position.z << ")"
                      << std::endl;
        }

    }

    world.save_system->save();
    if (inventory_ui) { delete inventory_ui; inventory_ui = nullptr; }
    if (quad_renderer) { delete quad_renderer; quad_renderer = nullptr; }
    if (uiTriangleShader) { delete uiTriangleShader; uiTriangleShader = nullptr; }
    if (triangleVBO) glDeleteBuffers(1, &triangleVBO);
    if (triangleVAO) glDeleteVertexArrays(1, &triangleVAO);
    Audio::Close();
    delete post_processor;
    glfwTerminate();
    return 0;
}
