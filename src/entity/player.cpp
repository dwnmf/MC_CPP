#include "player.h"
#include "../options.h"
#include <glm/gtc/matrix_transform.hpp>

// Константы скорости
const float WALKING_SPEED = 4.317f;
const float SPRINTING_SPEED = 7.0f;

Player::Player(World* w, Shader* s, float vw, float vh)
: Entity(w), shader(s), view_width(vw), view_height(vh), eyelevel(1.6), input(0), speed(4.3), target_speed(4.3) {}

void Player::handle_input_sprint(bool ctrl_pressed, bool forward_pressed) {
    // Логика как в оригинале:
    // Если нажали Ctrl и идем вперед -> включаем бег
    if (ctrl_pressed && forward_pressed) {
        is_sprinting = true;
    }
    // Если перестали идти вперед -> выключаем бег
    if (!forward_pressed) {
        is_sprinting = false;
    }

    target_speed = is_sprinting ? SPRINTING_SPEED : WALKING_SPEED;
}

void Player::update(float dt) {
    // Плавное изменение FOV или скорости
    if(dt * 20 > 1) speed = target_speed;
    else speed += (target_speed - speed) * dt * 20;

    float multiplier = speed;
    // В полете скорость выше
    if (flying) multiplier *= 2.0f;

    // Горизонтальное движение
    if(input.x || input.z) {
        // rotation.x = yaw (горизонтальный поворот)
        // input.z = вперед/назад, input.x = влево/вправо
        // -atan2(...) вычисляет угол вектора ввода
        float angle = rotation.x - atan2(input.z, input.x) + 1.57079f; // +PI/2

        accel.x = cos(angle) * multiplier;
        accel.z = sin(angle) * multiplier;
    }

    // Вертикальное движение (прыжок или полет)
    if (flying) {
        // В полете input.y управляет вертикальной скоростью напрямую через ускорение
        if (input.y != 0) {
            accel.y = input.y * multiplier;
        }
    } else {
        // Обычный режим: прыжок только если на земле и нажали пробел (input.y > 0)
        if(input.y > 0) jump();
    }

    Entity::update(dt);
}

void Player::update_matrices(float partial_ticks) {
    interpolated_position = glm::mix(old_position, position, partial_ticks);

    // Динамический FOV при беге
    float fov_mod = Options::FOV + 10.0f * (speed - WALKING_SPEED) / (SPRINTING_SPEED - WALKING_SPEED);

    p_matrix = glm::perspective(glm::radians(Options::FOV), view_width/view_height, 0.1f, 500.0f);
    mv_matrix = glm::mat4(1.0f);
    mv_matrix = glm::rotate(mv_matrix, rotation.y, glm::vec3(-1,0,0));
    mv_matrix = glm::rotate(mv_matrix, rotation.x + 1.57f, glm::vec3(0,1,0));

    // ДОБАВЛЕНО: + step_offset к высоте глаз
    mv_matrix = glm::translate(mv_matrix, -interpolated_position - glm::vec3(0, eyelevel + step_offset, 0));

    shader->setMat4(shader->find_uniform("u_MVPMatrix"), p_matrix * mv_matrix);
}

bool Player::check_in_frustum(glm::ivec3 chunk_pos) { return true; }
