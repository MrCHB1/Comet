#define ENHANCED_SHADERS

#include "MIDIRendererEnhanced.h"
#include "MIDIRendererEnhancedShaders.h"
#include <glm/glm.hpp>
#include "App/MIDIApp.h"
#include "App/Models.h"
#include <algorithm>
#include <random>
#include "Utils.h"
#include "App/Dialog/DialogMacros.h"
#include "RenderView.h"

#include "MIDI/Timer/MIDITimer.h"
#include "MIDI/TempoMap.h"

const float keyPosDiff[] = {
    0.6F, 0.4F, 0.8F, 0.2F, 1.0F, 0.6F, 0.4F, 0.675F, 0.325F, 0.8F, 0.2F, 1.0F
};

const GradientNoise g_vortexNoise;

// Derives a divergence-free 2D velocity from the noise's gradient (curl noise):
// treating the noise as a scalar potential and taking its perpendicular gradient
// guarantees the resulting field has no "sources" or "sinks" -- particles get
// swept around in swirling eddies instead of drifting toward/away from points.
glm::vec2 SampleCurlNoise(float x, float y, float t)
{
    const float eps = 0.05f;

    float dNdy = (g_vortexNoise.Sample(x, y + eps, t) - g_vortexNoise.Sample(x, y - eps, t)) / (2.0f * eps);
    float dNdx = (g_vortexNoise.Sample(x + eps, y, t) - g_vortexNoise.Sample(x - eps, y, t)) / (2.0f * eps);

    return glm::vec2(dNdy, -dNdx);
}

const std::vector<float> CUBE_VERTICES = {
    // front face
    0.0f,  1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,
    1.0f,  0.0f,  1.0f,
    0.0f,  0.0f,  1.0f,
    // back face
    0.0f,  1.0f,  0.0f,
    1.0f,  1.0f,  0.0f,
    1.0f,  0.0f,  0.0f,
    0.0f,  0.0f,  0.0f
};

const std::vector<unsigned int> CUBE_INDICES = {
    0, 1, 2, 2, 3, 0, // front
    1, 5, 6, 6, 2, 1, // right
    7, 6, 5, 5, 4, 7, // back
    4, 0, 3, 3, 7, 4, // left
    3, 2, 6, 6, 7, 3, // top
    4, 5, 1, 1, 0, 4  // bottom
};

const std::vector<float> QUAD_VERTICES = {
    0.0f, 1.0f,
    1.0f, 1.0f,
    1.0f, 0.0f,
    0.0f, 0.0f,
};

const std::vector<unsigned int> QUAD_INDICES = {
    0, 1, 3,
    1, 2, 3
};

void MIDIRendererEnhanced::Initialize()
{
    AbstractMIDIRenderer::Initialize();

#pragma region Note buffers + data

    notesProgram = ShaderProgram::Create(notes3dvert, notes3dfrag);
    notesVAO = std::make_unique<VertexArray>();
    notesVBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
    notesIBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
    notesEBO = std::make_unique<Buffer>(GL_ELEMENT_ARRAY_BUFFER);

    {
        VertexArrayBind vaoBind(*notesVAO);

        // static quad verts
        notesVBO->Bind();
        notesVBO->SetData(QUAD_VERTICES, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);

        // index buffer
        notesEBO->Bind();
        notesEBO->SetData(QUAD_INDICES, GL_STATIC_DRAW);

        // instance buffer
        notesIBO->Bind();
        notesIBO->SetData(renderNotes, GL_DYNAMIC_DRAW);

        notesVAO->SetFloatAttribute(1, 1, sizeof(RenderNote3D), offsetof(RenderNote3D, left));
        notesVAO->SetFloatAttribute(2, 1, sizeof(RenderNote3D), offsetof(RenderNote3D, right));
        notesVAO->SetFloatAttribute(3, 1, sizeof(RenderNote3D), offsetof(RenderNote3D, start));
        notesVAO->SetFloatAttribute(4, 1, sizeof(RenderNote3D), offsetof(RenderNote3D, end));
        notesVAO->SetIntAttribute(5, 1, sizeof(RenderNote3D), offsetof(RenderNote3D, color));

        glVertexAttribDivisor(1, 1);
        glVertexAttribDivisor(2, 1);
        glVertexAttribDivisor(3, 1);
        glVertexAttribDivisor(4, 1);
        glVertexAttribDivisor(5, 1);
    }

#pragma endregion

#pragma region Keyboard buffers + data
    // 1. Sort keys to render white keys first, then black keys
    std::vector<uint8_t> blackIDs;
    std::vector<uint8_t> whiteIDs;
    for (uint8_t key = 0; key < 128; key++)
    {
        bool black = KEY_IS_BLACK(key);
        if (black) blackIDs.push_back(key);
        else whiteIDs.push_back(key);
        keyMetas[key].MarkBlack(black);
    }

    numWhiteKeys = whiteIDs.size();
    numBlackKeys = blackIDs.size();

    int i = 0;
    for (auto& white : whiteIDs) kbIDs[i++] = white;
    for (auto& black : blackIDs) kbIDs[i++] = black;

    keyboardProgram = ShaderProgram::Create(keyboard3dvert, keyboard3dfrag);

    keyboardIBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
    keyboardIBO->Bind();
    keyboardIBO->SetData(keyboardData, GL_DYNAMIC_DRAW);

    whiteKeyVAO = std::make_unique<VertexArray>();
    whiteKeyVBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
    whiteKeyEBO = std::make_unique<Buffer>(GL_ELEMENT_ARRAY_BUFFER);

    {
        VertexArrayBind vaoBind(*whiteKeyVAO);
        auto* whiteMesh = Models::WhiteKeyMesh;

        whiteKeyVBO->Bind();
        whiteKeyVBO->SetData(whiteMesh->vertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);

        whiteKeyEBO->Bind();
        whiteKeyEBO->SetData(whiteMesh->indices, GL_STATIC_DRAW);

        keyboardIBO->Bind();
        whiteKeyVAO->SetFloatAttribute(1, 1, sizeof(RenderKeyboardKey3D), offsetof(RenderKeyboardKey3D, left));
        whiteKeyVAO->SetFloatAttribute(2, 1, sizeof(RenderKeyboardKey3D), offsetof(RenderKeyboardKey3D, right));
        whiteKeyVAO->SetFloatAttribute(3, 1, sizeof(RenderKeyboardKey3D), offsetof(RenderKeyboardKey3D, pressFactor));
        whiteKeyVAO->SetIntAttribute(4, 1, sizeof(RenderKeyboardKey3D), offsetof(RenderKeyboardKey3D, meta));

        glVertexAttribDivisor(1, 1);
        glVertexAttribDivisor(2, 1);
        glVertexAttribDivisor(3, 1);
        glVertexAttribDivisor(4, 1);
    }

    blackKeyVAO = std::make_unique<VertexArray>();
    blackKeyVBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
    blackKeyEBO = std::make_unique<Buffer>(GL_ELEMENT_ARRAY_BUFFER);

    {
        VertexArrayBind vaoBind(*blackKeyVAO);
        auto* blackMesh = Models::BlackKeyMesh;

        blackKeyVBO->Bind();
        blackKeyVBO->SetData(blackMesh->vertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);

        blackKeyEBO->Bind();
        blackKeyEBO->SetData(blackMesh->indices, GL_STATIC_DRAW);

        keyboardIBO->Bind();

        // shift the attribute pointers past the white keys
        size_t blackOffset = numWhiteKeys * sizeof(RenderKeyboardKey3D);

        blackKeyVAO->SetFloatAttribute(1, 1, sizeof(RenderKeyboardKey3D), blackOffset + offsetof(RenderKeyboardKey3D, left));
        blackKeyVAO->SetFloatAttribute(2, 1, sizeof(RenderKeyboardKey3D), blackOffset + offsetof(RenderKeyboardKey3D, right));
        blackKeyVAO->SetFloatAttribute(3, 1, sizeof(RenderKeyboardKey3D), blackOffset + offsetof(RenderKeyboardKey3D, pressFactor));
        blackKeyVAO->SetIntAttribute(4, 1, sizeof(RenderKeyboardKey3D), blackOffset + offsetof(RenderKeyboardKey3D, meta));

        glVertexAttribDivisor(1, 1);
        glVertexAttribDivisor(2, 1);
        glVertexAttribDivisor(3, 1);
        glVertexAttribDivisor(4, 1);
    }

#pragma endregion

#pragma region Saber buffers + data
    saberProgram = ShaderProgram::Create(saber3dvert, saber3dfrag);
    saberVAO = std::make_unique<VertexArray>();
    saberVBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
    saberEBO = std::make_unique<Buffer>(GL_ELEMENT_ARRAY_BUFFER);

    {
        VertexArrayBind vaoBind(*saberVAO);

        saberVBO->Bind();
        saberVBO->SetData(CUBE_VERTICES, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);

        saberEBO->Bind();
        saberEBO->SetData(CUBE_INDICES, GL_STATIC_DRAW);
    }
#pragma endregion

#pragma region Mist setup
    mistProgram = ShaderProgram::Create(mist3dvert, mist3dfrag);
    mistQuad = std::make_unique<Quad>();
    mistQuad->SetShader(mistProgram);
#pragma endregion

#pragma region Particle buffers + data
    particleProgram = ShaderProgram::Create(particle3dvert, particle3dfrag);
    particleVAO = std::make_unique<VertexArray>();
    particleVBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
    particleIBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
    particleEBO = std::make_unique<Buffer>(GL_ELEMENT_ARRAY_BUFFER);

    {
        VertexArrayBind vaoBind(*particleVAO);

        particleVBO->Bind();
        particleVBO->SetData(QUAD_VERTICES, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);

        // index buffer
        particleEBO->Bind();
        particleEBO->SetData(QUAD_INDICES, GL_STATIC_DRAW);

        particleIBO->Bind();
        particleIBO->SetData(particlePool, GL_DYNAMIC_DRAW);

        particleVAO->SetFloatAttribute(1, 3, sizeof(RenderParticleInstance3D), offsetof(RenderParticleInstance3D, position));
        particleVAO->SetFloatAttribute(2, 4, sizeof(RenderParticleInstance3D), offsetof(RenderParticleInstance3D, color));
        particleVAO->SetFloatAttribute(3, 1, sizeof(RenderParticleInstance3D), offsetof(RenderParticleInstance3D, scale));

        glVertexAttribDivisor(1, 1);
        glVertexAttribDivisor(2, 1);
        glVertexAttribDivisor(3, 1);
    }
#pragma endregion

#pragma region Flare buffers + data
    flaresProgram = ShaderProgram::Create(flareVert, flareFrag);
    flaresVAO = std::make_unique<VertexArray>();
    flaresVBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
    flaresIBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
    flaresEBO = std::make_unique<Buffer>(GL_ELEMENT_ARRAY_BUFFER);

    {
        VertexArrayBind vaoBind(*flaresVAO);

        flaresVBO->Bind();
        flaresVBO->SetData(QUAD_VERTICES, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);

        flaresEBO->Bind();
        flaresEBO->SetData(QUAD_INDICES, GL_STATIC_DRAW);

        flaresIBO->Bind();
        flaresIBO->SetData(renderFlares, GL_DYNAMIC_DRAW);

        flaresVAO->SetFloatAttribute(1, 1, sizeof(RenderFlare), offsetof(RenderFlare, left));
        flaresVAO->SetFloatAttribute(2, 1, sizeof(RenderFlare), offsetof(RenderFlare, right));
        flaresVAO->SetIntAttribute(3, 1, sizeof(RenderFlare), offsetof(RenderFlare, color));
        flaresVAO->SetFloatAttribute(4, 1, sizeof(RenderFlare), offsetof(RenderFlare, alpha));

        glVertexAttribDivisor(1, 1);
        glVertexAttribDivisor(2, 1);
        glVertexAttribDivisor(3, 1);
        glVertexAttribDivisor(4, 1);
    }
#pragma endregion

#pragma region post processing setup
    downsampleShader = ShaderProgram::Create(fullscreenvert, downsamplefrag);
    upsampleShader = ShaderProgram::Create(fullscreenvert, upsamplefrag);
    compositeShader = ShaderProgram::Create(fullscreenvert, compositefrag);

    screenQuad = std::make_unique<Quad>();
    hdrSceneFBO = std::make_unique<Framebuffer>();
    hdrSceneFBO->Setup(width, height, GL_RGBA16F, GL_RGBA, GL_FLOAT);

    glm::vec2 mipSize(width, height);
    while (true)
    {
        mipSize *= 0.5f;

        int mipWidth = static_cast<int>(mipSize.x);
        int mipHeight = static_cast<int>(mipSize.y);

        if (mipWidth < 2 || mipHeight < 2) {
            break;
        }

        auto fbo = std::make_unique<Framebuffer>();
        fbo->Setup(mipWidth, mipHeight, GL_RGBA16F, GL_RGBA, GL_FLOAT);
        bloomChain.push_back(std::move(fbo));
    }
#pragma endregion

#pragma region uniforms setup
    SetupUniforms();
#pragma endregion

    initialized = true;
    CalcKeyPosAndWidth();
    UpdateMSAAFramebuffer();
}

