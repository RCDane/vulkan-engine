#include <SDL_events.h>
#include <SDL_mouse.h>
#include <camera.h>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

void Camera::update()
{
    glm::mat4 cameraRotation = getRotationMatrix();
    position += glm::vec3(cameraRotation * glm::vec4(velocity * 0.5f, 0.f));
}

void Camera::processSDLEvent(SDL_Event& e)
{
    if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_w) { velocity.z = -1; }
        if (e.key.keysym.sym == SDLK_s) { velocity.z = 1; }
        if (e.key.keysym.sym == SDLK_a) { velocity.x = -1; }
        if (e.key.keysym.sym == SDLK_d) { velocity.x = 1; }
    }

    if (e.type == SDL_KEYUP) {
        if (e.key.keysym.sym == SDLK_w) { velocity.z = 0; }
        if (e.key.keysym.sym == SDLK_s) { velocity.z = 0; }
        if (e.key.keysym.sym == SDLK_a) { velocity.x = 0; }
        if (e.key.keysym.sym == SDLK_d) { velocity.x = 0; }
    }

    if (e.type == SDL_MOUSEBUTTONDOWN && e.button .button== SDL_BUTTON_RIGHT){
        rightClick = true;
    }
    else if (e.type == SDL_MOUSEBUTTONUP && e.button .button== SDL_BUTTON_RIGHT) {
        rightClick = false;
    }

    if (e.type == SDL_MOUSEMOTION && rightClick) {
        yaw += (float)e.motion.xrel / 200.f;
        pitch -= (float)e.motion.yrel / 200.f;
    }
    
}

glm::mat4 Camera::getViewMatrix()
{
    glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), position);
    glm::mat4 cameraRotation = getRotationMatrix();
    return glm::inverse(cameraTranslation * cameraRotation);
}

glm::mat4 Camera::getRotationMatrix()
{

    glm::quat pitchRotation = glm::angleAxis(pitch, glm::vec3 { 1.f, 0.f, 0.f });
    glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3 { 0.f, -1.f, 0.f });

    return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}

glm::mat4 Camera::getProjectionMatrix(bool raytracing) {
    float Near = raytracing ? zNear : zFar;
    float Far = raytracing ? zFar: zNear;

    
    if (isPerspective) {
        glm::mat4 perspective = glm::perspective(fov, aspectRatio, Near, Far);
        perspective[1, 1] *= -1.0f;
        return perspective;
    }
    else if (isOrtographic) {
        glm::mat4 ortho = glm::orthoRH_ZO(-xMag, xMag, -yMag, yMag, Near, Far);
        ortho[1, 1] *= -1.0f;

        return ortho;
    }
}