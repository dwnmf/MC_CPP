#include "player.h"
#include "../options.h"
#include "../chunk/chunk.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
#include <algorithm>

// Константы скорости
const float WALKING_SPEED = 4.317f;
const float SPRINTING_SPEED = 7.0f;

Player::Player(World* w, Shader* s, float vw, float vh)
: Entity(w), shader(s), view_width(vw), view_height(vh), eyelevel(1.6f), input(0), near_plane(0.1f), far_plane(500.0f), speed(4.3f), target_speed(4.3f), is_sprinting(false), max_health(10.0f), health(10.0f) {}

float Player::get_current_fov() const {
    float denom = (SPRINTING_SPEED - WALKING_SPEED);
    float factor = 0.0f;
    if (denom > 0.0001f) {
        factor = (speed - WALKING_SPEED) / denom;
    }
    factor = std::clamp(factor, 0.0f, 1.0f);
    return Options::FOV + 10.0f * factor;
}

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

void Player::heal(float amount) {
    health = std::clamp(health + amount, 0.0f, max_health);
}

void Player::take_damage(float amount) {
    health = std::clamp(health - amount, 0.0f, max_health);
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
    float fov_mod = get_current_fov();

    p_matrix = glm::perspective(glm::radians(fov_mod), view_width/view_height, near_plane, far_plane);
    mv_matrix = glm::mat4(1.0f);
    mv_matrix = glm::rotate(mv_matrix, rotation.y, glm::vec3(-1,0,0));
    mv_matrix = glm::rotate(mv_matrix, rotation.x + 1.57f, glm::vec3(0,1,0));

    // ДОБАВЛЕНО: + step_offset к высоте глаз
    mv_matrix = glm::translate(mv_matrix, -interpolated_position - glm::vec3(0, eyelevel + step_offset, 0));

    vp_matrix = p_matrix * mv_matrix;
    if (shader && shader->valid()) {
        int mvpLoc = shader->find_uniform("u_MVPMatrix");
        int viewLoc = shader->find_uniform("u_ViewMatrix");
        shader->setMat4(mvpLoc, vp_matrix);
        if (viewLoc >= 0) shader->setMat4(viewLoc, mv_matrix);
    }
}

bool Player::check_in_frustum(glm::ivec3 chunk_pos) {
    glm::vec3 chunk_min = glm::vec3(chunk_pos.x * CHUNK_WIDTH, chunk_pos.y * CHUNK_HEIGHT, chunk_pos.z * CHUNK_LENGTH);
    glm::vec3 chunk_max = chunk_min + glm::vec3(CHUNK_WIDTH, CHUNK_HEIGHT, CHUNK_LENGTH);
    glm::vec3 center = (chunk_min + chunk_max) * 0.5f;

    float max_dist = static_cast<float>(Options::RENDER_DISTANCE * CHUNK_WIDTH);
    if (glm::length2(center - position) > max_dist * max_dist) return false;

    glm::vec4 corners[8] = {
        {chunk_min.x, chunk_min.y, chunk_min.z, 1.0f},
        {chunk_max.x, chunk_min.y, chunk_min.z, 1.0f},
        {chunk_min.x, chunk_max.y, chunk_min.z, 1.0f},
        {chunk_max.x, chunk_max.y, chunk_min.z, 1.0f},
        {chunk_min.x, chunk_min.y, chunk_max.z, 1.0f},
        {chunk_max.x, chunk_min.y, chunk_max.z, 1.0f},
        {chunk_min.x, chunk_max.y, chunk_max.z, 1.0f},
        {chunk_max.x, chunk_max.y, chunk_max.z, 1.0f}
    };

    int outside_left = 0, outside_right = 0, outside_bottom = 0, outside_top = 0, outside_near = 0, outside_far = 0;
    for (auto& corner : corners) {
        glm::vec4 clip = vp_matrix * corner;
        float w = clip.w;
        if (w <= 0.0f) { outside_near++; continue; } // за ближней плоскостью

        if (clip.x < -w) outside_left++;
        if (clip.x >  w) outside_right++;
        if (clip.y < -w) outside_bottom++;
        if (clip.y >  w) outside_top++;
        if (clip.z < -w) outside_near++;
        if (clip.z >  w) outside_far++;
    }

    if (outside_left == 8 || outside_right == 8 ||
        outside_bottom == 8 || outside_top == 8 ||
        outside_near == 8 || outside_far == 8) {
        return false;
    }
    return true;
}