void MIDIRendererEnhanced::SetupUniforms()
{
    {
        ShaderBind notesBind(*notesProgram);

        notesProgram->SetFloat("noteOutlineGlow", rendererSettings.noteOutlineGlowFactor);
        notesProgram->SetInt("noteHsvShiftEnabled", rendererSettings.hsvShiftEnabled ? 1 : 0);
        notesProgram->SetFloat("noteHsvShiftStrength", rendererSettings.hsvShiftStrength);
        notesProgram->SetVec3("noteHsvShifts", rendererSettings.hsvShifts);
    }

    {
        ShaderBind keyboardBind(*keyboardProgram);

        keyboardProgram->SetFloat("keyGlowFactor", rendererSettings.keyGlowFactor);
        keyboardProgram->SetFloat("keyboardBrightness", 1.0f);
        keyboardProgram->SetInt("noteHsvShiftEnabled", rendererSettings.hsvShiftEnabled ? 1 : 0);
        keyboardProgram->SetFloat("noteHsvShiftStrength", rendererSettings.hsvShiftStrength);
        keyboardProgram->SetVec3("noteHsvShifts", rendererSettings.hsvShifts);
    }

    {
        ShaderBind mistBind(*mistProgram);
        mistProgram->SetVec3("mistColor", rendererSettings.saberColor);
        mistProgram->SetFloat("mistOpacity", rendererSettings.mistOpacity);
        mistProgram->SetFloat("mistSpeed", rendererSettings.mistSpeed);
        mistProgram->SetFloat("mistScale", rendererSettings.mistScale);
    }
}

void MIDIRendererEnhanced::LoadSequence(std::shared_ptr<MIDISequence> sequence)
{
    std::lock_guard<std::mutex> lock(renderMutex);

    if (seq != sequence) AbstractMIDIRenderer::UnloadSequence();
    AbstractMIDIRenderer::LoadSequence(sequence);

    colors.LoadColors();
    lastTime = 0;

    for (auto& id : startBlockIDs)
        id = 0;
}

void MIDIRendererEnhanced::CalcKeyPosAndWidth()
{
    MIDIPlayerConfig* config = app->GetConfig();
    int keyFirst = config->render.GetKeyFirst();
    int keyLast = config->render.GetKeyLast();

    float rawPos[128];
    float rawWidth[128];
    float pos = 0.0f;
    for (int i = 0; i < 128; i++)
    {
        rawPos[i] = pos;
        pos += keyPosDiff[i % 12];
    }

    int lastIdxWhiteUnit = -1;
    for (int j = 0; j < 128; j++)
    {
        if (KEY_IS_BLACK(j))
        {
            rawWidth[j] = 75.0f / 115.0f; // black key width relative to a white key step
        }
        else
        {
            if (lastIdxWhiteUnit != -1)
                rawWidth[lastIdxWhiteUnit] = rawPos[j] - rawPos[lastIdxWhiteUnit];
            lastIdxWhiteUnit = j;
        }
    }
    if (lastIdxWhiteUnit != -1)
        rawWidth[lastIdxWhiteUnit] = 1.0f;

    float spanUnits = (rawPos[keyLast] + rawWidth[keyLast]) - rawPos[keyFirst];
    if (spanUnits < 0.001f)
        spanUnits = 0.001f;

    // full 0-127 span, used as the baseline that keyboardMaxZ was tuned against
    float fullSpanUnits = (rawPos[127] + rawWidth[127]) - rawPos[0];
    if (fullSpanUnits < 0.001f)
        fullSpanUnits = 0.001f;

    float noteWidth = (float)width / spanUnits; // pixels per white-key unit
    float noteWidthBlack = noteWidth * (75.0f / 115.0f);

    float firstKeyPos = rawPos[keyFirst] * noteWidth;
    for (int i = 0; i < 128; i++)
        keyPos[i] = (rawPos[i] * noteWidth - firstKeyPos) / (float)width;

    int lastIdxWhite = -1;
    for (int j = 0; j < 128; j++)
    {
        if (KEY_IS_BLACK(j))
            keyWidth[j] = noteWidthBlack / (float)width;
        else
        {
            if (lastIdxWhite != -1)
                keyWidth[lastIdxWhite] = keyPos[j] - keyPos[lastIdxWhite];
            lastIdxWhite = j;
        }
    }

    keyWidth[keyLast] = 1.0f - keyPos[keyLast];

    float aspect = (float)width / (float)height;
    float heightScale = fullSpanUnits / spanUnits;
    keyboardHeight = keyboardMaxZ * heightScale * 1.75f;

    whiteKeyStart = 0;
    numDrawWhiteKeys = 0;
    bool foundWhiteStart = false;
    for (size_t idx = 0; idx < numWhiteKeys; idx++)
    {
        uint8_t key = kbIDs[idx];
        if (key >= keyFirst && key <= keyLast)
        {
            if (!foundWhiteStart) { whiteKeyStart = idx; foundWhiteStart = true; }
            numDrawWhiteKeys++;
        }
    }

    blackKeyStart = 0;
    numDrawBlackKeys = 0;
    bool foundBlackStart = false;
    for (size_t idx = 0; idx < numBlackKeys; idx++)
    {
        uint8_t key = kbIDs[numWhiteKeys + idx];
        if (key >= keyFirst && key <= keyLast)
        {
            if (!foundBlackStart) { blackKeyStart = idx; foundBlackStart = true; }
            numDrawBlackKeys++;
        }
    }

    RebindKeyboardDrawRange();

    keyboardDirty = true;
}

