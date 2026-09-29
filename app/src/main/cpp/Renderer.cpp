#include "Renderer.h"

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <GLES3/gl3.h>
#include <memory>
#include <vector>
#include <android/imagedecoder.h>
#include <cmath>

#include "AndroidOut.h"
#include "Shader.h"
#include "ColorShader.h"
#include "Utility.h"
#include "TextureAsset.h"

#define PRINT_GL_STRING(s) {aout << #s": "<< glGetString(s) << std::endl;}

static const char *vertex = R"vertex(#version 300 es
in vec3 inPosition;
in vec2 inUV;
out vec2 fragUV;
uniform mat4 uProjection;
void main() {
    fragUV = inUV;
    gl_Position = uProjection * vec4(inPosition, 1.0);
}
)vertex";

static const char *fragment = R"fragment(#version 300 es
precision mediump float;
in vec2 fragUV;
uniform sampler2D uTexture;
out vec4 outColor;
void main() {
    outColor = texture(uTexture, fragUV);
}
)fragment";

static const char *colorVertexShader = R"vertex(#version 300 es
in vec3 inPosition;
uniform mat4 uProjection;
void main() {
    gl_Position = uProjection * vec4(inPosition, 1.0);
}
)vertex";

static const char *colorFragmentShader = R"fragment(#version 300 es
precision mediump float;
uniform vec4 uColor;
out vec4 outColor;
void main() {
    outColor = uColor;
}
)fragment";

static constexpr float kProjectionHalfHeight = 2.f;
static constexpr float kProjectionNearPlane = -1.f;
static constexpr float kProjectionFarPlane = 1.f;

Renderer::~Renderer() {
    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context_ != EGL_NO_CONTEXT) {
            eglDestroyContext(display_, context_);
            context_ = EGL_NO_CONTEXT;
        }
        if (surface_ != EGL_NO_SURFACE) {
            eglDestroySurface(display_, surface_);
            surface_ = EGL_NO_SURFACE;
        }
        eglTerminate(display_);
        display_ = EGL_NO_DISPLAY;
    }
}

