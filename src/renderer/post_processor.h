#pragma once
#include <glad/glad.h>
#include <iostream>
#include <glm/vec2.hpp>
#include "shader.h"

class PostProcessor {
public:
    unsigned int FBO = 0;
    unsigned int textureColorBuffer = 0;
    unsigned int depthTexture = 0;
    unsigned int quadVAO = 0, quadVBO = 0;
    Shader* postShader = nullptr;
    int width = 0, height = 0;
    bool valid = false;

    PostProcessor(int w, int h) : width(w), height(h) {
        postShader = new Shader("assets/shaders/post/vert.glsl", "assets/shaders/post/frag.glsl");
        if (postShader && postShader->valid()) {
            initFramebuffer();
            initQuad();
        } else {
            valid = false;
        }
    }

    ~PostProcessor() {
        if (quadVBO) glDeleteBuffers(1, &quadVBO);
        if (quadVAO) glDeleteVertexArrays(1, &quadVAO);
        if (textureColorBuffer) glDeleteTextures(1, &textureColorBuffer);
        if (depthTexture) glDeleteTextures(1, &depthTexture);
        if (FBO) glDeleteFramebuffers(1, &FBO);
        delete postShader;
    }

    void resize(int w, int h) {
        if (!valid) return;
        width = w; height = h;
        glBindTexture(GL_TEXTURE_2D, textureColorBuffer);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
        glBindTexture(GL_TEXTURE_2D, depthTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    }

    void beginRender() {
        if (valid) glBindFramebuffer(GL_FRAMEBUFFER, FBO);
        else glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void endRenderAndDraw(bool isUnderwater, float time) {
        if (!valid) return; // already rendered to default framebuffer

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);

        postShader->use();
        postShader->setInt(postShader->find_uniform("screenTexture"), 0);
        postShader->setInt(postShader->find_uniform("depthTexture"), 1);
        postShader->setInt(postShader->find_uniform("u_IsUnderwater"), isUnderwater ? 1 : 0);
        postShader->setFloat(postShader->find_uniform("u_Time"), time);
        postShader->setVec2(postShader->find_uniform("u_ScreenSize"), glm::vec2(width, height));
        postShader->setFloat(postShader->find_uniform("u_SSAOStrength"), 1.0f);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureColorBuffer);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, depthTexture);

        glBindVertexArray(quadVAO);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
    }

private:
    void initFramebuffer() {
        glGenFramebuffers(1, &FBO);
        glBindFramebuffer(GL_FRAMEBUFFER, FBO);

        glGenTextures(1, &textureColorBuffer);
        glBindTexture(GL_TEXTURE_2D, textureColorBuffer);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureColorBuffer, 0);

        glGenTextures(1, &depthTexture);
        glBindTexture(GL_TEXTURE_2D, depthTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);

        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glReadBuffer(GL_COLOR_ATTACHMENT0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
            valid = false;
        } else {
            valid = true;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void initQuad() {
        float quadVertices[] = {
            // positions   // texCoords
            -1.0f,  1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
             1.0f, -1.0f,  1.0f, 0.0f,

            -1.0f,  1.0f,  0.0f, 1.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
             1.0f,  1.0f,  1.0f, 1.0f
        };
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    }
};