void MIDIRendererEnhanced::RebindKeyboardDrawRange()
{
    keyboardIBO->Bind();

    {
        VertexArrayBind vaoBind(*whiteKeyVAO);
        size_t whiteBase = whiteKeyStart * sizeof(RenderKeyboardKey3D);
        whiteKeyVAO->SetFloatAttribute(1, 1, sizeof(RenderKeyboardKey3D), whiteBase + offsetof(RenderKeyboardKey3D, left));
        whiteKeyVAO->SetFloatAttribute(2, 1, sizeof(RenderKeyboardKey3D), whiteBase + offsetof(RenderKeyboardKey3D, right));
        whiteKeyVAO->SetFloatAttribute(3, 1, sizeof(RenderKeyboardKey3D), whiteBase + offsetof(RenderKeyboardKey3D, pressFactor));
        whiteKeyVAO->SetIntAttribute(4, 1, sizeof(RenderKeyboardKey3D), whiteBase + offsetof(RenderKeyboardKey3D, meta));
    }

    {
        VertexArrayBind vaoBind(*blackKeyVAO);
        size_t blackOffset = numWhiteKeys * sizeof(RenderKeyboardKey3D);
        size_t blackBase = blackOffset + blackKeyStart * sizeof(RenderKeyboardKey3D);
        blackKeyVAO->SetFloatAttribute(1, 1, sizeof(RenderKeyboardKey3D), blackBase + offsetof(RenderKeyboardKey3D, left));
        blackKeyVAO->SetFloatAttribute(2, 1, sizeof(RenderKeyboardKey3D), blackBase + offsetof(RenderKeyboardKey3D, right));
        blackKeyVAO->SetFloatAttribute(3, 1, sizeof(RenderKeyboardKey3D), blackBase + offsetof(RenderKeyboardKey3D, pressFactor));
        blackKeyVAO->SetIntAttribute(4, 1, sizeof(RenderKeyboardKey3D), blackBase + offsetof(RenderKeyboardKey3D, meta));
    }
}

void MIDIRendererEnhanced::UpdateKeyboardInstance(double deltaTime)
{
    bool needsUpload = false;

    // cap the dt so physics does not explode lol
    float dt = static_cast<float>(std::min(deltaTime, 0.033));

    int i = 0;
    for (uint8_t id : kbIDs)
    {
        float targetPress = keyMetas[id].pressed ? 1.0f : 0.0f;
        float currentPress = keyboardData[i].pressFactor;
        float& velocity = keyMetas[id].velocity;

        bool pressing = targetPress > currentPress;
        float stiffness = pressing ? keyPressStiffness : keyReleaseStiffness;

        float force = stiffness * (targetPress - currentPress) - (keyDamping * velocity);
        velocity += force * dt;
        float newPress = currentPress + velocity * dt;

        if (newPress >= 1.0f)
        {
            newPress = 1.0f;
            velocity = 0.0f;
        }
        else if (newPress <= 0.0f && targetPress == 0.0f)
        {
            newPress = 0.0f;
            if (velocity < 0.0f)
            {
                velocity = -velocity * keyTopBounce;
            }
        }

        if (abs(newPress - targetPress) < 0.001f && std::abs(velocity) < 0.01f)
        {
            newPress = targetPress;
            velocity = 0.0f;
        }

        if (currentPress != newPress)
        {
            needsUpload = true;
        }

        keyboardData[i].left = keyPos[id];
        keyboardData[i].right = keyPos[id] + keyWidth[id];
        keyboardData[i].pressFactor = newPress;
        keyboardData[i].meta = keyMetas[id].GetMeta();

        keyMetas[id].MarkPressed(false);
        i++;
    }

    if (needsUpload || keyboardDirty)
    {
        keyboardIBO->Bind();
        glBufferSubData(GL_ARRAY_BUFFER, 0, keyboardData.size() * sizeof(RenderKeyboardKey3D), keyboardData.data());
        keyboardDirty = false;
    }
}

void MIDIRendererEnhanced::RenderKeyboard()
{
    ShaderBind kbBind(*keyboardProgram);

    // calculate required camera distance based on keyboard height and FOV
    // this ensures the keyboard spans the appropriate portion of the viewport
    float aspect = (float)width / (float)height;
    float verticalFOVRad = glm::radians(rendererSettings.keyboardFOV);

    cameraDistance = 1.0f / (2.0f * tan(verticalFOVRad / 2.0f) * aspect);

    float visibleHeight = 2.0f * cameraDistance * tan(verticalFOVRad / 2.0f);
    float keyDepthReach = keyboardHeight * 0.55f; // 2.2 * 0.25, matches the shader's actual multiplier
    keyboardZOffset = (visibleHeight / 2.0f) - keyDepthReach;
    keyboardZOffset += keyboardBottomFill;

    glm::mat4 projection = glm::perspective(verticalFOVRad, aspect, 0.1f, 100.0f);

    glm::mat4 view = GetViewMatrixFromEuler();
    glm::vec3 cameraPos = glm::vec3(glm::inverse(view)[3]);

    keyboardProgram->SetMat4("projection", projection);
    keyboardProgram->SetMat4("view", view);
    keyboardProgram->SetFloat("keyboardZOffset", keyboardZOffset);
    keyboardProgram->SetFloat("keyboardHeight", keyboardHeight);
    keyboardProgram->SetVec3("cameraPos", cameraPos);
    keyboardProgram->SetFloat("animTime", app->GetTimer()->Elapsed());


    {
        VertexArrayBind whiteBind(*whiteKeyVAO);
        glDrawElementsInstanced(GL_TRIANGLES, Models::WhiteKeyMesh->indices.size(), GL_UNSIGNED_INT, nullptr, (GLsizei)numDrawWhiteKeys);
    }

    {
        VertexArrayBind blackBind(*blackKeyVAO);
        glDrawElementsInstanced(GL_TRIANGLES, Models::BlackKeyMesh->indices.size(), GL_UNSIGNED_INT, nullptr, (GLsizei)numDrawBlackKeys);
    }
}

