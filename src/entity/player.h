#pragma once
#include "entity.h"
#include "../renderer/shader.h"

class Player : public Entity {
public:
    float view_width, view_height;
    glm::mat4 p_matrix, mv_matrix;
    Shader* shader;
    float eyelevel;
    glm::vec3 input;

    float speed;
    float target_speed;
    bool is_sprinting;

    glm::vec3 interpolated_position;

    Player(World* w, Shader* s, float vw, float vh);
    void update(float dt) override;
    void update_matrices(float partial_ticks); // Interpolation
    bool check_in_frustum(glm::ivec3 chunk_pos);
    void handle_input_sprint(bool ctrl_pressed, bool forward_pressed);
};
