#include "entity.h"
#include "../world.h"
#include "../renderer/block_type.h"
#include <cmath>
#include <algorithm>
#include <iostream>

// Константы из оригинального entity.py
// Python: GRAVITY_ACCEL = (0, -32, 0)
const glm::vec3 GRAVITY_ACCEL(0.0f, -32.0f, 0.0f);
const glm::vec3 FLYING_ACCEL(0.0f, 0.0f, 0.0f);

// Python: FRICTION = (20, 20, 20)
const glm::vec3 FRICTION(20.0f, 20.0f, 20.0f);

// Python: DRAG_FLY = (5, 5, 5)
const glm::vec3 DRAG_FLY(5.0f, 5.0f, 5.0f);

// Python: DRAG_JUMP = (1.8, 0, 1.8)
const glm::vec3 DRAG_JUMP(1.8f, 0.0f, 1.8f);

// Python: DRAG_FALL = (1.8, 0.4, 1.8)
const glm::vec3 DRAG_FALL(1.8f, 0.4f, 1.8f);

Entity::Entity(World* w) : world(w), position(0, 80, 0), old_position(0,80,0), rotation(-1.57, 0), velocity(0), accel(0) {}

void Entity::update_collider() {
    collider = Collider(position - glm::vec3(width/2, 0, width/2), position + glm::vec3(width/2, height, width/2));
}

void Entity::jump() {
    // Python: math.sqrt(2 * height * -GRAVITY_ACCEL[1])
    if(grounded) {
        float jump_height = 1.25f;
        velocity.y = sqrt(2 * jump_height * 32.0f);
    }
}

// Вспомогательная функция для выбора трения (как свойство friction в Python)
glm::vec3 get_friction_vector(bool flying, bool grounded, float vel_y) {
    if (flying) return DRAG_FLY;
    if (grounded) return FRICTION;
    if (vel_y > 0) return DRAG_JUMP;
    return DRAG_FALL;
}

