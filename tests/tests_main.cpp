#include <iostream>
#include <vector>
#include <memory>
#include <cmath>
#include <functional>
#include <glm/glm.hpp>
#include "../src/world.h"
#include "../src/physics/collider.h"
#include "../src/physics/hit.h"

struct TestRunner {
    int passed = 0;
    int failed = 0;
    void check(bool condition, const std::string& name, const std::string& message) {
        if (condition) {
            passed++;
            std::cout << "[PASS] " << name << "\n";
        } else {
            failed++;
            std::cout << "[FAIL] " << name << ": " << message << "\n";
        }
    }
    int report() const {
        std::cout << "Tests passed: " << passed << ", failed: " << failed << "\n";
        return failed;
    }
};

static BlockType* make_block_type(bool transparent, bool glass = false, bool translucent = false) {
    auto* bt = new BlockType();
    bt->transparent = transparent;
    bt->glass = glass;
    bt->translucent = translucent;
    bt->is_cube = true;
    bt->vertex_positions = std::vector<std::vector<float>>(6, std::vector<float>(12, 0.0f));
    bt->tex_coords = std::vector<std::vector<float>>(6, std::vector<float>(8, 0.0f));
    bt->tex_indices = std::vector<int>(6, 0);
    bt->shading_values = std::vector<std::vector<float>>(6, std::vector<float>(4, 1.0f));
    return bt;
}

static BlockType* make_solid_block_with_collider() {
    auto* bt = make_block_type(false);
    bt->colliders.push_back(Collider(glm::vec3(0), glm::vec3(1, 1, 1)));
    return bt;
}

static std::unique_ptr<World> build_test_world() {
    auto world = std::make_unique<World>(nullptr, nullptr, nullptr);
    world->block_types.resize(11);
    world->block_types[1] = make_solid_block_with_collider();   // Solid block
    world->block_types[2] = make_solid_block_with_collider();   // Ground block
    world->block_types[3] = make_solid_block_with_collider();   // Grass block
    world->block_types[10] = make_block_type(false);            // Light source
    return world;
}

