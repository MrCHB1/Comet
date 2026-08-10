#pragma once

#include "MIDI/MIDIDefs.h"
#include "Renderer/AbstractMIDIRenderer.h"
#include <cstdint>
#include <mutex>
#include <glm/glm.hpp>
#include "Concurrency/ThreadPool.h"

#define NOTE_BUFFER_SIZE 262144
#define NOTES_MAX_BATCHES 512
#define PARTICLE_BUFFER_SIZE 131072

#pragma region Gradient noise field (scary shit)

class GradientNoise
{
public:
    GradientNoise()
    {
        std::array<int, 256> p;
        for (int i = 0; i < 256; i++) p[i] = i;

        std::mt19937 rng(69420); // lmfao nice seed bro
        std::shuffle(p.begin(), p.end(), rng);

        for (int i = 0; i < 512; i++)
            perm[i] = p[i & 255];
    }

    // returns roughly in [-1, 1]
    float Sample(float x, float y, float z) const
    {
        int X = (int)std::floor(x) & 255;
        int Y = (int)std::floor(y) & 255;
        int Z = (int)std::floor(z) & 255;

        x -= std::floor(x);
        y -= std::floor(y);
        z -= std::floor(z);

        float u = Fade(x), v = Fade(y), w = Fade(z);

        int A = perm[X] + Y, AA = perm[A] + Z, AB = perm[A + 1] + Z;
        int B = perm[X + 1] + Y, BA = perm[B] + Z, BB = perm[B + 1] + Z;

        return Lerp(w,
            Lerp(v,
                Lerp(u, Grad(perm[AA], x, y, z), Grad(perm[BA], x - 1, y, z)),
                Lerp(u, Grad(perm[AB], x, y - 1, z), Grad(perm[BB], x - 1, y - 1, z))),
            Lerp(v,
                Lerp(u, Grad(perm[AA + 1], x, y, z - 1), Grad(perm[BA + 1], x - 1, y, z - 1)),
                Lerp(u, Grad(perm[AB + 1], x, y - 1, z - 1), Grad(perm[BB + 1], x - 1, y - 1, z - 1))
            )
        );
    }

private:
    int perm[512];

    static float Fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
    static float Lerp(float t, float a, float b) { return a + t * (b - a); }
    static float Grad(int hash, float x, float y, float z)
    {
        int h = hash & 15;
        float u = h < 8 ? x : y;
        float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
        return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
    }
};

#pragma endregion

#pragma region Renderer settings
enum MSAASetting
{
    None,
    AA2x2,
    AA4x4,
    AA6x6
};

struct ParticleSettings
{
    int emission = 12;
    float spawnRate = 0.05f;
    float brightness = 15.0f;

    float gravityFactor = 0.0f;
    float drag = 0.05f;
    float initialSpeed = 0.7f;
    float lifetime = 2.0f;
    float turbulence = 0.0f;
    float turbulenceVariance = 0.5f; // how much the wobble amplitude/speed differs between particles; 0 = uniform, 1 = wide spread
    float swirlStrength = 0.0f;      // strength of the ambient swirl force field
    float swirlScale = 8.0f;         // spatial frequency of the swirl field (higher = tighter eddies)
    float swirlSpeed = 0.3f;         // how fast the vortex field itself drifts/evolves over time (0 = frozen)

    static ParticleSettings Default()
    {
        return ParticleSettings{};
    }
};

struct EnhancedRendererSettings
{
    // visual settings
    MSAASetting msaa = MSAASetting::None;

    float exposure = 0.25f;
    float noteOutlineGlowFactor = 6.5f;

    // keyboard settings
    float keyGlowFactor = 5.0f;
    float keyboardFOV = 45.0f;
    float keyboardBrightness = 1.0f;

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
    ParticleSettings particleSettings = ParticleSettings::Default();

    // flare settings
    bool flaresEnabled = true;
    float flareHeight = 0.25f;
    float flareBrightness = 1.0f;
    float flareFadeDuration = 0.25f; 

