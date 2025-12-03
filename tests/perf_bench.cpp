#include <algorithm>
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <iostream>
#include <numeric>
#include <random>
#include <utility>
#include <vector>

using Clock = std::chrono::high_resolution_clock;

double bench_sort_with_sqrt(int count, int iterations) {
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-500.0f, 500.0f);

    glm::vec3 player(0.0f);
    std::vector<glm::vec3> base(count);
    for (auto& v : base) v = {dist(rng), dist(rng), dist(rng)};

    double total_ms = 0.0;
    for (int i = 0; i < iterations; i++) {
        auto data = base; // copy to avoid best-case sorts
        auto start = Clock::now();
        std::sort(data.begin(), data.end(), [&](const glm::vec3& a, const glm::vec3& b) {
            // Matches World::prepare_rendering comparator (glm::distance -> sqrt)
            return glm::distance(player, a) > glm::distance(player, b);
        });
        auto end = Clock::now();
        total_ms += std::chrono::duration<double, std::milli>(end - start).count();
    }
    return total_ms / iterations;
}

double bench_sort_with_dist2(int count, int iterations) {
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-500.0f, 500.0f);

    glm::vec3 player(0.0f);
    std::vector<std::pair<float, glm::vec3>> base(count);
    for (auto& v : base) {
        glm::vec3 p{dist(rng), dist(rng), dist(rng)};
        float d2 = glm::length2(p - player);
        v = {d2, p};
    }

    double total_ms = 0.0;
    for (int i = 0; i < iterations; i++) {
        auto data = base;
        auto start = Clock::now();
        std::sort(data.begin(), data.end(), [](const auto& a, const auto& b) {
            return a.first > b.first; // sort by squared distance
        });
        auto end = Clock::now();
        total_ms += std::chrono::duration<double, std::milli>(end - start).count();
    }
    return total_ms / iterations;
}

struct DummySubchunk {
    std::vector<float> mesh;
    std::vector<float> translucent;
};

std::vector<DummySubchunk> make_subchunks(int count, int mesh_size, int translucent_size) {
    std::vector<DummySubchunk> subchunks;
    subchunks.reserve(count);
    for (int i = 0; i < count; i++) {
        DummySubchunk sc;
        sc.mesh.resize(mesh_size, 1.0f);
        sc.translucent.resize(translucent_size, 2.0f);
        subchunks.push_back(std::move(sc));
    }
    return subchunks;
}

double bench_merge_no_reserve(const std::vector<DummySubchunk>& subs, int iterations) {
    double total_ms = 0.0;
    for (int i = 0; i < iterations; i++) {
        std::vector<float> mesh;
        std::vector<float> translucent;
        auto start = Clock::now();
        for (auto& sc : subs) {
            mesh.insert(mesh.end(), sc.mesh.begin(), sc.mesh.end());
            translucent.insert(translucent.end(), sc.translucent.begin(), sc.translucent.end());
        }
        auto end = Clock::now();
        total_ms += std::chrono::duration<double, std::milli>(end - start).count();
    }
    return total_ms / iterations;
}

double bench_merge_with_reserve(const std::vector<DummySubchunk>& subs, int iterations) {
    double total_ms = 0.0;
    size_t mesh_total = 0;
    size_t translucent_total = 0;
    for (auto& sc : subs) {
        mesh_total += sc.mesh.size();
        translucent_total += sc.translucent.size();
    }

    for (int i = 0; i < iterations; i++) {
        std::vector<float> mesh;
        std::vector<float> translucent;
        mesh.reserve(mesh_total);
        translucent.reserve(translucent_total);

        auto start = Clock::now();
        for (auto& sc : subs) {
            mesh.insert(mesh.end(), sc.mesh.begin(), sc.mesh.end());
            translucent.insert(translucent.end(), sc.translucent.begin(), sc.translucent.end());
        }
        auto end = Clock::now();
        total_ms += std::chrono::duration<double, std::milli>(end - start).count();
    }
    return total_ms / iterations;
}

int main() {
    const int chunk_count = 2000;
    const int iterations = 6;

    double sqrt_sort = bench_sort_with_sqrt(chunk_count, iterations);
    double dist2_sort = bench_sort_with_dist2(chunk_count, iterations);

    std::cout << "Sort by glm::distance (sqrt comparator): " << sqrt_sort << " ms avg\n";
    std::cout << "Sort by squared distance (precomputed):  " << dist2_sort << " ms avg\n";

    // Simulate 32 subchunks with moderate mesh payload
    auto subs = make_subchunks(32, 12000, 3000);
    double merge_plain = bench_merge_no_reserve(subs, iterations);
    double merge_reserved = bench_merge_with_reserve(subs, iterations);

    std::cout << "Merge subchunk meshes without reserve:    " << merge_plain << " ms avg\n";
    std::cout << "Merge subchunk meshes with pre-reserve:   " << merge_reserved << " ms avg\n";
    return 0;
}