void MIDIRendererEnhanced::RenderNotes()
{
    if (!seq) return;
    std::vector<NoteSequence>& notes = seq->mergedNotes;
    if (notes.empty()) return;
    auto* renderView = app->GetRenderView();
    if (!renderView) return;

    size_t notesToRender = 0;
    double playbackSeconds = app->GetTimer()->Elapsed();

    TempoMap* tempoMap = seq->GetTempoMap();
    long time = tempoMap->SecsToTicksFromMap(seq->resolution, playbackSeconds);
    double bpm = tempoMap->GetBPMAtTick(time);
    noteCounterInfo->tick.value = time >= 0 ? time : 0;
    noteCounterInfo->timeSeconds.value = playbackSeconds;
    noteCounterInfo->bpm.value = bpm;

    ShaderBind shaderBind(*notesProgram);

    VertexArrayBind notesVAOBind(*notesVAO);
    BufferBind notesIBOBind(*notesIBO);
    BufferBind notesVBOBind(*notesVBO);
    BufferBind notesEBOBind(*notesEBO);

    size_t noteID = 0;
    size_t notesPassed = 0;
    size_t polyphony = 0;
    size_t batchCount = 0;

    double accTime = isTimeBased ? playbackSeconds : time;
    double viewRegion = isTimeBased ? (double)renderView->viewTicks / 1000 : (double)renderView->viewTicks;
    const double invTimeMultiplier = 1.0 / (double)TIME_BASED_MULTIPLIER;
    double targetTick = accTime; if (isTimeBased) targetTick *= TIME_BASED_MULTIPLIER;

    notesProgram->SetFloat("kbHeight", keyboardHeight);
    notesProgram->SetFloat("animTime", static_cast<float>(playbackSeconds));

    MIDIPlayerConfig* config = app->GetConfig();
    int keyFirst = config->render.GetKeyFirst();
    int keyLast = config->render.GetKeyLast();

    for (uint8_t id : kbIDs)
    {
        uint16_t lastTrack = 0;
        uint8_t lastChannel = 0;
        uint32_t lastTick = 0;
        uint32_t lastGate = 0;
        uint32_t lastNote = 0;

        NoteSequence& notesNote = notes[id];
        const std::vector<NoteBlock>& blocks = seq->noteBlocks[id];
        if (blocks.empty()) continue;

#pragma region Note culling

        size_t& blockIndex = startBlockIDs[id];
        size_t noteBegin = blockIndex * NOTE_BLOCK_SIZE;

        if (lastTime < time)
        {
            while (blockIndex < blocks.size() && blocks[blockIndex].maxBound <= targetTick)
            {
                ++blockIndex;
            }
        }
        else if (lastTime > time)
        {
            while (blockIndex > 0 && blocks[blockIndex - 1].maxBound > targetTick)
            {
                --blockIndex;
            }
        }

        noteBegin = std::min(notesNote.Size(), blockIndex * NOTE_BLOCK_SIZE);
        notesPassed += noteBegin;

#pragma endregion

        size_t numNotes = notesNote.Size();
        size_t i = noteBegin;

        while (i < numNotes)
        {
            const size_t blockIndex = i / NOTE_BLOCK_SIZE;
            const size_t blockEnd = std::min(numNotes, (blockIndex + 1) * NOTE_BLOCK_SIZE);

            const NoteBlock& currBlock = blocks[blockIndex];
            double blockMin = currBlock.minBound;
            double blockMax = currBlock.maxBound;

            if (isTimeBased)
            {
                blockMin *= invTimeMultiplier;
                blockMax *= invTimeMultiplier;
            }

            if (blockMax <= accTime)
            {
                notesPassed += blockEnd - i;
                i = blockEnd;
                continue;
            }

            if (blockMin > accTime + viewRegion) break;

            bool beyondView = false;

            for (; i < blockEnd; ++i)
            {
                uint32_t nTick = notesNote.tick[i];
                uint32_t nGate = notesNote.gate[i];
                uint8_t nNote = notesNote.note[i];
                uint16_t nTrack = notesNote.track[i];
                uint8_t nChannel = notesNote.channel[i];

                double noteStart = (double)nTick;
                double noteEnd = (double)(nTick + nGate);

                if (isTimeBased)
                {
                    noteStart *= invTimeMultiplier;
                    noteEnd *= invTimeMultiplier;
                }

                if (noteStart > accTime + viewRegion)
                {
                    beyondView = true;
                    break;
                }

                if (noteEnd <= accTime)
                {
                    notesPassed++;
                    continue;
                }

                if (noteStart <= accTime)
                {
                    keyMetas[nNote].MarkPressed(true);
                    keyMetas[nNote].color = colors.GetColor(nTrack, nChannel);
                    keyboardDirty = true;
                    notesPassed++;
                    polyphony++;
                }

                if (id < keyFirst || id > keyLast)
                {
                    beyondView = true;
                    break;
                }

                // skip rendering this note if it's basically the same one lol
                if (lastNote &&
                    nTick == lastTick &&
                    nNote == lastNote &&
                    nChannel == lastChannel &&
                    nTrack == lastTrack &&
                    nGate == lastGate)
                {
                    continue;
                }

                lastTick = nTick;
                lastNote = nNote;
                lastChannel = nChannel;
                lastTrack = nTrack;
                lastGate = nGate;

                renderNotes[noteID++] = RenderNote3D(
                    keyPos[id],
                    keyPos[id] + keyWidth[id],
                    (float)(noteStart - accTime) / (float)viewRegion,
                    (float)(noteEnd - accTime) / (float)viewRegion,
                    colors.GetColor(nTrack, nChannel)
                );

                if (noteID >= NOTE_BUFFER_SIZE)
                {
                    UploadNoteBuffer(NOTE_BUFFER_SIZE);
                    batchCount++;
                    if (batchCount >= NOTES_MAX_BATCHES)
                    {
                        glFlush();
                        batchCount = 0;
                    }
                    noteID = 0;
                }
                notesToRender++;
            }

            if (beyondView) break;
        }
    }

    if (noteID != 0)
    {
        UploadNoteBuffer(noteID);
        batchCount++;
    }

    noteCounterInfo->notesPassed.value = static_cast<uint64_t>(notesPassed);
    noteCounterInfo->polyphony.value = static_cast<uint64_t>(polyphony);

    if (!noteCounterInfo->npsHistory.empty() && playbackSeconds < noteCounterInfo->npsHistory.back().timeSeconds)
    {
        noteCounterInfo->npsHistory.clear();
    }

    noteCounterInfo->npsHistory.push_back({ playbackSeconds, static_cast<uint64_t>(notesPassed) });
    while (!noteCounterInfo->npsHistory.empty() &&
        (playbackSeconds - noteCounterInfo->npsHistory.front().timeSeconds) > 1.0)
    {
        noteCounterInfo->npsHistory.pop_front();
    }

    if (!noteCounterInfo->npsHistory.empty())
    {
        uint64_t notesOneSecondAgo = noteCounterInfo->npsHistory.front().totalNotes;
        uint64_t currentNotes = static_cast<uint64_t>(notesPassed);

        if (currentNotes >= notesOneSecondAgo)
        {
            noteCounterInfo->notesPerSecond.value = currentNotes - notesOneSecondAgo;
        }
        else
        {
            noteCounterInfo->notesPerSecond.value = 0;
        }
    }
    else
    {
        noteCounterInfo->notesPerSecond.value = 0;
    }

    lastTime = time;
}

void MIDIRendererEnhanced::RenderSaber()
{
    ShaderBind saberBind(*saberProgram);

    float aspect = (float)width / (float)height;
    float verticalFOVRad = glm::radians(rendererSettings.keyboardFOV);
    glm::mat4 projection = glm::perspective(verticalFOVRad, aspect, 0.1f, 100.0f);
    glm::mat4 view = GetViewMatrixFromEuler();

    float kbThickness = 0.20f;
    float blackKeyHeight = 0.035f * kbThickness;
    float blackKeyElevation = 0.001f * kbThickness;
    float exactTopY = blackKeyHeight + blackKeyElevation;

    float saberZ = keyboardZOffset - 0.01f;

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0.0f, exactTopY, saberZ));
    model = glm::translate(model, glm::vec3(0.0f, 0.0f, -rendererSettings.saberThickness));
    model = glm::scale(model, glm::vec3(1.0f, rendererSettings.saberThickness, rendererSettings.saberThickness));

    saberProgram->SetMat4("projection", projection);
    saberProgram->SetMat4("view", view);
    saberProgram->SetMat4("model", model);

    // apply bloom overdrive color
    glm::vec3 glowingColor = rendererSettings.saberColor * rendererSettings.saberBrightness;
    saberProgram->SetVec3("saberColor", glowingColor);

    VertexArrayBind vaoBind(*saberVAO);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
}

void MIDIRendererEnhanced::RenderMist()
{
    mistQuad->SetTransform({ glm::vec3(0.0f, keyboardHeight, 0.0f), glm::vec2(1.0f, 1.0f - keyboardHeight) });
    mistQuad->Draw();

    {
        ShaderBind mistBind(*mistProgram);
        mistProgram->SetFloat("animTime", app->GetTimer()->Elapsed());
    }
}

void MIDIRendererEnhanced::EmitNoteExplosion(uint8_t keyID, uint32_t hexColor)
{
    float r = ((hexColor >> 16) & 0xFF) / 255.0f;
    float g = ((hexColor >> 8) & 0xFF) / 255.0f;
    float b = (hexColor & 0xFF) / 255.0f;

    const auto& pSettings = rendererSettings.particleSettings;

    glm::vec3 hdrColor = glm::vec3(r, g, b) * pSettings.brightness;

    float spawnX = keyPos[keyID];
    int count = pSettings.emission;

    const float spreadDegrees = 30.0f;
    const float minAngle = 90.0f - spreadDegrees * 0.5f;
    const float maxAngle = 90.0f + spreadDegrees * 0.5f;

    for (int i = 0; i < count; i++)
    {
        if (liveParticleCount >= PARTICLE_BUFFER_SIZE) break;
        Particle3D& p = particlePool[liveParticleCount++];
        p.position = glm::vec3(spawnX + ((rand() / (float)RAND_MAX)) * keyWidth[keyID], keyboardHeight, 0.0f);

        float angle = glm::radians(minAngle + ((rand() / (float)RAND_MAX) * (maxAngle - minAngle)));
        float speed = RandRange(0.05f, 0.15f) * pSettings.initialSpeed;

        p.velocity.x = cos(angle) * speed * 0.2f;
        p.velocity.y = sin(angle) * speed * 1.2f;

        p.curveSeed = RandRange(0.0f, 6.2831853f);

        // turbulenceVariance controls how much curveSpeed/curveAmp differ from one
        // particle to the next: 0 makes every particle wobble identically, 1 gives
        // the widest spread. turbulence is the overall master strength on top of that.
        const float speedCenter = 5.5f, speedHalfRange = 1.5f;
        const float ampCenter = 0.055f, ampHalfRange = 0.025f;
        float variance = std::clamp(pSettings.turbulenceVariance, 0.0f, 1.0f);

        p.curveSpeed = speedCenter + RandRange(-speedHalfRange, speedHalfRange) * variance;
        p.curveAmp = (ampCenter + RandRange(-ampHalfRange, ampHalfRange) * variance) * pSettings.turbulence;

        p.color = glm::vec4(hdrColor, 1.0f);
        p.maxLife = RandRange(0.5f, 1.5f) * pSettings.lifetime;
        p.life = p.maxLife;
        p.scale = 0.002f;
    }
}

