#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <vector>

#include "../src/world.h"
#include "../src/physics/collider.h"

using Clock = std::chrono::high_resolution_clock;

static BlockType* make_block_type(bool transparent, bool translucent = false) {
    auto* bt = new BlockType();
    bt->transparent = transparent;
    bt->glass = false;
    bt->translucent = translucent;
    bt->is_cube = true;
    bt->vertex_positions = std::vector<std::vector<float>>(6, std::vector<float>(12, 0.0f));
    bt->tex_coords = std::vector<std::vector<float>>(6, std::vector<float>(8, 0.0f));
    bt->tex_indices = std::vector<int>(6, 0);
    bt->shading_values = std::vector<std::vector<float>>(6, std::vector<float>(4, 1.0f));
    return bt;
}

static std::unique_ptr<World> build_world_for_bench() {
    auto world = std::make_unique<World>(nullptr, nullptr, nullptr);
    world->block_types.resize(76);
    world->block_types[1] = make_block_type(false); // обычный блок
    world->block_types[10] = make_block_type(false); // источник света
    return world;
}

double bench_set_block(int block_id, int iterations) {
    auto world = build_world_for_bench();
    std::vector<glm::ivec3> positions(iterations);
    for (int i = 0; i < iterations; i++) {
        int x = i % 16;
        int z = (i / 16) % 16;
        int y = 60 + (i % 8);
        positions[i] = {x, y, z};
    }

    auto start = Clock::now();
    for (int i = 0; i < iterations; i++) {
        world->set_block(positions[i], block_id);
        world->set_block(positions[i], 0); // удаляем, чтобы прогнать обратное освещение
    }
    auto end = Clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

static void fill_chunk(Chunk* chunk, int block_id, bool sparse) {
    for (int x = 0; x < CHUNK_WIDTH; x++) {
        for (int z = 0; z < CHUNK_LENGTH; z++) {
            for (int y = 0; y < CHUNK_HEIGHT; y++) {
                bool place = !sparse || ((x + y + z) % 31 == 0);
                chunk->blocks[x][y][z] = place ? block_id : 0;
            }
        }
    }
}

double bench_chunk_meshing(bool sparse, int iterations) {
    auto world = build_world_for_bench();
    Chunk chunk(world.get(), {0, 0, 0});
    fill_chunk(&chunk, 1, sparse);

    auto rebuild_subchunks = [&]() {
        for (auto& kv : chunk.subchunks) {
            kv.second->update_mesh();
        }
    };

    rebuild_subchunks(); // первый прогон для инициализации
    chunk.update_mesh();

    auto start = Clock::now();
    for (int i = 0; i < iterations; i++) {
        rebuild_subchunks();
        chunk.update_mesh();
    }
    auto end = Clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count() / iterations;
}

int main() {
    const int set_iters = 500;
    double opaque_ms = bench_set_block(1, set_iters);
    double light_ms = bench_set_block(10, set_iters);

    std::cout << "[set_block] " << set_iters << " opaque place/remove: " << opaque_ms << " ms total\n";
    std::cout << "[set_block] " << set_iters << " light place/remove:  " << light_ms << " ms total\n";

    double dense_mesh = bench_chunk_meshing(false, 10);
    double sparse_mesh = bench_chunk_meshing(true, 10);
    std::cout << "[meshing] dense chunk avg:  " << dense_mesh << " ms per rebuild\n";
    std::cout << "[meshing] sparse chunk avg: " << sparse_mesh << " ms per rebuild\n";
    return 0;
}