static bool ivec_equal(glm::ivec3 a, glm::ivec3 b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

static void test_chunk_and_local_coords(TestRunner& tr) {
    auto world = build_test_world();
    tr.check(ivec_equal(world->get_chunk_pos({0, 0, 0}), {0, 0, 0}),
             "chunk_pos_origin", "Origin should map to chunk (0,0,0)");
    tr.check(ivec_equal(world->get_chunk_pos({-1, 0, -1}), {-1, 0, -1}),
             "chunk_pos_negative", "Negative positions should floor toward -inf");
    tr.check(ivec_equal(world->get_local_pos({0, 0, 0}), {0, 0, 0}),
             "local_pos_origin", "Local pos for origin should be zero");
    tr.check(ivec_equal(world->get_local_pos({-1, -1, -1}), {15, 127, 15}),
             "local_pos_negative", "Negative coords should wrap into chunk bounds");
    tr.check(world->get_skylight({0, 10, 0}) == 15,
             "skylight_open_sky", "Empty world should return full skylight");
}

static void test_block_placement_and_updates(TestRunner& tr) {
    auto world = build_test_world();
    glm::ivec3 global = {1, 2, 3};
    world->set_block(global, 1);

    auto cp = world->get_chunk_pos(glm::vec3(global));
    auto lp = world->get_local_pos(glm::vec3(global));
    auto it = world->chunks.find(cp);
    tr.check(it != world->chunks.end(), "chunk_created_on_set", "Chunk should be created when placing a block");
    tr.check(it != world->chunks.end() && it->second->blocks[lp.x][lp.y][lp.z] == 1,
             "block_written", "Block id should be stored inside the chunk");
    tr.check(it != world->chunks.end() && it->second->modified,
             "chunk_marked_modified", "Chunk should be flagged as modified after placement");
    tr.check(it != world->chunks.end() && !it->second->chunk_update_queue.empty(),
             "chunk_has_pending_updates", "Chunk update queue should contain subchunks to rebuild");
}

static void test_light_propagation(TestRunner& tr) {
    auto world = build_test_world();
    glm::ivec3 source = {0, 64, 0};
    world->set_block(source, 10);

    tr.check(world->get_light(source) == 15, "light_source_level", "Light source should emit level 15");
    tr.check(world->get_light({1, 64, 0}) == 14, "light_decay_to_neighbor", "Neighbor should receive level 14 light");

    world->set_block(source, 0);
    tr.check(world->get_light(source) == 0, "light_cleared_after_removal", "Removing light source should clear its light");
    tr.check(world->get_light({1, 64, 0}) == 0, "light_removed_from_neighbors", "Neighbors should be cleared after removal");
}

static void test_block_placement_collides_with_player(TestRunner& tr) {
    auto world = build_test_world();
    Collider player(glm::vec3(0, 0, 0), glm::vec3(1, 2, 1));

    bool placed = world->try_set_block({0, 0, 0}, 1, player);
    tr.check(!placed, "placement_blocked_by_player", "Block overlapping the player should not be placed");
    tr.check(world->get_block_number({0, 0, 0}) == 0, "blocked_block_not_written", "Blocked placement should leave air");

    bool placed_far = world->try_set_block({2, 0, 0}, 1, player);
    tr.check(placed_far, "placement_allowed_far_from_player", "Non-overlapping placement should succeed");
    tr.check(world->get_block_number({2, 0, 0}) == 1, "block_written_far", "Successful placement should write block");
}

static void test_collider_sweep(TestRunner& tr) {
    Collider moving(glm::vec3(0, 0, 0), glm::vec3(1, 1, 1));
    Collider wall(glm::vec3(1.5f, 0, 0), glm::vec3(2.5f, 1, 1));
    auto [t, normal] = moving.collide(wall, glm::vec3(2, 0, 0));

    tr.check(std::abs(t - 0.25f) < 1e-4f, "collider_time_of_impact", "Collision time should be 0.25 for a 2m/s move");
    tr.check(ivec_equal(glm::ivec3(normal), glm::ivec3(-1, 0, 0)),
             "collider_normal", "Collision normal should point against movement on X");
}

static void test_hit_ray_finds_block(TestRunner& tr) {
    auto world = build_test_world();
    world->set_block({1, 0, 0}, 1);

    HitRay ray(world.get(), glm::vec2(0.0f, 0.0f), glm::vec3(0.1f, 0.1f, 0.1f));
    glm::ivec3 hit_block(0);
    bool hit = false;
    for (int i = 0; i < 10 && !hit; i++) {
        hit = ray.step([&](glm::ivec3 current, glm::ivec3 next) {
            hit_block = next;
        });
    }
    tr.check(hit, "hit_ray_hits", "Raycast should detect the neighboring solid block");
    tr.check(ivec_equal(hit_block, glm::ivec3(1, 0, 0)), "hit_ray_block_coords", "Raycast should hit the block at (1,0,0)");
}

static void test_save_load_centers_on_player(TestRunner& tr) {
    auto world = build_test_world();
    Player player(world.get(), nullptr, 800, 600);
    world->player = &player;

    // Place the player far from the origin so the loaded area should not include (0,0) chunks
    player.position = glm::vec3(800.0f, 80.0f, -800.0f); // Chunk (50, 0, -50)

    Save save(world.get());
    save.path = "save_test_render_center";
    save.load(2);

    glm::ivec3 center_chunk = world->get_chunk_pos(player.position);

    bool center_loaded = world->chunks.find(center_chunk) != world->chunks.end();
    bool origin_loaded = world->chunks.find(glm::ivec3(0)) != world->chunks.end();

    tr.check(center_loaded, "save_load_includes_player_chunk", "Save::load should start around the player's chunk, not the origin");
    tr.check(!origin_loaded, "save_load_skips_distant_origin", "Chunks far outside the player's render radius should stay unloaded");
}

int main() {
    TestRunner tr;
    test_chunk_and_local_coords(tr);
    test_block_placement_and_updates(tr);
    test_light_propagation(tr);
    test_block_placement_collides_with_player(tr);
    test_collider_sweep(tr);
    test_hit_ray_finds_block(tr);
    test_save_load_centers_on_player(tr);
    return tr.report();
}
