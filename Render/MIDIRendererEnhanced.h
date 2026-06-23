#pragma once

#include "MIDI/MIDIDefs.h"
#include "Renderer/AbstractMIDIRenderer.h"
#include <cstdint>
#include <mutex>
#include <glm/glm.hpp>

#pragma region Renderer settings
struct EnhancedRendererSettings
{
    float exposure = 0.25f;
    float noteOutlineGlowFactor = 6.5f;
    float keyGlowFactor = 5.0f;

    // hsv shift settings
    bool hsvShiftEnabled = true;
    float hsvShiftStrength = 0.9f;
    glm::vec3 hsvShifts = glm::vec3(0.15f, 0.0f, 0.0f);

    // saber settings
    glm::vec3 saberColor = glm::vec3(0.3f, 0.3f, 1.0f);
    float saberBrightness = 12.0f;
    float saberThickness = 0.003f;

    // mist settings
    bool mistEnabled = true;
    float mistOpacity = 0.9f;
    float mistSpeed = 0.2f;
    float mistScale = 10.0f;

    // particle settings
    bool particlesEnabled = true;
};
#pragma endregion

#pragma region 3D Piano Keyboard Data

#pragma pack(push, 1)
struct RenderKeyboardKey3D
{
    float left;
    float right;
    float pressFactor;
    uint32_t meta;
	RenderKeyboardKey3D() = default;
    RenderKeyboardKey3D(float left, float right, uint32_t meta) : left(left), right(right), meta(meta), pressFactor(0.0f) {}
};
#pragma pack(pop)

struct KeyboardMeta3D
{
	static constexpr uint32_t META_PRESSED = 1u << 24;
	static constexpr uint32_t META_BLACK = 1u << 25;

	bool pressed = false;
	bool black = false;
	uint32_t color = 0x000000;

	KeyboardMeta3D() = default;
	KeyboardMeta3D(uint32_t color, bool pressed, bool black)
		: color(color), pressed(pressed), black(black) {
	}

	constexpr uint32_t GetMeta() const
	{
		return (color & 0x00FFFFFF)
			| (pressed ? META_PRESSED : 0)
			| (black ? META_BLACK : 0);
	}

	void MarkPressed(bool pressed)
	{
		this->pressed = pressed;
	}

	void MarkBlack(bool black)
	{
		this->black = black;
	}
};

#pragma endregion

#pragma region Note Data

#pragma pack(push, 1)
struct RenderNote3D
{
    float left;
    float right;
    float start;
    float end;
    uint32_t color;
    RenderNote3D() = default;
    RenderNote3D(float left, float right, float start, float end, uint32_t color)
        : left(left), right(right), start(start), end(end), color(color) {
    }
};
#pragma pack(pop)

#pragma endregion

#pragma region Particle Data

#pragma pack(push, 1)
struct RenderParticleInstance3D
{
    glm::vec3 position;
    glm::vec4 color;
    float scale;

    RenderParticleInstance3D() = default;
    RenderParticleInstance3D(glm::vec3 pos, glm::vec4 col, float scale)
        : position(pos), color(col), scale(scale) {
    }
};
#pragma pack(pop)

struct Particle3D
{
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec4 color;
    float life;
    float maxLife;
    float scale;

    float curveSeed;   
    float curveSpeed;  
    float curveAmp;    
};

#pragma endregion

#define KEY_IS_BLACK(n) \
( ((n) % 12) == 1 || \
  ((n) % 12) == 3 || \
  ((n) % 12) == 6 || \
  ((n) % 12) == 8 || \
  ((n) % 12) == 10 )

#define NOTE_BUFFER_SIZE 32768
#define NOTES_MAX_BATCHES 512
#define PARTICLE_BUFFER_SIZE 16384

class MIDIRendererEnhanced : public AbstractMIDIRenderer
{
public:
    MIDIRendererEnhanced(MIDIApp* app) : AbstractMIDIRenderer(app)
    {
        keyboardData.fill(RenderKeyboardKey3D());
        keyMetas.fill(KeyboardMeta3D());
    }

    void LoadSequence(std::shared_ptr<MIDISequence> sequence) override;

    void Initialize() override;
    void Render(double deltaTime) override;
    void RenderSettings() override;
    void OnResize(int width, int height) override;
    void ResetRenderer() override;
private:
    void CalcKeyPosAndWidth();
    void UpdateKeyboardInstance(double deltaTime);
    void UploadNoteBuffer(size_t count);
    void RenderKeyboard();
    void RenderNotes();
    void RenderSaber();
    void RenderMist();

