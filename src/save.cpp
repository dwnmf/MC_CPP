#include "save.h"
#include "world.h"
#include "nbt_utils.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <algorithm>

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

void Save::load() {
    std::cout << "Loading world..." << std::endl;

    world->light_increase_queue.clear();
    world->skylight_increase_queue.clear();

    // 1. LOAD BLOCKS
    for(int x = (-1) * Options::RENDER_DISTANCE; x < Options::RENDER_DISTANCE; x++) {
        for(int z = (-1) * Options::RENDER_DISTANCE; z < Options::RENDER_DISTANCE; z++) {
            glm::ivec3 pos = {x, 0, z};
            world->chunks[pos] = new Chunk(world, pos);
            Chunk* c = world->chunks[pos];
            bool loaded = false;

            // Пробуем загрузить из структурированной папки (NBT)
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
        }
    }

    std::cout << "Calculating light..." << std::endl;

    // 2. INIT SKYLIGHT
    // Must be done for ALL chunks before propagation starts
    for(auto& kv : world->chunks) {
        world->init_skylight(kv.second);
    }

    // 3. INIT BLOCK LIGHT
    for(auto& kv : world->chunks) {
        Chunk* c = kv.second;
        for(int x=0; x<CHUNK_WIDTH; x++) {
            for(int y=0; y<CHUNK_HEIGHT; y++) {
                for(int z=0; z<CHUNK_LENGTH; z++) {
                    int id = c->blocks[x][y][z];
                    if (id == 0) continue;
                    bool is_light_source = false;
                    for(int light_id : world->light_blocks) {
                        if(id == light_id) { is_light_source = true; break; }
                    }
                    if (is_light_source) {
                        c->set_block_light({x,y,z}, 15);
                        glm::ivec3 global_pos = {c->chunk_position.x * 16 + x, y, c->chunk_position.z * 16 + z};
                        world->light_increase_queue.push_back({global_pos, 15});
                    }
                }
            }
        }
    }

    std::cout << "Propagating light..." << std::endl;

    // 4. PROPAGATE
    world->propagate_skylight_increase(false);
    world->propagate_increase(false);

    std::cout << "Building meshes..." << std::endl;
    for(auto& kv : world->chunks) kv.second->update_subchunk_meshes();
}