void MIDIRendererEnhanced::UpdateParticles(double deltaTime)
{
    const auto& pSettings = rendererSettings.particleSettings;
    float swirlTime = static_cast<float>(app->GetTimer()->Elapsed()) * pSettings.swirlSpeed;

    const auto& ProcessParticles = [this, deltaTime, pSettings, swirlTime](size_t start, size_t end) {
        float dt = static_cast<float>(deltaTime);

        for (size_t i = start; i < end; i++)
        {
            Particle3D& p = particlePool[i];
            p.life -= dt;
            if (p.life <= 0.0f)
            {
                continue;
            }

            float t = p.maxLife - p.life;
            float curve = std::sin(t * p.curveSpeed + p.curveSeed) * p.curveAmp;
            p.velocity.x += curve * dt;

            // gravity pulls particles down (+gravity sinks them), wind pushes sideways
            p.velocity.y -= pSettings.gravityFactor * dt;

            // swirl: samples a time-varying curl noise field at the particle's current
            // position, so particles drift through shared, evolving vortices instead of
            // orbiting a fixed anchor.
            if (pSettings.swirlStrength != 0.0f)
            {
                glm::vec2 curl = SampleCurlNoise(p.position.x * pSettings.swirlScale, p.position.y * pSettings.swirlScale, swirlTime);
                p.velocity.x += curl.x * pSettings.swirlStrength * dt;
                p.velocity.y += curl.y * pSettings.swirlStrength * dt;
            }

            // drag exponentially bleeds off velocity over time
            float dragFactor = std::max(0.0f, 1.0f - pSettings.drag * dt);
            p.velocity *= dragFactor;

            p.position += p.velocity * dt;
            p.color.a = p.life / p.maxLife;
            p.scale = p.life / p.maxLife * 0.002f;
        }
        };

    if (liveParticleCount > 0)
    {
        constexpr size_t CHUNK_SIZE = 4096;

        for (size_t i = 0; i < liveParticleCount; i += CHUNK_SIZE)
        {
            size_t end = std::min(i + CHUNK_SIZE, liveParticleCount);
            particleThreadPool.SubmitJob([&, i, end] {
                ProcessParticles(i, end);
                });
        }

        particleThreadPool.WaitForAllJobs();

        size_t i = 0;
        while (i < liveParticleCount)
        {
            if (particlePool[i].life <= 0.0f)
            {
                particlePool[i] = particlePool[--liveParticleCount];
            }
            else
            {
                ++i;
            }
        }
    }

    MIDIPlayerConfig* config = app->GetConfig();
    int keyFirst = config->render.GetKeyFirst();
    int keyLast = config->render.GetKeyLast();

    for (uint8_t id : kbIDs)
    {
        if (id < keyFirst || id > keyLast) continue;

        const auto& key = keyMetas[id];
        float& timer = particleEmissionTimers[id];
        if (!key.pressed)
        {
            timer = 0.0f;
            continue;
        }
        uint32_t keyColor = key.color;

        if (timer == 0.0f)
        {
            EmitNoteExplosion(id, keyColor);
        }

        timer += deltaTime;
        while (timer >= pSettings.spawnRate)
        {
            EmitNoteExplosion(id, keyColor);
            timer -= pSettings.spawnRate;
        }
    }
}

void MIDIRendererEnhanced::RenderParticles()
{
    if (liveParticleCount == 0) return;

    for (size_t i = 0; i < liveParticleCount; i++)
    {
        particleGpuData[i] = { particlePool[i].position, particlePool[i].color, particlePool[i].scale };
    }

    particleIBO->Bind();
    glBufferSubData(GL_ARRAY_BUFFER, 0, liveParticleCount * sizeof(RenderParticleInstance3D), particleGpuData.data());

    ShaderBind pBind(*particleProgram);
    VertexArrayBind pVAOBind(*particleVAO);
    BufferBind pVBOBind(*particleVBO);
    BufferBind pEBOBind(*particleEBO);

    particleProgram->SetFloat("aspect", (float)width / (float)height);
    glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr, liveParticleCount);
}

void MIDIRendererEnhanced::RenderFlares(double deltaTime)
{
    // for now just go through the keyboard and borrow states from there
    // TODO: move uniform calls to rendersettings
    float dt = static_cast<float>(std::min(deltaTime, 0.033));
    float fadeRate = dt / std::max(rendererSettings.flareFadeDuration, 0.0001f);

    size_t flaresToRender = 0;
    for (uint8_t id : kbIDs)
    {
        const auto& key = keyMetas[id];
        float target = key.pressed ? 1.0f : 0.0f;
        float& fade = flareFade[id];

        // linear ramp toward 0 or 1 over flareFadeDuration seconds, so a press fades
        // the flare in and a release fades it back out instead of popping instantly
        if (target > fade)
            fade = std::min(target, fade + fadeRate);
        else
            fade = std::max(target, fade - fadeRate);

        if (fade <= 0.0f) continue;

        float left = keyPos[id];
        float right = left + keyWidth[id];

        renderFlares[flaresToRender++] = RenderFlare(
            left,
            right,
            key.color,
            fade
        );
    }

    if (flaresToRender == 0) return;

    flaresIBO->Bind();
    glBufferSubData(GL_ARRAY_BUFFER, 0, flaresToRender * sizeof(RenderFlare), renderFlares.data());

    ShaderBind flaresBind(*flaresProgram);
    VertexArrayBind vaoBind(*flaresVAO);
    BufferBind vboBind(*flaresVBO);
    BufferBind eboBind(*flaresEBO);

    flaresProgram->SetFloat("kbHeight", keyboardHeight);
    flaresProgram->SetFloat("flareHeight", rendererSettings.flareHeight);
    flaresProgram->SetFloat("flareBrightness", rendererSettings.flareBrightness);
    glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr, flaresToRender);
}

void MIDIRendererEnhanced::Render(double deltaTime)
{
    if (!initialized) return;

    MIDIPlayerConfig* config = app->GetConfig();
    if (config->render.ConsumeKeyRangeChanged())
    {
        liveParticleCount = 0;
        CalcKeyPosAndWidth();
    }

    int samples = GetMSAASamples();

    if (samples > 1)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, msaaFBO);
        glEnable(GL_MULTISAMPLE);
    }
    else
    {
        hdrSceneFBO->Bind();
        glDisable(GL_MULTISAMPLE);
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glDepthMask(GL_FALSE);
    RenderNotes();

    if (rendererSettings.mistEnabled)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        RenderMist();
        glDisable(GL_BLEND);
    }

    if (rendererSettings.flaresEnabled)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        RenderFlares(deltaTime);
        glDisable(GL_BLEND);
    }

    if (rendererSettings.particlesEnabled)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        UpdateParticles(deltaTime);
        RenderParticles();
        glDisable(GL_BLEND);
    }

    glDepthMask(GL_TRUE);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    UpdateKeyboardInstance(deltaTime);
    RenderKeyboard();
    RenderSaber();

    glDisable(GL_DEPTH_TEST);

    if (samples > 1)
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, msaaFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, hdrSceneFBO->GetID());
        glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    else
    {
        hdrSceneFBO->Unbind();
    }

    glDisable(GL_DEPTH_TEST);

    {
        ShaderBind downsampleBind(*downsampleShader);
        downsampleShader->SetInt("srcTexture", 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, hdrSceneFBO->GetSceneTexture());

        screenQuad->SetShader(downsampleShader);
        for (int i = 0; i < bloomChain.size(); i++) {
            bloomChain[i]->Bind();
            glViewport(0, 0, bloomChain[i]->GetWidth(), bloomChain[i]->GetHeight());
            downsampleShader->SetInt("mipLevel", i);
            screenQuad->Draw();

            glBindTexture(GL_TEXTURE_2D, bloomChain[i]->GetSceneTexture()); // Set texture for NEXT pass
        }
    }

    {
        ShaderBind upsampleBind(*upsampleShader);

        upsampleShader->SetInt("srcTexture", 0);
        upsampleShader->SetFloat("filterRadius", 1.5f); // could be adjustable but eh

        // Enable additive blending so the bloom layers stack
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);

        screenQuad->SetShader(upsampleShader);
        for (int i = bloomChain.size() - 1; i > 0; i--) {
            bloomChain[i - 1]->Bind(); // Bind the larger FBO
            glViewport(0, 0, bloomChain[i - 1]->GetWidth(), bloomChain[i - 1]->GetHeight());

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, bloomChain[i]->GetSceneTexture()); // Read from smaller FBO

            screenQuad->Draw();
        }
        glDisable(GL_BLEND);
    }


    sceneFramebuffer->Bind();
    glViewport(0, 0, width, height);

    {
        ShaderBind compositeBind(*compositeShader);
        compositeShader->SetInt("sceneTex", 0);
        compositeShader->SetInt("bloomTex", 1);
        compositeShader->SetFloat("exposure", rendererSettings.exposure);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, hdrSceneFBO->GetSceneTexture());

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, bloomChain[0]->GetSceneTexture());

        screenQuad->SetShader(compositeShader);
        screenQuad->Draw();
    }

    sceneFramebuffer->Unbind();

}

