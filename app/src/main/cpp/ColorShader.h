#ifndef PIXEL_TURBO_RACER_COLOR_SHADER_H
#define PIXEL_TURBO_RACER_COLOR_SHADER_H

#include <string>
#include <GLES3/gl3.h>

class ColorShader {
public:
    static ColorShader *loadShader(
            const std::string &vertexSource,
            const std::string &fragmentSource,
            const std::string &positionAttributeName,
            const std::string &projectionMatrixUniformName,
            const std::string &colorUniformName);

    inline ~ColorShader() {
        if (program_) {
            glDeleteProgram(program_);
            program_ = 0;
        }
    }

    void activate() const;
    void deactivate() const;
    void setProjectionMatrix(float *matrix) const;
    void setColor(float r, float g, float b, float a) const;
    void drawQuad(float x, float y, float width, float height) const;

private:
    static GLuint loadShader(GLenum shaderType, const std::string &shaderSource);

    constexpr ColorShader(
            GLuint program,
            GLint position,
            GLint projectionMatrix,
            GLint color)
            : program_(program),
              position_(position),
              projectionMatrix_(projectionMatrix),
              color_(color) {}

    GLuint program_;
    GLint position_;
    GLint projectionMatrix_;
    GLint color_;
};

#endif //PIXEL_TURBO_RACER_COLOR_SHADER_H
