#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>
#include <camera.h>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

void Camera::update()
{
    glm::mat4 cameraRotation = getRotationMatrix();
    position += glm::vec3(cameraRotation * glm::vec4(velocity * 0.5f, 0.f));
}

bool Camera::isMoving() {
    return glm::length(velocity) > 0.1 || rightClick;
}

void Camera::processSDLEvent(SDL_Event& e)
{
    if (e.type == SDL_EVENT_KEY_DOWN) {
        if (e.key.key == SDLK_W) { velocity.z = -1; }
        if (e.key.key== SDLK_S) { velocity.z = 1; }
        if (e.key.key== SDLK_A) { velocity.x = -1; }
        if (e.key.key== SDLK_D) { velocity.x = 1; }
    }

    if (e.type == SDL_EVENT_KEY_UP) {
        if (e.key.key== SDLK_W) { velocity.z = 0; }
        if (e.key.key== SDLK_S) { velocity.z = 0; }
        if (e.key.key== SDLK_A) { velocity.x = 0; }
        if (e.key.key== SDLK_D) { velocity.x = 0; }
    }

    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button .button== SDL_BUTTON_RIGHT){
        rightClick = true;
    }
    else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button .button== SDL_BUTTON_RIGHT) {
        rightClick = false;

    }
       

    if (e.type == SDL_EVENT_MOUSE_MOTION && rightClick) {
        yaw += (float)e.motion.xrel / 200.f;
        pitch -= (float)e.motion.yrel / 200.f;
    }


    
}

glm::mat4 Camera::getViewMatrix(glm::vec3 jitter)
{
    glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), position + jitter);
    glm::mat4 cameraRotation = getRotationMatrix();
    return glm::inverse(cameraTranslation * cameraRotation);
}

glm::mat4 Camera::getRotationMatrix()
{

    glm::quat pitchRotation = glm::angleAxis(pitch, glm::vec3 { 1.f, 0.f, 0.f });
    glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3 { 0.f, -1.f, 0.f });

    return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}

glm::mat4 perspective_opengl_rh(
    const float fovy, const float aspect, const float n, const float f)
{
    const float e = 1.0f / std::tan(fovy * 0.5f);
    return { e / aspect, 0.0f,  0.0f,                    0.0f,
            0.0f,       e,     0.0f,                    0.0f,
            0.0f,       0.0f, (f + n) / (n - f),       -1.0f,
            0.0f,       0.0f, (2.0f * f * n) / (n - f), 0.0f };
}


glm::mat4 perspective_vulkan_rh(
    const float fovy, const float aspect, const float n, const float f)
{

    glm::mat4 opengl_perspective = perspective_opengl_rh(fovy, aspect, n, f);



    constexpr glm::mat4 normalize_range{ 1.0f, 0.0f, 0.0f, 0.0f,
                                      0.0f, 1.0f, 0.0f, 0.0f,
                                      0.0f, 0.0f, 0.5f, 0.0f,
                                      0.0f, 0.0f, 0.5f, 1.0f };

    constexpr glm::mat4 reverse_z{ 1.0f, 0.0f,  0.0f, 0.0f,
                              0.0f, 1.0f,  0.0f, 0.0f,
                              0.0f, 0.0f, -1.0f, 0.0f,
                              0.0f, 0.0f,  1.0f, 1.0f };

    // 4) Flip Y for Vulkan’s top‑left origin:
    constexpr glm::mat4 flip_y{
        1.0f,  0.0f, 0.0f, 0.0f,
        0.0f, -1.0f, 0.0f, 0.0f,
        0.0f,  0.0f, 1.0f, 0.0f,
        0.0f,  0.0f, 0.0f, 1.0f
    }; 

    return flip_y * reverse_z * normalize_range * opengl_perspective;
}



glm::mat4 Camera::getProjectionMatrix(bool raytracing) {


    
    if (isPerspective) {
        glm::mat4 perspective = perspective_vulkan_rh(fov, aspectRatio, zNear, zFar);
        return perspective;
    }
    else if (isOrtographic) {
        glm::mat4 ortho = glm::orthoRH_ZO(-xMag, xMag, -yMag, yMag, zFar, zNear);
        ortho[1, 1] *= -1.0f;

        return ortho;
    }
}