void Renderer::render() {
    updateRenderArea();

    auto currentTime = std::chrono::steady_clock::now();
    std::chrono::duration<float> elapsed = currentTime - lastFrameTime_;
    lastFrameTime_ = currentTime;
    float deltaTime = elapsed.count();
    if (deltaTime > 0.1f) deltaTime = 0.1f; // cap max delta time

    game_.update(deltaTime);

    float projectionMatrix[16] = {0};
    Utility::buildOrthographicMatrix(
            projectionMatrix,
            kProjectionHalfHeight,
            float(width_) / height_,
            kProjectionNearPlane,
            kProjectionFarPlane);

    if (shaderNeedsNewProjectionMatrix_) {
        shader_->setProjectionMatrix(projectionMatrix);
        colorShader_->setProjectionMatrix(projectionMatrix);
        shaderNeedsNewProjectionMatrix_ = false;
    }

    glClearColor(0.12f, 0.12f, 0.16f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Render using ColorShader
    colorShader_->activate();
    colorShader_->setProjectionMatrix(projectionMatrix);

    // 1. Grass / Background (Green)
    colorShader_->setColor(0.15f, 0.5f, 0.2f, 1.0f);
    colorShader_->drawQuad(0.0f, 0.0f, 3.6f, 5.0f);

    // 2. Road (Dark Asphalt Gray)
    colorShader_->setColor(0.22f, 0.22f, 0.26f, 1.0f);
    colorShader_->drawQuad(0.0f, 0.0f, 1.8f, 5.0f);

    // 3. Road Shoulder Stripes (Yellow/White borders)
    colorShader_->setColor(0.9f, 0.9f, 0.9f, 1.0f);
    colorShader_->drawQuad(-0.92f, 0.0f, 0.06f, 5.0f);
    colorShader_->drawQuad(0.92f, 0.0f, 0.06f, 5.0f);

    // 4. Scrolling Lane Dividers
    colorShader_->setColor(0.95f, 0.95f, 0.2f, 1.0f);
    float scroll = game_.getRoadScroll();
    for (float y = -2.5f + std::fmod(scroll, 0.8f); y < 2.5f; y += 0.8f) {
        colorShader_->drawQuad(-0.3f, y, 0.04f, 0.35f);
        colorShader_->drawQuad(0.3f, y, 0.04f, 0.35f);
    }

    // 5. Enemy Cars
    for (const auto &enemy : game_.getEnemies()) {
        // Enemy car body
        colorShader_->setColor(enemy.r, enemy.g, enemy.b, 1.0f);
        colorShader_->drawQuad(enemy.x, enemy.y, enemy.width, enemy.height);

        // Enemy windshield
        colorShader_->setColor(0.1f, 0.1f, 0.1f, 1.0f);
        colorShader_->drawQuad(enemy.x, enemy.y + 0.08f, enemy.width * 0.7f, enemy.height * 0.3f);

        // Enemy wheels
        colorShader_->setColor(0.05f, 0.05f, 0.05f, 1.0f);
        colorShader_->drawQuad(enemy.x - enemy.width * 0.52f, enemy.y + 0.1f, 0.05f, 0.15f);
        colorShader_->drawQuad(enemy.x + enemy.width * 0.52f, enemy.y + 0.1f, 0.05f, 0.15f);
        colorShader_->drawQuad(enemy.x - enemy.width * 0.52f, enemy.y - 0.1f, 0.05f, 0.15f);
        colorShader_->drawQuad(enemy.x + enemy.width * 0.52f, enemy.y - 0.1f, 0.05f, 0.15f);
    }

    // 6. Player Car
    float pX = game_.getPlayerX();
    float pY = game_.getPlayerY();
    float pW = game_.getPlayerWidth();
    float pH = game_.getPlayerHeight();

    // Player body (Bright Red Sports Car)
    colorShader_->setColor(0.95f, 0.15f, 0.15f, 1.0f);
    colorShader_->drawQuad(pX, pY, pW, pH);

    // Player roof / windshield
    colorShader_->setColor(0.15f, 0.15f, 0.2f, 1.0f);
    colorShader_->drawQuad(pX, pY - 0.02f, pW * 0.72f, pH * 0.35f);

    // Player headlights (Yellow)
    colorShader_->setColor(0.95f, 0.95f, 0.2f, 1.0f);
    colorShader_->drawQuad(pX - pW * 0.35f, pY - pH * 0.46f, 0.05f, 0.04f);
    colorShader_->drawQuad(pX + pW * 0.35f, pY - pH * 0.46f, 0.05f, 0.04f);

    // Player wheels
    colorShader_->setColor(0.05f, 0.05f, 0.05f, 1.0f);
    colorShader_->drawQuad(pX - pW * 0.52f, pY + 0.12f, 0.05f, 0.16f);
    colorShader_->drawQuad(pX + pW * 0.52f, pY + 0.12f, 0.05f, 0.16f);
    colorShader_->drawQuad(pX - pW * 0.52f, pY - 0.12f, 0.05f, 0.16f);
    colorShader_->drawQuad(pX + pW * 0.52f, pY - 0.12f, 0.05f, 0.16f);

    // 7. Start / Game Over UI Overlays (Drawn as stylish HUD panels)
    if (game_.getState() == GameState::START) {
        // Semi-transparent overlay box
        colorShader_->setColor(0.0f, 0.0f, 0.0f, 0.75f);
        colorShader_->drawQuad(0.0f, 0.0f, 1.6f, 1.2f);

        // Title accent bars
        colorShader_->setColor(0.95f, 0.8f, 0.1f, 1.0f);
        colorShader_->drawQuad(0.0f, 0.35f, 1.4f, 0.08f);
        colorShader_->drawQuad(0.0f, -0.25f, 1.2f, 0.06f);
    } else if (game_.getState() == GameState::GAMEOVER) {
        // Game Over panel
        colorShader_->setColor(0.8f, 0.1f, 0.1f, 0.85f);
        colorShader_->drawQuad(0.0f, 0.0f, 1.7f, 1.3f);

        colorShader_->setColor(0.95f, 0.95f, 0.95f, 1.0f);
        colorShader_->drawQuad(0.0f, -0.3f, 1.3f, 0.06f);
    }

    colorShader_->deactivate();

    auto swapResult = eglSwapBuffers(display_, surface_);
    assert(swapResult == EGL_TRUE);
}

void Renderer::initRenderer() {
    constexpr EGLint attribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_DEPTH_SIZE, 24,
            EGL_NONE
    };

    auto display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, nullptr, nullptr);

    EGLint numConfigs;
    eglChooseConfig(display, attribs, nullptr, 0, &numConfigs);

    std::unique_ptr<EGLConfig[]> supportedConfigs(new EGLConfig[numConfigs]);
    eglChooseConfig(display, attribs, supportedConfigs.get(), numConfigs, &numConfigs);

    auto config = *std::find_if(
            supportedConfigs.get(),
            supportedConfigs.get() + numConfigs,
            [&display](const EGLConfig &config) {
                EGLint red, green, blue, depth;
                if (eglGetConfigAttrib(display, config, EGL_RED_SIZE, &red)
                    && eglGetConfigAttrib(display, config, EGL_GREEN_SIZE, &green)
                    && eglGetConfigAttrib(display, config, EGL_BLUE_SIZE, &blue)
                    && eglGetConfigAttrib(display, config, EGL_DEPTH_SIZE, &depth)) {
                    return red == 8 && green == 8 && blue == 8 && depth == 24;
                }
                return false;
            });

    EGLint format;
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
    EGLSurface surface = eglCreateWindowSurface(display, config, app_->window, nullptr);

    EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    EGLContext context = eglCreateContext(display, config, nullptr, contextAttribs);

    auto madeCurrent = eglMakeCurrent(display, surface, surface, context);
    assert(madeCurrent);

    display_ = display;
    surface_ = surface;
    context_ = context;

    width_ = -1;
    height_ = -1;

    shader_ = std::unique_ptr<Shader>(
            Shader::loadShader(vertex, fragment, "inPosition", "inUV", "uProjection"));
    assert(shader_);

    colorShader_ = std::unique_ptr<ColorShader>(
            ColorShader::loadShader(colorVertexShader, colorFragmentShader, "inPosition", "uProjection", "uColor"));
    assert(colorShader_);

    shader_->activate();

    glClearColor(0.12f, 0.12f, 0.16f, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    createModels();
}