    static EnhancedRendererSettings Default()
    {
        return EnhancedRendererSettings{};
    }
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
    float velocity = 0.0f;

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

#pragma region Flare Data

#pragma pack(push, 1)
struct RenderFlare
{
    float left;
    float right;
    uint32_t color;
    float alpha; // per-key fade multiplier (0-1), ramped on press/release

    RenderFlare() = default;
    RenderFlare(float left, float right, uint32_t color, float alpha) : left(left), right(right), color(color), alpha(alpha) {}
};
#pragma pack(pop)

#pragma endregion

#define KEY_IS_BLACK(n) \
( ((n) % 12) == 1 || \
  ((n) % 12) == 3 || \
  ((n) % 12) == 6 || \
  ((n) % 12) == 8 || \
  ((n) % 12) == 10 )

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
    void ResetSettings() override;
    void OnResize(int width, int height) override;
    void ResetRenderer() override;

    std::string GetSerializationKey() const override { return "enhanced"; }
    YAML::Node GetSettings() override;
    void LoadSettings(const YAML::Node& node) override;
private:
    uint32_t msaaFBO = 0;
    uint32_t msaaColorTexture = 0;
    uint32_t msaaDepthRBO = 0;

    void SetupUniforms();
    int GetMSAASamples() const
    {
        switch (rendererSettings.msaa)
        {
        case MSAASetting::AA2x2: return 4;
        case MSAASetting::AA4x4: return 8;
        case MSAASetting::AA6x6: return 16;
        default: return 1;
        }
    }
    void UpdateMSAAFramebuffer();

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

    // flares
    void RenderFlares(double deltaTime);

    EnhancedRendererSettings rendererSettings;

#pragma region Keyboard data
    std::unique_ptr<ShaderProgram> keyboardProgram;

    std::unique_ptr<VertexArray> whiteKeyVAO;
    std::unique_ptr<Buffer> whiteKeyVBO;
    std::unique_ptr<Buffer> whiteKeyEBO;

    std::unique_ptr<VertexArray> blackKeyVAO;
    std::unique_ptr<Buffer> blackKeyVBO;
    std::unique_ptr<Buffer> blackKeyEBO;

    // shared instance buffer used by both black and white keys
    std::unique_ptr<Buffer> keyboardIBO;

    size_t numWhiteKeys = 0;
    size_t numBlackKeys = 0;

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

    std::array<float, MIDI_KEYS> particleEmissionTimers;
    ThreadPool particleThreadPool;
#pragma endregion

#pragma region Flare data
    std::unique_ptr<ShaderProgram> flaresProgram;
    std::unique_ptr<Buffer> flaresVBO;
    std::unique_ptr<VertexArray> flaresVAO;
    std::unique_ptr<Buffer> flaresIBO;
    std::unique_ptr<Buffer> flaresEBO;

    std::array<RenderFlare, MIDI_KEYS> renderFlares;
    std::array<float, MIDI_KEYS> flareFade{}; // current per-key fade multiplier, ramped toward 0 or 1 each frame
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
    float keyPressStiffness = 1600.0f;
    float keyReleaseStiffness = 400.0f;
    float keyDamping = 14.0f;
    float keyTopBounce = 0.8f;

    float cameraDistance = 1.0f; // distance from keyboard
    glm::vec3 camPos = glm::vec3(0.5f, 0.5f, 0.0f);
    glm::vec3 keyboardPosition = glm::vec3(0.5f, 0.0f, 0.0f); // center of keyboard
    float keyboardElevation = 0.5f; // height of keyboard in world space

    float keyboardMaxZ = 0.075f;

    float keyboardHeight = 0.13f;
    float keyboardZOffset = 0.0f; // calculated automatically


    static float Rand01()
    {
        return (float)rand() / (float)RAND_MAX;
    }

    static float RandRange(float a, float b)
    {
        return a + (b - a) * Rand01();
    }
};