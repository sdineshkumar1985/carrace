#ifndef ANDROIDGLINVESTIGATIONS_RENDERER_H
#define ANDROIDGLINVESTIGATIONS_RENDERER_H

#include <EGL/egl.h>
#include <memory>
#include <chrono>

#include "Model.h"
#include "Shader.h"
#include "ColorShader.h"
#include "Game.h"

struct android_app;

class Renderer {
public:
    inline Renderer(android_app *pApp) :
            app_(pApp),
            display_(EGL_NO_DISPLAY),
            surface_(EGL_NO_SURFACE),
            context_(EGL_NO_CONTEXT),
            width_(0),
            height_(0),
            shaderNeedsNewProjectionMatrix_(true),
            lastFrameTime_(std::chrono::steady_clock::now()) {
        initRenderer();
    }

    virtual ~Renderer();

    /*!
     * Handles input from the android_app.
     */
    void handleInput();

    /*!
     * Renders the game scene
     */
    void render();

private:
    void initRenderer();
    void updateRenderArea();
    void createModels();

    android_app *app_;
    EGLDisplay display_;
    EGLSurface surface_;
    EGLContext context_;
    EGLint width_;
    EGLint height_;

    bool shaderNeedsNewProjectionMatrix_;

    std::unique_ptr<Shader> shader_;
    std::unique_ptr<ColorShader> colorShader_;
    std::vector<Model> models_;
    Game game_;
    std::chrono::steady_clock::time_point lastFrameTime_;
};

#endif //ANDROIDGLINVESTIGATIONS_RENDERER_H