void Renderer::updateRenderArea() {
    EGLint width;
    eglQuerySurface(display_, surface_, EGL_WIDTH, &width);

    EGLint height;
    eglQuerySurface(display_, surface_, EGL_HEIGHT, &height);

    if (width != width_ || height != height_) {
        width_ = width;
        height_ = height;
        glViewport(0, 0, width, height);
        shaderNeedsNewProjectionMatrix_ = true;
    }
}

void Renderer::createModels() {
    std::vector<Vertex> vertices = {
            Vertex(Vector3{1, 1, 0}, Vector2{0, 0}),
            Vertex(Vector3{-1, 1, 0}, Vector2{1, 0}),
            Vertex(Vector3{-1, -1, 0}, Vector2{1, 1}),
            Vertex(Vector3{1, -1, 0}, Vector2{0, 1})
    };
    std::vector<Index> indices = {
            0, 1, 2, 0, 2, 3
    };
    auto assetManager = app_->activity->assetManager;
    auto spAndroidRobotTexture = TextureAsset::loadAsset(assetManager, "android_robot.png");
    models_.emplace_back(vertices, indices, spAndroidRobotTexture);
}

void Renderer::handleInput() {
    auto *inputBuffer = android_app_swap_input_buffers(app_);
    if (!inputBuffer) {
        return;
    }

    bool actionTriggered = false;
    float steerInput = 0.0f;
    float moveInput = 0.0f;

    for (auto i = 0; i < inputBuffer->motionEventsCount; i++) {
        auto &motionEvent = inputBuffer->motionEvents[i];
        auto action = motionEvent.action;

        switch (action & AMOTION_EVENT_ACTION_MASK) {
            case AMOTION_EVENT_ACTION_DOWN:
            case AMOTION_EVENT_ACTION_POINTER_DOWN:
            case AMOTION_EVENT_ACTION_MOVE: {
                actionTriggered = true;
                for (auto pIndex = 0; pIndex < motionEvent.pointerCount; pIndex++) {
                    auto &pointer = motionEvent.pointers[pIndex];
                    auto x = GameActivityPointerAxes_getX(&pointer);
                    auto y = GameActivityPointerAxes_getY(&pointer);

                    if (width_ > 0 && height_ > 0) {
                        float aspect = float(width_) / height_;
                        float worldX = ((x / width_) - 0.5f) * (2.0f * kProjectionHalfHeight * aspect);
                        float worldY = (0.5f - (y / height_)) * (2.0f * kProjectionHalfHeight);
                        game_.setPosition(worldX, worldY);
                    }
                }
                break;
            }
            default:
                break;
        }
    }
    android_app_clear_motion_events(inputBuffer);

    // Handle key events (keyboard / controller)
    for (auto i = 0; i < inputBuffer->keyEventsCount; i++) {
        auto &keyEvent = inputBuffer->keyEvents[i];
        if (keyEvent.action == AKEY_EVENT_ACTION_DOWN) {
            actionTriggered = true;
            if (keyEvent.keyCode == AKEYCODE_DPAD_LEFT || keyEvent.keyCode == AKEYCODE_A) {
                steerInput = -1.0f;
            } else if (keyEvent.keyCode == AKEYCODE_DPAD_RIGHT || keyEvent.keyCode == AKEYCODE_D) {
                steerInput = 1.0f;
            } else if (keyEvent.keyCode == AKEYCODE_DPAD_UP || keyEvent.keyCode == AKEYCODE_W) {
                moveInput = 1.0f;
            } else if (keyEvent.keyCode == AKEYCODE_DPAD_DOWN || keyEvent.keyCode == AKEYCODE_S) {
                moveInput = -1.0f;
            }
        }
    }
    android_app_clear_key_events(inputBuffer);

    game_.handleInput(steerInput, moveInput, actionTriggered);
}
