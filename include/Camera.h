#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>

struct Camera {
    glm::vec3 position{0.0f, 0.0f, 3.0f};
    float yaw = -90.0f, pitch = 0.0f;
    float lastX = 400, lastY = 300;
    bool firstMouse = true;

    glm::vec3 getFront() const {
        glm::vec3 f;
        f.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        f.y = sin(glm::radians(pitch));
        f.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        return glm::normalize(f);
    }

    glm::mat4 getViewMatrix() const {
        return glm::lookAt(position, position + getFront(), glm::vec3(0,1,0));
    }

    void processMouse(double xpos, double ypos) {
        if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
        float xoffset = (xpos - lastX) * 0.1f;
        float yoffset = (lastY - ypos) * 0.1f; // reversed: screen y is top-down
        lastX = xpos; lastY = ypos;
        yaw += xoffset;
        pitch = glm::clamp(pitch + yoffset, -89.0f, 89.0f);
    }

    void processKeyboard(GLFWwindow* window, float dt) {
        float speed = 40.0f * dt;

        glm::vec3 forward{
            cos(glm::radians(yaw)),
            0.0f,
            sin(glm::radians(yaw))
        };
        forward = glm::normalize(forward);

        glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) position += forward * speed;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) position -= forward * speed;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) position -= right * speed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) position += right * speed;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) position.y += speed;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) position.y -= speed;
    }
};