    // particles
    void EmitNoteExplosion(uint8_t keyID, uint32_t hexColor);
    void UpdateParticles(double deltaTime);
    void RenderParticles();
    glm::mat4 GetViewMatrixFromEuler();

    EnhancedRendererSettings rendererSettings;

    #pragma region Keyboard data
    std::unique_ptr<ShaderProgram> keyboardProgram;
    std::unique_ptr<Buffer> keyboardVBO;
    std::unique_ptr<VertexArray> keyboardVAO;
    std::unique_ptr<Buffer> keyboardIBO;
    std::unique_ptr<Buffer> keyboardEBO;

    std::array<float, 128> keyPos;
    std::array<float, 128> keyWidth;
    std::array<RenderKeyboardKey3D, 128> keyboardData;
    std::array<KeyboardMeta3D, 128> keyMetas;
    std::array<uint8_t, 128> kbIDs;
    #pragma endregion

    #pragma region Note data
    std::unique_ptr<ShaderProgram> notesProgram;
    std::unique_ptr<Buffer> notesVBO;
    std::unique_ptr<VertexArray> notesVAO;
    std::unique_ptr<Buffer> notesIBO;
    std::unique_ptr<Buffer> notesEBO;

    std::array<RenderNote3D, NOTE_BUFFER_SIZE> renderNotes;
    std::array<size_t, MIDI_KEYS> startRenderIDs;
    std::array<size_t, MIDI_KEYS> endRenderIDs;

    long lastTime = -1;
    #pragma endregion

    #pragma region Saber data
    std::unique_ptr<ShaderProgram> saberProgram;
    std::unique_ptr<VertexArray> saberVAO;
    std::unique_ptr<Buffer> saberVBO;
    std::unique_ptr<Buffer> saberEBO;
    #pragma endregion

    #pragma region Mist data
    std::unique_ptr<Quad> mistQuad;
    std::shared_ptr<ShaderProgram> mistProgram;
    #pragma endregion

    #pragma region Particle data
    std::array<Particle3D, PARTICLE_BUFFER_SIZE> particlePool;
    std::array<RenderParticleInstance3D, PARTICLE_BUFFER_SIZE> particleGpuData;
    size_t liveParticleCount = 0;

    std::unique_ptr<ShaderProgram> particleProgram;
    std::unique_ptr<VertexArray> particleVAO;
    std::unique_ptr<Buffer> particleVBO;
    std::unique_ptr<Buffer> particleEBO;
    std::unique_ptr<Buffer> particleIBO;

    const float EMISSION_COOLDOWN = 0.05f;
    std::array<float, MIDI_KEYS> particleEmissionTimers;
    #pragma endregion

    #pragma region post processing effect shaders n stuff
    std::unique_ptr<Quad> screenQuad;
    std::unique_ptr<Framebuffer> hdrSceneFBO;
    std::vector<std::unique_ptr<Framebuffer>> bloomChain;
    std::shared_ptr<ShaderProgram> downsampleShader;
    std::shared_ptr<ShaderProgram> upsampleShader;
    std::shared_ptr<ShaderProgram> compositeShader;
    #pragma endregion

    std::mutex renderMutex;

    bool initialized = false;
    bool keyboardDirty = false;

    // 3d Specific dimensions
    float keyboardDepth = 0.01f;
    float keyThickness = 0.2f;

    // animation speed for key presses
    float pressSpeed = 15.0f;

    float cameraDistance = 1.0f; // distance from keyboard
    float cameraFOV = 45.0f;
    glm::vec3 camPos = glm::vec3(0.5f, 0.5f, 0.0f);
    glm::vec3 keyboardPosition = glm::vec3(0.5f, 0.0f, 0.0f); // center of keyboard
    float keyboardElevation = 0.5f; // height of keyboard in world space

    float keyboardMaxZ = 0.075f;

    float keyboardHeight = 0.13f;
    float keyboardZOffset = 0.0f; // calculated automatically
    
    // stuff from config
    bool isTimeBased = false;

    static float Rand01()
    {
        return (float)rand() / (float)RAND_MAX;
    }

    static float RandRange(float a, float b)
    {
        return a + (b - a) * Rand01();
    }
};