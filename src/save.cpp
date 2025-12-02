#include "save.h"
#include "world.h"
#include "nbt_utils.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <vector>
#include <cmath>

namespace fs = std::filesystem;

Save::Save(World* w) : world(w), path("save") {}

// Функция для генерации имен папок (как в Minecraft Classic/Indev)
std::string to_base36(int value) {
    std::string chars = "0123456789abcdefghijklmnopqrstuvwxyz";
    std::string result = "";
    if (value == 0) return "0";
    int val = std::abs(value);
    while (val > 0) { result = chars[val % 36] + result; val /= 36; }
    if (value < 0) result = "-" + result;
    return result;
}

namespace {
int manhattan_distance_offset(const glm::ivec3& offset) { return std::abs(offset.x) + std::abs(offset.z); }
int ring_distance_offset(const glm::ivec3& offset) { return std::max(std::abs(offset.x), std::abs(offset.z)); }
}

void Save::save() {
    // Убедимся, что корневая папка существует
    if (!fs::exists(path)) fs::create_directory(path);

    int saved_count = 0;

    for(auto& kv : world->chunks) {
        Chunk* c = kv.second;

        // Сохраняем только если были изменения (или можно убрать проверку для принудительного сохранения)
        if(!c->modified) continue;

        // Вычисляем путь к папке: save/1a/2b/
        int x = kv.first.x;
        int z = kv.first.z;

        // Хеширование папок (mod 64)
        int rx = x % 64; if(rx<0) rx+=64;
        int rz = z % 64; if(rz<0) rz+=64;

        std::string dir_path = path + "/" + to_base36(rx) + "/" + to_base36(rz);

        // Создаем вложенные папки
        if (!fs::exists(dir_path)) {
            fs::create_directories(dir_path);
        }

        // Имя файла: c.base36(x).base36(z).dat
        std::string filename = dir_path + "/c." + to_base36(x) + "." + to_base36(z) + ".dat";

        // Сохраняем в формате NBT Gzip
        bool success = NBT::write_blocks_to_gzip(
            filename,
            (const uint8_t*)c->blocks,
            CHUNK_WIDTH, CHUNK_HEIGHT, CHUNK_LENGTH
        );

        if (success) {
            c->modified = false;
            saved_count++;
        } else {
            std::cout << "Failed to save chunk: " << x << ", " << z << std::endl;
        }
    }
    std::cout << "Saved " << saved_count << " chunks." << std::endl;
}

bool Save::load_chunk(const glm::ivec3& chunk_pos) {
    if (world->chunks.find(chunk_pos) != world->chunks.end()) return false;

    Chunk* c = new Chunk(world, chunk_pos);
    world->chunks[chunk_pos] = c;
    bool loaded = false;

    // Пробуем загрузить из структурированной папки (NBT)
    int x = chunk_pos.x;
    int z = chunk_pos.z;
    int rx = x % 64; if(rx<0) rx+=64;
    int rz = z % 64; if(rz<0) rz+=64;
    std::string nbt_path = path + "/" + to_base36(rx) + "/" + to_base36(rz) + "/c." + to_base36(x) + "." + to_base36(z) + ".dat";
    if (std::filesystem::exists(nbt_path)) {
        if (NBT::read_blocks_from_gzip(nbt_path, (uint8_t*)c->blocks, sizeof(c->blocks))) loaded = true;
    }

    // Fallback: Пробуем старый формат (плоская папка), если вдруг он остался
    if (!loaded) {
        std::string bin_path = path + "/c." + std::to_string(x) + "." + std::to_string(z) + ".dat";
        std::ifstream in(bin_path, std::ios::binary);
        if (in.is_open()) { in.read((char*)c->blocks, sizeof(c->blocks)); loaded = true; }
    }

    // Генерация нового чанка, если не загрузили
    if (!loaded) {
        // Flat world gen
        for (int lx = 0; lx < CHUNK_WIDTH; lx++) {
            for (int lz = 0; lz < CHUNK_LENGTH; lz++) {
                for (int ly = 0; ly < CHUNK_HEIGHT; ly++) {
                    if (ly < 60) c->blocks[lx][ly][lz] = 1;
                    else if (ly < 64) c->blocks[lx][ly][lz] = 3;
                    else if (ly == 64) c->blocks[lx][ly][lz] = 2;
                    else c->blocks[lx][ly][lz] = 0;
                }
            }
        }
        c->modified = true;
    }

    // Инициализация света для только что созданного чанка
    world->init_skylight(c);

    // Проставляем block light для источников
    auto is_light_source = [&](int id) {
        return std::find(world->light_blocks.begin(), world->light_blocks.end(), id) != world->light_blocks.end();
    };

    for(int lx=0; lx<CHUNK_WIDTH; lx++) {
        for(int ly=0; ly<CHUNK_HEIGHT; ly++) {
            for(int lz=0; lz<CHUNK_LENGTH; lz++) {
                int id = c->blocks[lx][ly][lz];
                if (id == 0) continue;
                if (is_light_source(id)) {
                    c->set_block_light({lx,ly,lz}, 15);
                    glm::ivec3 global_pos = {c->chunk_position.x * 16 + lx, ly, c->chunk_position.z * 16 + lz};
                    world->light_increase_queue.push_back({global_pos, 15});
                }
            }
        }
    }

    world->propagate_skylight_increase(false);
    world->propagate_increase(false);

    c->update_subchunk_meshes();
    return true;
}

void Save::load(int initial_radius) {
    glm::ivec3 center_chunk(0);
    if (world && world->player) {
        center_chunk = world->get_chunk_pos(world->player->position);
    }

    std::cout << "Loading world around chunk (" << center_chunk.x << ", " << center_chunk.z << ")..." << std::endl;

    world->light_increase_queue.clear();
    world->skylight_increase_queue.clear();
    pending_chunks.clear();

    const int radius = Options::RENDER_DISTANCE;
    const int max_initial = std::max(0, radius - 1);
    initial_radius = std::clamp(initial_radius, 0, max_initial);

    std::vector<glm::ivec3> offsets;
    offsets.reserve(radius * radius * 4);
    for(int x = (-1) * radius; x < radius; x++) {
        for(int z = (-1) * radius; z < radius; z++) {
            offsets.push_back({x, 0, z});
        }
    }

    std::sort(offsets.begin(), offsets.end(), [](const glm::ivec3& a, const glm::ivec3& b){
        int da = manhattan_distance_offset(a);
        int db = manhattan_distance_offset(b);
        if (da == db) return ring_distance_offset(a) < ring_distance_offset(b);
        return da < db;
    });

    int loaded_now = 0;
    for (const auto& offset : offsets) {
        glm::ivec3 chunk_pos = center_chunk + offset;
        if (ring_distance_offset(offset) <= initial_radius) {
            load_chunk(chunk_pos);
            loaded_now++;
        } else {
            pending_chunks.push_back(chunk_pos);
        }
    }

    std::cout << "Loaded " << loaded_now << " chunks upfront, queued " << pending_chunks.size() << " for streaming." << std::endl;
}

void Save::stream_next(int max_chunks) {
    if (max_chunks <= 0) return;
    int loaded = 0;
    while (loaded < max_chunks && !pending_chunks.empty()) {
        glm::ivec3 pos = pending_chunks.front();
        pending_chunks.pop_front();
        load_chunk(pos);
        loaded++;
    }
    if (pending_chunks.empty() && loaded > 0) {
        std::cout << "Chunk streaming finished." << std::endl;
    }
}