void Entity::update(float dt) {
    old_position = position;

    // === ПЛАВНОЕ УМЕНЬШЕНИЕ СМЕЩЕНИЯ ШАГА ===
    // Если offset отрицательный (мы поднялись), стремимся к 0
    if (step_offset < 0.0f) {
        step_offset += dt * 8.0f; // Скорость подъема камеры (чем больше, тем быстрее)
        if (step_offset > 0.0f) step_offset = 0.0f;
    }
    // ========================================

    // 1. Apply input acceleration & friction
    // Python: self.velocity = [v + a * f * delta_time ...]
    glm::vec3 f_vec = get_friction_vector(flying, grounded, velocity.y);
    velocity += accel * f_vec * dt;
    accel = glm::vec3(0); // Сброс ускорения

    // 2. Compute collisions
    update_collider();
    grounded = false;

    // Python делает 3 прохода для разрешения коллизий по осям
    for(int i=0; i<3; i++) {
        glm::vec3 adj_vel = velocity * dt;

        // Broad-phase (поиск блоков вокруг)
        int step_x = (adj_vel.x > 0) ? 1 : -1;
        int step_y = (adj_vel.y > 0) ? 1 : -1;
        int step_z = (adj_vel.z > 0) ? 1 : -1;

        int steps_xz = (int)(width/2);
        int steps_y = (int)height;

        int x = (int)std::floor(position.x);
        int y = (int)std::floor(position.y);
        int z = (int)std::floor(position.z);

        int cx = (int)std::floor(position.x + adj_vel.x);
        int cy = (int)std::floor(position.y + adj_vel.y);
        int cz = (int)std::floor(position.z + adj_vel.z);

        // Ищем самое раннее время столкновения
        std::pair<float, glm::vec3> collision = {1.0f, glm::vec3(0)};

        for(int bx = x - step_x * (steps_xz + 1); bx != cx + step_x * (steps_xz + 2); bx += step_x) {
            for(int by = y - step_y * (steps_y + 2); by != cy + step_y * (steps_y + 3); by += step_y) {
                for(int bz = z - step_z * (steps_xz + 1); bz != cz + step_z * (steps_xz + 2); bz += step_z) {
                    int num = world->get_block_number({bx, by, bz});
                    if(!num) continue;
                    if(world->block_types[num] == nullptr) continue;

                    for(auto& col_offset : world->block_types[num]->colliders) {
                        // col_offset - это локальный коллайдер блока (0..1), добавляем позицию блока
                        auto res = collider.collide(col_offset + glm::vec3(bx, by, bz), adj_vel);
                        if(res.second != glm::vec3(0) && res.first < collision.first) {
                            collision = res;
                        }
                    }
                }
            }
        }

        // Применяем коллизию
        float entry = collision.first;
        glm::vec3 normal = collision.second;

        // В Python используется entry_time -= 0.001 (внутри цикла выбора, но здесь применим перед движением)
        // Это предотвращает застревание ровно в границе блока
        if (entry < 1.0f) {
            entry -= 0.001f;
            if (entry < 0) entry = 0;
        }

        // === STEP ASSIST С ПЛАВНОСТЬЮ ===
        bool horizontal_collision = (normal.x != 0 || normal.z != 0);
        if (grounded && horizontal_collision && !flying) {
            float step_height = 0.6f;
            Collider step_collider = collider + glm::vec3(adj_vel.x, step_height, adj_vel.z);

            bool blocked = false;
            for(int bx = x - 1; bx <= x + 1; bx++) {
                for(int by = y; by <= y + 2; by++) {
                    for(int bz = z - 1; bz <= z + 1; bz++) {
                        int num = world->get_block_number({bx, by, bz});
                        if (!num) continue;
                        for(auto& col_offset : world->block_types[num]->colliders) {
                            Collider block_col = col_offset + glm::vec3(bx, by, bz);
                            if (step_collider & block_col) {
                                blocked = true;
                                goto check_done;
                            }
                        }
                    }
                }
            }
            check_done:;

            if (!blocked) {
                position.y += step_height;

                // ВАЖНО: Компенсируем резкий скачок физики визуальным смещением
                step_offset -= step_height;

                // Хак: обновляем old_position, чтобы стандартная интерполяция
                // в Player::update_matrices не пыталась интерполировать этот скачок
                // (мы его обрабатываем сами через step_offset)
                old_position.y += step_height;

                update_collider();
                entry = 1.0f;
                normal = glm::vec3(0);
            }
        }
        // ==============================================

        if (normal.x != 0) {
            position.x += adj_vel.x * entry;
            velocity.x = 0;
        }
        if (normal.y != 0) {
            position.y += adj_vel.y * entry;
            velocity.y = 0;
        }
        if (normal.z != 0) {
            position.z += adj_vel.z * entry;
            velocity.z = 0;
        }

        if (normal.y == 1) grounded = true;
    }

    // 3. Apply velocity (remaining movement)
    // В Python: self.position = [x + v * delta_time ...]
    // Мы уже частично подвинули игрока в коллизии, но если коллизии не было, нужно подвинуть сейчас.
    // Но так как мы обнулили velocity при ударе, умножение на dt даст 0 приращения по заблокированной оси.
    position += velocity * dt;

    // 4. Apply Gravity
    // Python: self.velocity = [v + a * delta_time for v, a in zip(self.velocity, gravity)]
    glm::vec3 gravity = flying ? FLYING_ACCEL : GRAVITY_ACCEL;
    velocity += gravity * dt;

    // 5. Apply Decay (Friction/Drag part 2)
    // Python: self.velocity = [v - min(v * f * delta_time, v, key=abs) ...]
    f_vec = get_friction_vector(flying, grounded, velocity.y); // Получаем актуальное трение

    auto apply_decay = [&](float& v, float f, float dt) {
        float decay = v * f * dt;
        // min по модулю: если decay больше самой скорости, обнуляем скорость
        if (std::abs(decay) > std::abs(v)) {
            v = 0;
        } else {
            v -= decay;
        }
    };

    apply_decay(velocity.x, f_vec.x, dt);
    apply_decay(velocity.y, f_vec.y, dt);
    apply_decay(velocity.z, f_vec.z, dt);

    update_collider();
}
