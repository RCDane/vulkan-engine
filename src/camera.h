#pragma once
#include <vk_types.h>
#include <SDL3/SDL_events.h>



class Camera {
public:
    glm::vec3 velocity = glm::vec3(0.0f);
    glm::vec3 position = glm::vec3(0.0f);
    glm::mat4 transform = glm::mat4(1.0f);

    bool isPerspective = false;
    bool isOrtographic = false;

    float zFar;
    float zNear;
    float fov;
    float aspectRatio;

    float xMag;
    float yMag;

    bool cameraMovedThisFrame;
    bool rightClick = false;
    float pitch {0.f};
    float yaw {0.f};


    glm::mat4 getViewMatrix(glm::vec3 jitter = glm::vec3(0));
    glm::mat4 getRotationMatrix();
    glm::mat4 getProjectionMatrix();
    bool isMoving();
    void processSDLEvent(SDL_Event& e);

    void update();
};
