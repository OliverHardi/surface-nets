#include <iostream>

#include "glm/glm.hpp"
#include "glad/glad.h"
#include "GLFW/glfw3.h"

#include "Shader.h"
#include "Camera.h"
#include "Frustum.h"
#include "World.h"



Camera camera;
void mouseCallback(GLFWwindow*, double xpos, double ypos) { camera.processMouse(xpos, ypos); }

glm::mat4 projection = glm::perspective(glm::radians(70.0f), 800.0f/600.0f, 0.1f, 300.0f);
glm::mat4 view = camera.getViewMatrix();
glm::mat4 model = glm::mat4(1.0f);

int main() {
    // camera.position = glm::vec3(16.0f, 16.0f, 50.0f);
    camera.position = glm::vec3(0.0f, 0.0f, 0.0f);
    
    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // required on macOS

    GLFWwindow* window = glfwCreateWindow(800, 600, "GLFW Test", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);

    if (glfwRawMouseMotionSupported())
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD\n";
        return -1;
    }

    Shader simpleShader("shaders/simple.vert",
                "shaders/simple.frag");

    // Chunk chunk;
    // chunk.generateVoxels();
    // chunk.buildMesh();
    // chunk.uploadMesh();

    Frustum frustum;

    World world;


    glEnable(GL_DEPTH_TEST);
    // glDisable(GL_DEPTH_TEST);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    
    // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    
    // glEnable(GL_BLEND);
    // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouseCallback);

    float lastFrame = glfwGetTime();

    simpleShader.use();
    GLint projLoc  = glGetUniformLocation(simpleShader.id(), "projection");
    GLint viewLoc  = glGetUniformLocation(simpleShader.id(), "view");
    GLint modelLoc = glGetUniformLocation(simpleShader.id(), "model");


    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        float current = glfwGetTime();
        float dt = current - lastFrame;
        lastFrame = current;

        camera.processKeyboard(window, dt);
        view = camera.getViewMatrix();

        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        simpleShader.use();
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, &projection[0][0]);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &view[0][0]);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);

        // chunk.draw();

        frustum.extract(projection * view);

        world.update(camera.position, frustum);
        
        world.draw(simpleShader, frustum);

        glfwSwapBuffers(window);

    }


    glfwTerminate();
    return 0;
}

/*

TODO:

switch to flat array chunks - done

fix meshing to use neighboring chunks for the edges

use a thread pool for chunk generation and meshing



*/