void MIDIRendererEnhanced::RenderSettings()
{
    if (ImGui::BeginTabBar("##renderSettings"))
    {
        if (ImGui::BeginTabItem("Visual"))
        {
            SECTION_HEADER("Rendering");
            BEGIN_SECTION("##rendering")
            {
                SETUP_SECTION;

                SECTION_ENTRY(SECTION_LABEL("Exposure"),
                    {
                        float exposure = rendererSettings.exposure;

                        if (ImGui::SliderFloat("##exposure", &exposure, 0.05f, 5.0f))
                        {
                            rendererSettings.exposure = std::clamp(exposure, 0.05f, 5.0f);
                        }
                    });

                SECTION_ENTRY(SECTION_LABEL("Anti-aliasing"),
                    {
                        MSAASetting previousMSAA = rendererSettings.msaa;

                        IMGUI_RADIO_BUTTON("None", rendererSettings.msaa, MSAASetting::None);
                        IMGUI_RADIO_BUTTON("Low (2x2)", rendererSettings.msaa, MSAASetting::AA2x2);
                        IMGUI_RADIO_BUTTON("Medium (4x4)", rendererSettings.msaa, MSAASetting::AA4x4);
                        IMGUI_RADIO_BUTTON("High (6x6)", rendererSettings.msaa, MSAASetting::AA6x6);

                        if (previousMSAA != rendererSettings.msaa)
                            UpdateMSAAFramebuffer();
                    });

                END_SECTION;
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Notes"))
        {
            SECTION_HEADER("Note Appearance");
            BEGIN_SECTION("##noteAppearance")
            {
                SETUP_SECTION;

                SECTION_ENTRY(SECTION_LABEL("Outline emission"),
                    {
                        float glow = rendererSettings.noteOutlineGlowFactor;

                        if (ImGui::SliderFloat("##noteOutlineGlow", &glow, 1.0f, 8.0f))
                        {
                            rendererSettings.noteOutlineGlowFactor = std::clamp(glow, 1.0f, 8.0f);
                            ShaderBind notesBind(*notesProgram);
                            notesProgram->SetFloat("noteOutlineGlow", rendererSettings.noteOutlineGlowFactor);
                        }
                    });

                END_SECTION;
            }

            SECTION_HEADER("HSV Shift");
            BEGIN_SECTION("##hsvShift")
            {
                SETUP_SECTION;

                SECTION_ENTRY(
                    TABLE_LABEL_TOOLTIP(
                        "Enable HSV shift",
                        "Applies a hue, saturation, and value gradient shift to notes."
                    ),
                    {
                        if (ImGui::Checkbox("##hsvShift", &rendererSettings.hsvShiftEnabled))
                        {
                            {
                                ShaderBind notesBind(*notesProgram);
                                notesProgram->SetInt("noteHsvShiftEnabled", rendererSettings.hsvShiftEnabled ? 1 : 0);
                            }

                            {
                                ShaderBind keyboardBind(*keyboardProgram);
                                keyboardProgram->SetInt("noteHsvShiftEnabled", rendererSettings.hsvShiftEnabled ? 1 : 0);
                            }
                        }
                    });

                if (rendererSettings.hsvShiftEnabled)
                {
                    SECTION_ENTRY(SECTION_LABEL("Strength"),
                        {
                            float strength = rendererSettings.hsvShiftStrength;

                            if (ImGui::SliderFloat("##hsvStrength", &strength, 0.0f, 1.0f))
                            {
                                rendererSettings.hsvShiftStrength = std::clamp(strength, 0.0f, 1.0f);

                                {
                                    ShaderBind notesBind(*notesProgram);
                                    notesProgram->SetFloat("noteHsvShiftStrength", rendererSettings.hsvShiftStrength);
                                }

                                {
                                    ShaderBind keyboardBind(*keyboardProgram);
                                    keyboardProgram->SetFloat("noteHsvShiftStrength", rendererSettings.hsvShiftStrength);
                                }
                            }
                        });

                    bool shouldForwardUniform = false;

                    SECTION_ENTRY(SECTION_LABEL("Hue shift"),
                        {
                            float value = rendererSettings.hsvShifts.x;

                            if (ImGui::SliderFloat("##hsvHueShift", &value, -1.0f, 1.0f))
                            {
                                rendererSettings.hsvShifts.x = std::clamp(value, -1.0f, 1.0f);
                                shouldForwardUniform = true;
                            }
                        });

                    SECTION_ENTRY(SECTION_LABEL("Saturation shift"),
                        {
                            float value = rendererSettings.hsvShifts.y;

                            if (ImGui::SliderFloat("##hsvSatShift", &value, -1.0f, 1.0f))
                            {
                                rendererSettings.hsvShifts.y = std::clamp(value, -1.0f, 1.0f);
                                shouldForwardUniform = true;
                            }
                        });

                    SECTION_ENTRY(SECTION_LABEL("Value shift"),
                        {
                            float value = rendererSettings.hsvShifts.z;

                            if (ImGui::SliderFloat("##hsvValShift", &value, -1.0f, 1.0f))
                            {
                                rendererSettings.hsvShifts.z = std::clamp(value, -1.0f, 1.0f);
                                shouldForwardUniform = true;
                            }
                        });

                    if (shouldForwardUniform)
                    {
                        {
                            ShaderBind notesBind(*notesProgram);
                            notesProgram->SetVec3("noteHsvShifts", rendererSettings.hsvShifts);
                        }

                        {
                            ShaderBind keyboardBind(*keyboardProgram);
                            keyboardProgram->SetVec3("noteHsvShifts", rendererSettings.hsvShifts);
                        }
                    }
                }

                END_SECTION;
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Keyboard"))
        {
            SECTION_HEADER("Keyboard Appearance");
            BEGIN_SECTION("##keyboardAppearance")
            {
                SETUP_SECTION;

                SECTION_ENTRY(SECTION_LABEL("Key glow"),
                    {
                        float value = rendererSettings.keyGlowFactor;

                        if (ImGui::SliderFloat("##keyGlowFactor", &value, 1.0f, 8.0f))
                        {
                            rendererSettings.keyGlowFactor = std::clamp(value, 1.0f, 8.0f);

                            ShaderBind keyboardBind(*keyboardProgram);
                            keyboardProgram->SetFloat("keyGlowFactor", rendererSettings.keyGlowFactor);
                        }
                    });

                SECTION_ENTRY(SECTION_LABEL("Brightness"),
                    {
                        float value = rendererSettings.keyboardBrightness;

                        if (ImGui::SliderFloat("##keyboardBrightness", &value, 0.1f, 1.0f))
                        {
                            rendererSettings.keyboardBrightness = std::clamp(value, 0.1f, 1.0f);

                            ShaderBind keyboardBind(*keyboardProgram);
                            keyboardProgram->SetFloat("keyboardBrightness", rendererSettings.keyboardBrightness);
                        }
                    });

                SECTION_ENTRY(SECTION_LABEL("Field of view"),
                    {
                        float value = rendererSettings.keyboardFOV;

                        if (ImGui::SliderFloat("##keyboardFov", &value, 20.0f, 89.9f))
                        {
                            rendererSettings.keyboardFOV = std::clamp(value, 20.0f, 89.9f);
                        }
                    });

                END_SECTION;
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Saber"))
        {
            SECTION_HEADER("Saber Appearance");
            BEGIN_SECTION("##saberAppearance")
            {
                SETUP_SECTION;

                SECTION_ENTRY(SECTION_LABEL("Color"),
                    {
                        if (ImGui::ColorEdit3("##saberColor", &rendererSettings.saberColor.x))
                        {
                            ShaderBind mistBind(*mistProgram);
                            mistProgram->SetVec3("mistColor", rendererSettings.saberColor);
                        }
                    });

                SECTION_ENTRY(SECTION_LABEL("Brightness"),
                    {
                        if (ImGui::SliderFloat("##saberBrightness", &rendererSettings.saberBrightness, 0.0f, 20.0f))
                        {
                            rendererSettings.saberBrightness = std::clamp(rendererSettings.saberBrightness, 0.0f, 20.0f);
                        }
                    });

                END_SECTION;
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Mist"))
        {
            SECTION_HEADER("Mist");
            BEGIN_SECTION("##mist")
            {
                SETUP_SECTION;

                SECTION_ENTRY(SECTION_LABEL("Enable mist"),
                    {
                        ImGui::Checkbox("##enableMist", &rendererSettings.mistEnabled);
                    });

                END_SECTION;
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Particle system"))
        {
            SECTION_HEADER("Particles");
            BEGIN_SECTION("##particles")
            {
                SETUP_SECTION;

                SECTION_ENTRY(SECTION_LABEL("Enable particles"),
                    {
                        ImGui::Checkbox("##enableParticles", &rendererSettings.particlesEnabled);
                    });

                END_SECTION;
            }

            if (rendererSettings.particlesEnabled)
            {
                auto& pSettings = rendererSettings.particleSettings;

                SECTION_HEADER("Appearance");
                BEGIN_SECTION("##particleAppearance")
                {
                    SETUP_SECTION;

                    SECTION_ENTRY(
                        SECTION_LABEL("Particle brightness"),
                        {
                            float value = pSettings.brightness;
                            if (ImGui::SliderFloat("##particleBrightness", &value, 1.0f, 20.0f))
                            {
                                pSettings.brightness = std::clamp(value, 1.0f, 20.0f);
                            }
                        });

                    END_SECTION;
                }

                SECTION_HEADER("Behaviour");
                BEGIN_SECTION("##particleBehaviour")
                {
                    SETUP_SECTION;

                    SECTION_ENTRY(
                        TABLE_LABEL_TOOLTIP(
                            "Emission",
                            "Controls how many particles are emitted (spawned) at once. Default is 12. Higher values are not recommended on Black MIDIs."
                        ),
                        {
                            int value = pSettings.emission;
                            if (ImGui::SliderInt("##particleEmission", &value, 1, 50))
                            {
                                pSettings.emission = std::clamp(value, 1, 50);
                            }
                        });

                    SECTION_ENTRY(
                        TABLE_LABEL_TOOLTIP(
                            "Spawn rate",
                            "Controls how frequently particle bursts are emitted. Default is 0.05. Higher values are not recommended on Black MIDIs."
                        ),
                        {
                            float value = pSettings.spawnRate;
                            if (ImGui::SliderFloat("##particleSpawnRate", &value, 0.01f, 1.0f))
                            {
                                pSettings.spawnRate = std::clamp(value, 0.01f, 1.0f);
                            }
                        });

                    SECTION_ENTRY(
                        TABLE_LABEL_TOOLTIP(
                            "Initial speed",
                            "Controls how fast particles fly out from the key on emission."
                        ),
                        {
                            float value = pSettings.initialSpeed;
                            if (ImGui::SliderFloat("##particleInitialSpeed", &value, 0.1f, 3.0f))
                            {
                                pSettings.initialSpeed = std::clamp(value, 0.1f, 3.0f);
                            }
                        });

                    SECTION_ENTRY(
                        TABLE_LABEL_TOOLTIP(
                            "Lifetime",
                            "Controls how long particles survive before fading out."
                        ),
                        {
                            float value = pSettings.lifetime;
                            if (ImGui::SliderFloat("##particleLifetime", &value, 0.2f, 5.0f))
                            {
                                pSettings.lifetime = std::clamp(value, 0.2f, 5.0f);
                            }
                        });

                    END_SECTION;
                }

                SECTION_HEADER("Dynamics & Forces");
                BEGIN_SECTION("##dynamicsForces")
                {
                    SETUP_SECTION;

                    SECTION_ENTRY(
                        TABLE_LABEL_TOOLTIP(
                            "Gravity",
                            "Pulls particles downward over their lifetime. Negative values make them float upward."
                        ),
                        {
                            float value = pSettings.gravityFactor;
                            if (ImGui::SliderFloat("##particleGravity", &value, -2.0f, 2.0f))
                            {
                                pSettings.gravityFactor = std::clamp(value, -2.0f, 2.0f);
                            }
                        });

                    SECTION_ENTRY(
                        TABLE_LABEL_TOOLTIP(
                            "Drag",
                            "Bleeds off particle velocity over time. Higher values make particles slow down and stop faster."
                        ),
                        {
                            float value = pSettings.drag;
                            if (ImGui::SliderFloat("##particleDrag", &value, 0.0f, 3.0f))
                            {
                                pSettings.drag = std::clamp(value, 0.0f, 3.0f);
                            }
                        });

                    SECTION_ENTRY(
                        TABLE_LABEL_TOOLTIP(
                            "Turbulence",
                            "Adds a wobbling sideways drift to each particle's path. 0 is a straight arc."
                        ),
                        {
                            float value = pSettings.turbulence;
                            if (ImGui::SliderFloat("##particleTurbulence", &value, 0.0f, 2.0f))
                            {
                                pSettings.turbulence = std::clamp(value, 0.0f, 2.0f);
                            }
                        });

                    SECTION_ENTRY(
                        TABLE_LABEL_TOOLTIP(
                            "Turbulence variation",
                            "Controls how much the wobble differs from one particle to the next. 0 makes every particle wobble identically; higher values spread them out."
                        ),
                        {
                            float value = pSettings.turbulenceVariance;
                            if (ImGui::SliderFloat("##particleTurbulenceVariance", &value, 0.0f, 1.0f))
                            {
                                pSettings.turbulenceVariance = std::clamp(value, 0.0f, 1.0f);
                            }
                        });

                    SECTION_ENTRY(
                        TABLE_LABEL_TOOLTIP(
                            "Swirl strength",
                            "Pulls particles through ambient swirling eddies as they fly, rather than a straight or purely turbulent path. 0 disables it."
                        ),
                        {
                            float value = pSettings.swirlStrength;
                            if (ImGui::SliderFloat("##particleSwirlStrength", &value, 0.0f, 3.0f))
                            {
                                pSettings.swirlStrength = std::clamp(value, 0.0f, 3.0f);
                            }
                        });

                    if (pSettings.swirlStrength > 0.0f)
                    {
                        SECTION_ENTRY(
                            TABLE_LABEL_TOOLTIP(
                                "Swirl scale",
                                "Spatial frequency of the swirl field. Lower values make broad, sweeping eddies; higher values make tight, small ones."
                            ),
                            {
                                float value = pSettings.swirlScale;
                                if (ImGui::SliderFloat("##particleSwirlScale", &value, 1.0f, 40.0f))
                                {
                                    pSettings.swirlScale = std::clamp(value, 1.0f, 40.0f);
                                }
                            });

                        SECTION_ENTRY(
                            TABLE_LABEL_TOOLTIP(
                                "Swirl speed",
                                "How fast the vortex field itself drifts and evolves over time. 0 freezes it into a static field."
                            ),
                            {
                                float value = pSettings.swirlSpeed;
                                if (ImGui::SliderFloat("##particleSwirlSpeed", &value, 0.0f, 2.0f))
                                {
                                    pSettings.swirlSpeed = std::clamp(value, 0.0f, 2.0f);
                                }
                            });
                    }

                    END_SECTION;
                }
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Flares"))
        {
            BEGIN_SECTION("##flares")
            {
                SETUP_SECTION;

                SECTION_ENTRY(SECTION_LABEL("Enable flares"),
                    {
                        ImGui::Checkbox("##enableFlares", &rendererSettings.flaresEnabled);
                    });

                SECTION_ENTRY(SECTION_LABEL("Height"),
                    {
                        float val = rendererSettings.flareHeight;
                        if (ImGui::SliderFloat("##flareHeight", &val, 0.1f, 0.9f))
                            rendererSettings.flareHeight = std::clamp(val, 0.1f, 0.9f);
                    });

                SECTION_ENTRY(SECTION_LABEL("Brightness"),
                    {
                        float val = rendererSettings.flareBrightness;
                        if (ImGui::SliderFloat("##flareBrightness", &val, 0.1f, 3.0f))
                        rendererSettings.flareBrightness = std::clamp(val, 0.1f, 3.0f);
                    });

                SECTION_ENTRY(SECTION_LABEL("Fade duration"),
                    {
                        float val = rendererSettings.flareFadeDuration;
                        if (ImGui::SliderFloat("##flareFadeDuration", &val, 0.0f, 1.0f))
                        rendererSettings.flareFadeDuration = std::clamp(val, 0.0f, 1.0f);
                    });

                END_SECTION;
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    AbstractMIDIRenderer::RenderSettings();
}

void MIDIRendererEnhanced::ResetSettings()
{
    rendererSettings = EnhancedRendererSettings::Default();

    {
        ShaderBind notesBind(*notesProgram);
        notesProgram->SetFloat("noteOutlineGlow", rendererSettings.noteOutlineGlowFactor);
        notesProgram->SetVec3("noteHsvShifts", rendererSettings.hsvShifts);
    }

    {
        ShaderBind keyboardBind(*keyboardProgram);
        keyboardProgram->SetFloat("keyGlowFactor", rendererSettings.keyGlowFactor);
    }

    {
        ShaderBind mistBind(*mistProgram);
        mistProgram->SetVec3("mistColor", rendererSettings.saberColor);
    }
}

glm::mat4 MIDIRendererEnhanced::GetViewMatrixFromEuler()
{
    // Convert Euler angles to a rotation matrix
    glm::mat4 rotation = glm::mat4(1.0f);
    rotation = glm::rotate(rotation, glm::radians(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    rotation = glm::rotate(rotation, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    rotation = glm::rotate(rotation, glm::radians(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    // Apply rotation to forward direction (0, 0, -1) to get camera's looking direction
    glm::vec4 forward = rotation * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);

    // Camera position is offset from keyboard along the looking direction (opposite)
    glm::vec3 camPos = keyboardPosition - glm::vec3(forward) * cameraDistance;
    glm::vec4 upVec = rotation * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
    glm::vec3 up = glm::vec3(upVec);

    glm::vec3 target = keyboardPosition;

    return glm::lookAt(camPos, target, up);
}

void MIDIRendererEnhanced::UploadNoteBuffer(size_t count)
{
    if (!notesIBO) return;

    notesIBO->Bind();

    GLbitfield mapFlags = GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT;
    void* ptr = glMapBufferRange(GL_ARRAY_BUFFER, 0, count * sizeof(RenderNote3D), mapFlags);
    if (ptr)
    {
        memcpy(ptr, renderNotes.data(), count * sizeof(RenderNote3D));
        glUnmapBuffer(GL_ARRAY_BUFFER);
    }

    glDrawElementsInstanced(
        GL_TRIANGLES,
        6,
        GL_UNSIGNED_INT,
        nullptr,
        count
    );
}

void MIDIRendererEnhanced::UpdateMSAAFramebuffer()
{
    if (msaaFBO) glDeleteFramebuffers(1, &msaaFBO);
    if (msaaColorTexture) glDeleteTextures(1, &msaaColorTexture);
    if (msaaDepthRBO) glDeleteRenderbuffers(1, &msaaDepthRBO);

    msaaFBO = 0;
    msaaColorTexture = 0;
    msaaDepthRBO = 0;

    int samples = GetMSAASamples();
    if (samples <= 1) return;

    int maxSamples;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    if (samples > maxSamples) samples = maxSamples;

    glGenFramebuffers(1, &msaaFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, msaaFBO);

    glGenTextures(1, &msaaColorTexture);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msaaColorTexture);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, GL_RGBA16F, width, height, GL_TRUE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, msaaColorTexture, 0);

    glGenRenderbuffers(1, &msaaDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, msaaDepthRBO);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msaaDepthRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void MIDIRendererEnhanced::ResetRenderer()
{
    liveParticleCount = 0;
    for (float& timer : particleEmissionTimers)
    {
        timer = 0.0f;
    }

    for (int i = 0; i < kbIDs.size(); i++)
    {
        keyboardData[i].pressFactor = 0.0f;
        keyMetas[i].velocity = 0.0f;
        flareFade[i] = 0.0f;
    }
}

void MIDIRendererEnhanced::OnResize(int width, int height)
{
    AbstractMIDIRenderer::OnResize(width, height);
    this->width = width;
    this->height = height;

    if (!initialized)
    {
        float aspect = (float)width / (float)height;
        keyboardHeight = keyboardMaxZ * aspect;
        return;
    }
    CalcKeyPosAndWidth();

    // regenerate bloom stuff
    hdrSceneFBO->Resize(width, height);
    bloomChain.clear();

    glm::vec2 mipSize(width, height);
    while (true)
    {
        mipSize *= 0.5f;

        int mipWidth = static_cast<int>(mipSize.x);
        int mipHeight = static_cast<int>(mipSize.y);

        if (mipWidth < 2 || mipHeight < 2) {
            break;
        }

        auto fbo = std::make_unique<Framebuffer>();
        fbo->Setup(mipWidth, mipHeight, GL_RGBA16F, GL_RGBA, GL_FLOAT);
        bloomChain.push_back(std::move(fbo));
    }

    // update msaa buffers
    UpdateMSAAFramebuffer();
}

YAML::Node MIDIRendererEnhanced::GetSettings()
{
    YAML::Node node;

    YAML::Node visual;
    visual["msaa"] = static_cast<int>(rendererSettings.msaa);
    visual["exposure"] = rendererSettings.exposure;

    YAML::Node keyboard;
    keyboard["keyGlowFactor"] = rendererSettings.keyGlowFactor;
    keyboard["fov"] = rendererSettings.keyboardFOV;
    keyboard["brightness"] = rendererSettings.keyboardBrightness;

    YAML::Node notes;
    notes["outlineGlowFactor"] = rendererSettings.noteOutlineGlowFactor;
    notes["hsv"]["shiftEnabled"] = rendererSettings.hsvShiftEnabled;
    notes["hsv"]["shiftStrength"] = rendererSettings.hsvShiftStrength;
    notes["hsv"]["shifts"] = Utils::Vec3ToNode(rendererSettings.hsvShifts);

    YAML::Node saber;
    auto saberColor = rendererSettings.saberColor;
    saber["color"] = Utils::EncodeColor(ImVec4(
        saberColor.x,
        saberColor.y,
        saberColor.z,
        1.0
    ));
    saber["brightness"] = rendererSettings.saberBrightness;

    YAML::Node mist;
    mist["enabled"] = rendererSettings.mistEnabled;

    YAML::Node particles;
    particles["enabled"] = rendererSettings.particlesEnabled;

    particles["brightness"] = rendererSettings.particleSettings.brightness;
    particles["emission"] = rendererSettings.particleSettings.emission;
    particles["spawnRate"] = rendererSettings.particleSettings.spawnRate;
    particles["gravityFactor"] = rendererSettings.particleSettings.gravityFactor;
    particles["drag"] = rendererSettings.particleSettings.drag;
    particles["initialSpeed"] = rendererSettings.particleSettings.initialSpeed;
    particles["lifetime"] = rendererSettings.particleSettings.lifetime;
    particles["turbulence"] = rendererSettings.particleSettings.turbulence;
    particles["turbulenceVariance"] = rendererSettings.particleSettings.turbulenceVariance;
    particles["swirlStrength"] = rendererSettings.particleSettings.swirlStrength;
    particles["swirlScale"] = rendererSettings.particleSettings.swirlScale;
    particles["swirlSpeed"] = rendererSettings.particleSettings.swirlSpeed;

    YAML::Node flares;
    flares["enabled"] = rendererSettings.flaresEnabled;
    flares["height"] = rendererSettings.flareHeight;
    flares["brightness"] = rendererSettings.flareBrightness;
    flares["fadeDuration"] = rendererSettings.flareFadeDuration;

    node["visual"] = visual;
    node["notes"] = notes;
    node["keyboard"] = keyboard;
    node["saber"] = saber;
    node["mist"] = mist;
    node["particles"] = particles;
    node["flares"] = flares;

    return node;
}

void MIDIRendererEnhanced::LoadSettings(const YAML::Node& node)
{
    if (!node) return;

    if (node["visual"])
    {
        auto visual = node["visual"];
        int msaaVal = static_cast<int>(rendererSettings.msaa);
        LOAD_VAL(visual, "msaa", msaaVal);
        rendererSettings.msaa = static_cast<decltype(rendererSettings.msaa)>(msaaVal);
        LOAD_VAL(visual, "exposure", rendererSettings.exposure);
    }

    if (node["keyboard"])
    {
        auto keyboard = node["keyboard"];
        LOAD_VAL(keyboard, "keyGlowFactor", rendererSettings.keyGlowFactor);
        LOAD_VAL(keyboard, "fov", rendererSettings.keyboardFOV);
        LOAD_VAL(keyboard, "brightness", rendererSettings.keyboardBrightness);
    }

    if (node["notes"])
    {
        auto notes = node["notes"];
        LOAD_VAL(notes, "outlineGlowFactor", rendererSettings.noteOutlineGlowFactor);

        if (notes["hsv"])
        {
            auto hsv = notes["hsv"];
            LOAD_VAL(hsv, "shiftEnabled", rendererSettings.hsvShiftEnabled);
            LOAD_VAL(hsv, "shiftStrength", rendererSettings.hsvShiftStrength);
            if (hsv["shifts"])
            {
                rendererSettings.hsvShifts = Utils::NodeToVec3(hsv["shifts"]);
            }
        }
    }

    if (node["saber"])
    {
        auto saber = node["saber"];
        LOAD_VAL(saber, "brightness", rendererSettings.saberBrightness);
        if (saber["color"])
        {
            std::variant<std::string, uint32_t> colorInput;
            try
            {
                colorInput = saber["color"].as<std::string>();
            }
            catch (...)
            {
                colorInput = saber["color"].as<uint32_t>();
            }

            auto parsedColor = Utils::ParseColor(colorInput, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            if (parsedColor)
            {
                rendererSettings.saberColor = glm::vec3(parsedColor->x, parsedColor->y, parsedColor->z);

            }
        }
    }

    if (node["mist"])
    {
        auto mist = node["mist"];
        LOAD_VAL(mist, "enabled", rendererSettings.mistEnabled);
    }

    if (node["particles"])
    {
        auto particles = node["particles"];
        LOAD_VAL(particles, "enabled", rendererSettings.particlesEnabled);
        LOAD_VAL(particles, "brightness", rendererSettings.particleSettings.brightness);
        LOAD_VAL(particles, "emission", rendererSettings.particleSettings.emission);
        LOAD_VAL(particles, "spawnRate", rendererSettings.particleSettings.spawnRate);
        LOAD_VAL(particles, "gravityFactor", rendererSettings.particleSettings.gravityFactor);
        LOAD_VAL(particles, "drag", rendererSettings.particleSettings.drag);
        LOAD_VAL(particles, "initialSpeed", rendererSettings.particleSettings.initialSpeed);
        LOAD_VAL(particles, "lifetime", rendererSettings.particleSettings.lifetime);
        LOAD_VAL(particles, "turbulence", rendererSettings.particleSettings.turbulence);
        LOAD_VAL(particles, "turbulenceVariance", rendererSettings.particleSettings.turbulenceVariance);
        LOAD_VAL(particles, "swirlStrength", rendererSettings.particleSettings.swirlStrength);
        LOAD_VAL(particles, "swirlScale", rendererSettings.particleSettings.swirlScale);
        LOAD_VAL(particles, "swirlSpeed", rendererSettings.particleSettings.swirlSpeed);
    }

    if (node["flare"])
    {
        auto flare = node["flare"];
        LOAD_VAL(flare, "enabled", rendererSettings.flaresEnabled);
        LOAD_VAL(flare, "height", rendererSettings.flareHeight);
        LOAD_VAL(flare, "fadeDuration", rendererSettings.flareFadeDuration);
        LOAD_VAL(flare, "brightness", rendererSettings.flareBrightness);
    }

    SetupUniforms();
    if (rendererSettings.msaa != MSAASetting::None) UpdateMSAAFramebuffer();
}