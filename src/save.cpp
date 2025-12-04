#include "save.h"
#include "world.h"
#include "nbt_utils.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <vector>
#include <cmath>
#include <limits>

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

bool Save::save_chunk(Chunk* chunk) {
    if (!chunk) return false;

    if (!fs::exists(path)) fs::create_directory(path);

    int x = chunk->chunk_position.x;
    int z = chunk->chunk_position.z;

    int rx = x % 64; if(rx<0) rx+=64;
    int rz = z % 64; if(rz<0) rz+=64;

    std::string dir_path = path + "/" + to_base36(rx) + "/" + to_base36(rz);

    if (!fs::exists(dir_path)) {
        fs::create_directories(dir_path);
    }

    std::string filename = dir_path + "/c." + to_base36(x) + "." + to_base36(z) + ".dat";

    bool success = NBT::write_blocks_to_gzip(
        filename,
        (const uint8_t*)chunk->blocks,
        CHUNK_WIDTH, CHUNK_HEIGHT, CHUNK_LENGTH
    );

    if (success) {
        chunk->modified = false;
    }

    return success;
}

void Save::save() {
    // Убедимся, что корневая папка существует
    if (!fs::exists(path)) fs::create_directory(path);

    int saved_count = 0;

    for(auto& kv : world->chunks) {
        Chunk* c = kv.second;

        // Сохраняем только если были изменения (или можно убрать проверку для принудительного сохранения)
        if(!c->modified) continue;

        if (save_chunk(c)) {
            saved_count++;
        } else {
            std::cout << "Failed to save chunk: " << kv.first.x << ", " << kv.first.z << std::endl;
        }
    }
    std::cout << "Saved " << saved_count << " chunks." << std::endl;
}

bool Save::load_chunk(const glm::ivec3& chunk_pos, bool eager_build) {
    if (world->chunks.find(chunk_pos) != world->chunks.end()) return false;

    Chunk* c = new Chunk(world, chunk_pos);
    world->chunks[chunk_pos] = c;

    // Link neighbor pointers for fast access.
    static const int OPPOSITE[6] = {1, 0, 3, 2, 5, 4};
    for (int i = 0; i < 6; i++) {
        glm::ivec3 npos = chunk_pos + Util::DIRECTIONS[i];
        auto it = world->chunks.find(npos);
        Chunk* n = (it != world->chunks.end()) ? it->second : nullptr;
        c->neighbors[i] = n;
        if (n) n->neighbors[OPPOSITE[i]] = c;
    }
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
    if (loaded) {
        bool has_data = false;
        for (int lx = 0; lx < CHUNK_WIDTH && !has_data; lx++) {
            for (int ly = 0; ly < CHUNK_HEIGHT && !has_data; ly++) {
                for (int lz = 0; lz < CHUNK_LENGTH; lz++) {
                    if (c->blocks[lx][ly][lz] != 0) { has_data = true; break; }
                }
            }
        }
        if (!has_data) loaded = false;
    }

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
    world->stitch_sky_light(c);

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

    // Подхватываем свет от соседних чанков (торчи, лава и т.п.) на границах
    world->stitch_block_light(c);

    if (eager_build) {
        world->propagate_skylight_increase(false, std::numeric_limits<int>::max());
        world->propagate_increase(false, std::numeric_limits<int>::max());
    }

    c->update_subchunk_meshes();

    auto update_neighbor = [&](glm::ivec3 offset) {
        glm::ivec3 n_pos = c->chunk_position + offset;
        auto it = world->chunks.find(n_pos);
        if (it == world->chunks.end()) return;
        Chunk* neighbor = it->second;
        neighbor->modified = true;
        neighbor->update_subchunk_meshes();
    };

    update_neighbor(Util::EAST);
    update_neighbor(Util::WEST);
    update_neighbor(Util::NORTH);
    update_neighbor(Util::SOUTH);

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
            load_chunk(chunk_pos, true);
            loaded_now++;
        } else {
            pending_chunks.push_back(chunk_pos);
        }
    }

    std::cout << "Loaded " << loaded_now << " chunks upfront, queued " << pending_chunks.size() << " for streaming." << std::endl;
}

void Save::update_streaming(glm::vec3 player_pos) {
    glm::ivec3 current_center = world->get_chunk_pos(player_pos);

    // No-op if player stays in the same chunk.
    if (current_center == last_center_chunk) return;

    last_center_chunk = current_center;
    int radius = Options::RENDER_DISTANCE;

    // --- QUEUE GENERATION AROUND PLAYER ---
    for (int x = -radius; x <= radius; x++) {
        for (int z = -radius; z <= radius; z++) {
            if (x * x + z * z > radius * radius) continue;

            glm::ivec3 chunk_pos = current_center + glm::ivec3(x, 0, z);

            // Skip if chunk already exists.
            if (world->chunks.find(chunk_pos) != world->chunks.end()) continue;

            // Avoid duplicates in pending queue.
            bool already_queued = false;
            for (const auto& p : pending_chunks) {
                if (p == chunk_pos) { already_queued = true; break; }
            }
            if (!already_queued) pending_chunks.push_back(chunk_pos);
        }
    }

    // --- UNLOAD FAR CHUNKS ---
    const int unload_dist_sq = (radius + 3) * (radius + 3);

    for (auto it = world->chunks.begin(); it != world->chunks.end(); ) {
        glm::ivec3 pos = it->first;
        Chunk* c = it->second;

        int dx = pos.x - current_center.x;
        int dz = pos.z - current_center.z;
        int dist_sq = dx * dx + dz * dz;

        if (dist_sq > unload_dist_sq) {
            if (c->modified) {
                save_chunk(c);
            }

            static const int OPPOSITE[6] = {1, 0, 3, 2, 5, 4};
            for (int i = 0; i < 6; i++) {
                Chunk* n = c->neighbors[i];
                if (n) n->neighbors[OPPOSITE[i]] = nullptr;
            }

            auto& visible = world->visible_chunks;
            visible.erase(std::remove(visible.begin(), visible.end(), c), visible.end());

            auto& build_queue = world->chunk_building_queue;
            build_queue.erase(std::remove(build_queue.begin(), build_queue.end(), c), build_queue.end());

            delete c;
            it = world->chunks.erase(it);
        } else {
            ++it;
        }
    }
}

void Save::stream_next(int max_chunks) {
    if (max_chunks <= 0) return;
    int loaded = 0;
    while (loaded < max_chunks && !pending_chunks.empty()) {
        glm::ivec3 pos = pending_chunks.front();
        pending_chunks.pop_front();
        load_chunk(pos, false);
        loaded++;
    }
    if (pending_chunks.empty() && loaded > 0) {
        std::cout << "Chunk streaming finished." << std::endl;
    }
}
