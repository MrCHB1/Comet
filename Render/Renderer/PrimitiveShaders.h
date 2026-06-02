#pragma once
#include "Render/RenderEngine/Shaders.h"
#include <memory>

// colored rect
inline const char* primVert = "#version 330\n"
"layout (location = 0) in vec2 aPos;\n"
"layout (location = 1) in vec2 aUv;\n"
"uniform mat4 model;\n"
"out vec2 uv;\n"
"void main() {\n"
"    uv = aUv;\n"
"    gl_Position = model * vec4(aPos, 0.0, 1.0);\n"
"    gl_Position.xy = gl_Position.xy * 2.0 - 1.0;\n"
"}";

inline const char* primFrag = "#version 330\n"
"in vec2 uv;\n"
"out vec4 fragColor;\n"
"uniform vec3 color;\n"
"void main() {\n"
"    fragColor = vec4(color, 1.0);\n"
"}";

inline std::shared_ptr<ShaderProgram> SINGLE_COLOR_SHADER;

// rect that draws the blurred version of whatever is behind it
inline const char* primBlurVert = "#version 330\n"
"layout (location = 0) in vec2 aPos;\n"
"layout (location = 1) in vec2 aUv;\n"
"uniform mat4 model;\n"
"out vec2 uv;\n"
"void main() {\n"
"    vec2 firstCorner = (model * vec4(0.0)).xy;\n"
"    vec2 lastCorner = (model * vec4(1.0)).xy;\n"
"    gl_Position = model * vec4(aPos, 0.0, 1.0);\n"
"    // because this draws whatever is behind it, we can just use the primitive's vertex positions as the uvs, that way they match up with the scene\n"
"    uv = gl_Position.xy;\n"
"    gl_Position.xy = gl_Position.xy * 2.0 - 1.0;\n"
"}";

inline const char* primBlurFrag = "#version 330\n"
"in vec2 uv;\n"
"out vec4 fragColor;\n"
"uniform sampler2D scene;\n"
"uniform float width;\n"
"uniform float height;\n"
"const float cornerRadiusNorm = 0.1f;\n"
"const float BLUR_SIZE = 1.5f;\n"
"vec3 SampleBlurred(vec2 uv) {\n"
"    vec3 color = vec3(0.0);\n"
"    float totalWeight = 0.0;\n"
"    for (int x = -8; x <= 8; ++x) {\n"
"        for (int y = -8; y <= 8; ++y) {\n"
"            float weight = 1.0 - length(vec2(x, y)) / 11.313708499;\n"
"            color += texture(scene, uv + vec2(x / width, y / height) * BLUR_SIZE).rgb * weight;\n"
"            totalWeight += weight;\n"
"        }\n"
"    }\n"
"    return color / totalWeight;\n"
"}\n"
"void main() {\n"
"    fragColor = vec4(SampleBlurred(uv), 1.0);\n"
"}";

inline std::shared_ptr<ShaderProgram> BLUR_SHADER;

// rect that just draws the scene, nothing special
inline const char* primSceneVert = "#version 330\n"
"layout (location = 0) in vec2 aPos;\n"
"layout (location = 1) in vec2 aUv;\n"
"uniform mat4 model;\n"
"out vec2 uv;\n"
"void main() {\n"
"    uv = aUv;\n"
"    gl_Position = model * vec4(aPos, 0.0, 1.0);\n"
"    gl_Position.xy = gl_Position.xy * 2.0 - 1.0;\n"
"}";

inline const char* primSceneFrag = "#version 330\n"
"in vec2 uv;\n"
"out vec4 fragColor;\n"
"uniform sampler2D scene;\n"
"void main() {\n"
"    // as a test, invert colors\n"
"    fragColor = texture(scene, uv);\n"
"}";

inline std::shared_ptr<ShaderProgram> SCENE_SHADER;

inline void InitPrimitiveShaders()
{
	SINGLE_COLOR_SHADER = ShaderProgram::Create(primVert, primFrag);
	BLUR_SHADER = ShaderProgram::Create(primBlurVert, primBlurFrag);
	SCENE_SHADER = ShaderProgram::Create(primSceneVert, primSceneFrag);
}