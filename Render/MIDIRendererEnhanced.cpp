#include "MIDIRendererEnhanced.h"
#include <glm/glm.hpp>
#include "App/MIDIApp.h"
#include "App/Models.h"

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

    float aspect = (float)width / (float)height;
    keyboardHeight = keyboardMaxZ * aspect;

    #pragma region Note buffers + data

    notesProgram = ShaderProgram::CreateFromFiles("assets/shaders/notes3d");
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

        notesVAO->SetFloatAttribute(1, 1, sizeof(RenderNote), offsetof(RenderNote, left));
        notesVAO->SetFloatAttribute(2, 1, sizeof(RenderNote), offsetof(RenderNote, right));
        notesVAO->SetFloatAttribute(3, 1, sizeof(RenderNote), offsetof(RenderNote, start));
        notesVAO->SetFloatAttribute(4, 1, sizeof(RenderNote), offsetof(RenderNote, end));
        notesVAO->SetIntAttribute(5, 1, sizeof(RenderNote), offsetof(RenderNote, color));

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

    keyboardProgram = ShaderProgram::CreateFromFiles("assets/shaders/keyboard3d");

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
    saberProgram = ShaderProgram::CreateFromFiles("assets/shaders/saber3d");
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
    mistProgram = ShaderProgram::CreateFromFiles("assets/shaders/mist3d");
    mistQuad = std::make_unique<Quad>();
    mistQuad->SetShader(mistProgram);
    #pragma endregion

    #pragma region Particle buffers + data
    particleProgram = ShaderProgram::CreateFromFiles("assets/shaders/particle3d");
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

    #pragma region post processing setup
    downsampleShader = ShaderProgram::CreateFromFiles("assets/shaders/postProcessing/fullscreen", "assets/shaders/postProcessing/downsample");
    upsampleShader = ShaderProgram::CreateFromFiles("assets/shaders/postProcessing/fullscreen", "assets/shaders/postProcessing/upsample");
    compositeShader = ShaderProgram::CreateFromFiles("assets/shaders/postProcessing/fullscreen", "assets/shaders/postProcessing/composite");

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
    }

    {
        ShaderBind mistBind(*mistProgram);
        mistProgram->SetVec3("mistColor", rendererSettings.saberColor);
        mistProgram->SetFloat("mistOpacity", rendererSettings.mistOpacity);
        mistProgram->SetFloat("mistSpeed", rendererSettings.mistSpeed);
        mistProgram->SetFloat("mistScale", rendererSettings.mistScale);
    }

    initialized = true;
    CalcKeyPosAndWidth();
    UpdateMSAAFramebuffer();
}

void MIDIRendererEnhanced::LoadSequence(std::shared_ptr<MIDISequence> sequence)
{
    std::lock_guard<std::mutex> lock(renderMutex);

    if (seq != sequence) AbstractMIDIRenderer::UnloadSequence();
    AbstractMIDIRenderer::LoadSequence(sequence);

    colors.LoadColors();
    lastTime = 0;

    for (auto& id : startRenderIDs)
        id = 0;

    for (auto& id : endRenderIDs)
        id = 0;
}

void MIDIRendererEnhanced::CalcKeyPosAndWidth()
{
    float noteWidth = (float)width / 75.0f;
    float noteWidthBlack = (float)width / 115.0f;
    float pos = 0.0f;

    for (int i = 0; i < 128; i++)
    {
        keyPos[i] = pos / (float)width;
        pos += keyPosDiff[i % 12] * noteWidth;
    }

    int lastIdxWhite = -1;
    for (int j = 0; j < 128; j++)
    {
        if (KEY_IS_BLACK(j))
            keyWidth[j] = noteWidthBlack / (float)width;
        else
        {
            if (lastIdxWhite != -1) keyWidth[lastIdxWhite] = keyPos[j] - keyPos[lastIdxWhite];
            lastIdxWhite = j;
        }
    }
    if (lastIdxWhite != -1) keyWidth[lastIdxWhite] = 1.0f - keyPos[lastIdxWhite];

    keyboardDirty = true;
}

void MIDIRendererEnhanced::UpdateKeyboardInstance(double deltaTime)
{
    bool needsUpload = false;

    // cap the dt so physics does not explode lol
    float dt = static_cast<float>(min(deltaTime, 0.033));

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
    keyboardZOffset = (visibleHeight / 2.0f) - keyboardMaxZ;

    glm::mat4 projection = glm::perspective(verticalFOVRad, aspect, 0.1f, 100.0f);

    glm::mat4 view = GetViewMatrixFromEuler();
    glm::vec3 cameraPos = glm::vec3(glm::inverse(view)[3]);

    keyboardProgram->SetMat4("projection", projection);
    keyboardProgram->SetMat4("view", view);
    keyboardProgram->SetFloat("keyboardZOffset", keyboardZOffset);
    keyboardProgram->SetVec3("cameraPos", cameraPos);
    keyboardProgram->SetFloat("animTime", app->GetTimer()->Elapsed());


    {
        VertexArrayBind whiteBind(*whiteKeyVAO);
        glDrawElementsInstanced(GL_TRIANGLES, Models::WhiteKeyMesh->indices.size(), GL_UNSIGNED_INT, nullptr, numWhiteKeys);
    }

    {
        VertexArrayBind blackBind(*blackKeyVAO);
        glDrawElementsInstanced(GL_TRIANGLES, Models::BlackKeyMesh->indices.size(), GL_UNSIGNED_INT, nullptr, numBlackKeys);
    }
}

void MIDIRendererEnhanced::RenderNotes()
{
    if (!seq) return;
    std::vector<std::vector<NoteEvent>>& notes = seq->mergedNotes;
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

    notesProgram->SetFloat("kbHeight", keyboardHeight);
    notesProgram->SetFloat("animTime", static_cast<float>(playbackSeconds));

    for (uint8_t id : kbIDs)
    {
        std::vector<NoteEvent>& notesNote = notes[id];
        
        #pragma region Note culling

        size_t noteBegin = startRenderIDs[id];

        if (lastTime < time)
        {
            while (noteBegin < notesNote.size())
            {
                auto& n = notesNote[noteBegin];
                double noteEnd = isTimeBased
                    ? (double)(n.tick + n.gate) / TIME_BASED_MULTIPLIER
                    : (double)(n.tick + n.gate);

                if (noteEnd > accTime) break; // note is still on screen
                ++noteBegin;
            }
        }
        else if (lastTime > time)
        {
            while (noteBegin > 0)
            {
                auto& n = notesNote[noteBegin - 1];
                double noteEnd = isTimeBased
                    ? (double)(n.tick + n.gate) / TIME_BASED_MULTIPLIER
                    : (double)(n.tick + n.gate);

                if (noteEnd <= accTime) break;
                --noteBegin;
            }
        }

        auto searchStart = notesNote.begin() + noteBegin;
        auto endIt = notesNote.end();

        if (isTimeBased)
        {
            double targetSecs = playbackSeconds + viewRegion;

            long target10Us = static_cast<long>(targetSecs * TIME_BASED_MULTIPLIER);

            endIt = std::upper_bound(searchStart, notesNote.end(), target10Us,
                [](long target, const NoteEvent& n)
                {
                    return target < n.tick;
                }
            );
        }
        else
        {
            long targetTick = time + renderView->viewTicks;

            endIt = std::upper_bound(searchStart, notesNote.end(), targetTick,
                [](long target, const NoteEvent& n)
                {
                    return target < n.tick;
                }
            );
        }

        size_t noteEnd = endIt - notesNote.begin();
        startRenderIDs[id] = noteBegin;
        endRenderIDs[id] = noteEnd;
        notesPassed += noteBegin;

        #pragma endregion

        for (auto note = notesNote.begin() + noteBegin; note != notesNote.begin() + noteEnd; ++note)
        {
            auto& n = *note;
            double noteStart = 0.0;
            double noteEnd = 0.0;

            if (isTimeBased)
            {
                noteStart = (double)note->tick / (double)TIME_BASED_MULTIPLIER;
                noteEnd = (double)(note->tick + note->gate) / (double)TIME_BASED_MULTIPLIER;
            }
            else
            {
                noteStart = note->tick;
                noteEnd = note->tick + note->gate;
            }

            if (noteEnd <= accTime)
            {
                notesPassed++;
                continue;
            }

            if (noteStart <= accTime)
            {
                uint32_t color = colors.GetColor(note->track, note->channel);
                keyMetas[n.note].MarkPressed(true);
                keyMetas[n.note].color = color;
                keyboardDirty = true;

                notesPassed++;
                polyphony++;

                // if (particleEmissionTimers[id] >= EMISSION_COOLDOWN)
                // {
                //     EmitNoteExplosion(id, color);
                //     particleEmissionTimers[id] = 0.0f;
                // }
            }

            renderNotes[noteID++] = RenderNote3D(
                keyPos[id],
                keyPos[id] + keyWidth[id],
                (float)(noteStart - accTime) / (float)viewRegion,
                (float)(noteEnd - accTime) / (float)viewRegion,
                colors.GetColor(note->track, note->channel)
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
        noteCounterInfo->notesPerSecond.value = static_cast<uint64_t>(notesPassed) - notesOneSecondAgo;
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

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0.0f, exactTopY, keyboardZOffset - 0.001f));
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

    glm::vec3 hdrColor = glm::vec3(r, g, b) * 15.0f;

    float spawnX = keyPos[keyID];
    const int count = 12;

    const float spreadDegrees = 30.0f;
    const float minAngle = 90.0f - spreadDegrees * 0.5f;
    const float maxAngle = 90.0f + spreadDegrees * 0.5f;

    for (int i = 0; i < count; i++)
    {
        if (liveParticleCount >= PARTICLE_BUFFER_SIZE) break;
        Particle3D& p = particlePool[liveParticleCount++];
        p.position = glm::vec3(spawnX + ((rand() / (float)RAND_MAX)) * keyWidth[keyID], keyboardHeight, 0.0f);

       float angle = glm::radians(
            minAngle +
            ((rand() / (float)RAND_MAX) * (maxAngle - minAngle)));
        float speed = ((rand() % 100) / 100.0f) * 0.1f + 0.05f;

        p.velocity.x = cos(angle) * speed * 0.2f;
        p.velocity.y = sin(angle) * speed * 1.2f;

        p.curveSeed = (rand() / (float)RAND_MAX) * 6.2831853f;
        p.curveSpeed = 4.0f + ((rand() / (float)RAND_MAX) * 3.0f);
        p.curveAmp = 0.03f + ((rand() / (float)RAND_MAX) * 0.05f);
        
        p.color = glm::vec4(hdrColor, 1.0f);
        p.maxLife = ((rand() % 100) / 100.0f) * 2.0f + 0.2f;
        p.life = p.maxLife;
        p.scale = 0.002f;
    }
}

void MIDIRendererEnhanced::UpdateParticles(double deltaTime)
{
    float dt = static_cast<float>(deltaTime);
    
    for (size_t i = 0; i < liveParticleCount; )
    {
        Particle3D& p = particlePool[i];
        p.life -= dt;
        if (p.life <= 0.0f)
        {
            particlePool[i] = particlePool[--liveParticleCount];
            continue;
        }

        float t = p.maxLife - p.life;
        float curve = std::sin(t * p.curveSpeed + p.curveSeed) * p.curveAmp;
        p.velocity.x += curve * dt;

        p.position += p.velocity * dt;
        p.color.a = p.life / p.maxLife;
        p.scale = p.life / p.maxLife * 0.002f;
        ++i;
    }

    for (uint8_t id : kbIDs)
    {
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
        if (timer >= EMISSION_COOLDOWN)
        {
            timer = 0.0f;
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

void MIDIRendererEnhanced::Render(double deltaTime)
{
	if (!initialized) return;
    
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

#define IMGUI_RADIO_BUTTON(label, variable, value) \
    if (ImGui::RadioButton(label, variable == value)) variable = value

void MIDIRendererEnhanced::RenderSettings()
{
    if (ImGui::BeginTabBar("##renderSettings"))
    {
        if (ImGui::BeginTabItem("Visual"))
        {
            ImGui::Text("Exposure");
            ImGui::SameLine();
            ImGui::SliderFloat("##exposure", &rendererSettings.exposure, 0.0f, 5.0f);

            ImGui::Text("Anti-aliasing");

            MSAASetting previousMSAA = rendererSettings.msaa;

            IMGUI_RADIO_BUTTON("None", rendererSettings.msaa, MSAASetting::None);
            IMGUI_RADIO_BUTTON("Low (2x2)", rendererSettings.msaa, MSAASetting::AA2x2);
            IMGUI_RADIO_BUTTON("Medium (4x4)", rendererSettings.msaa, MSAASetting::AA4x4);
            IMGUI_RADIO_BUTTON("High (6x6)", rendererSettings.msaa, MSAASetting::AA6x6);

            if (previousMSAA != rendererSettings.msaa) UpdateMSAAFramebuffer();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Notes"))
        {
            ImGui::Text("Note outline emission");
            ImGui::SameLine();
            if (ImGui::SliderFloat("##noteOutlineGlow", &rendererSettings.noteOutlineGlowFactor, 1.0f, 8.0f))
            {
                ShaderBind notesBind(*notesProgram);
                notesProgram->SetFloat("noteOutlineGlow", rendererSettings.noteOutlineGlowFactor);
            }

            ImGui::Spacing();
            ImGui::Text("Enable HSV shift");
            ImGui::SameLine();
            if (ImGui::Checkbox("##hsvShift", &rendererSettings.hsvShiftEnabled))
            {
                ShaderBind notesBind(*notesProgram);
                notesProgram->SetInt("noteHsvShiftEnabled", rendererSettings.hsvShiftEnabled ? 1 : 0);
            }
            if (rendererSettings.hsvShiftEnabled)
            {
                ImGui::Text("HSV strength");
                ImGui::SameLine();
                if (ImGui::SliderFloat("##hsvStrength", &rendererSettings.hsvShiftStrength, 0.0f, 1.0f))
                {
                    ShaderBind notesBind(*notesProgram);
                    notesProgram->SetFloat("noteHsvShiftStrength", rendererSettings.hsvShiftStrength);
                }

                bool shouldForwardUniform = false;

                ImGui::Text("Hue shift");
                ImGui::SameLine();
                if (ImGui::SliderFloat("##hsvHueShift", &rendererSettings.hsvShifts.x, -1.0f, 1.0f))
                {
                    shouldForwardUniform = true;
                }

                ImGui::Text("Saturation shift");
                ImGui::SameLine();
                if (ImGui::SliderFloat("##hsvSatShift", &rendererSettings.hsvShifts.y, -1.0f, 0.0f))
                {
                    shouldForwardUniform = true;
                }

                ImGui::Text("Value shift");
                ImGui::SameLine();
                if (ImGui::SliderFloat("##hsvValShift", &rendererSettings.hsvShifts.z, -1.0f, 0.0f))
                {
                    shouldForwardUniform = true;
                }

                if (shouldForwardUniform)
                {
                    ShaderBind notesBind(*notesProgram);
                    notesProgram->SetVec3("noteHsvShifts", rendererSettings.hsvShifts);
                }
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Keyboard"))
        {
            ImGui::Text("Key glow factor");
            ImGui::SameLine();
            if (ImGui::SliderFloat("##keyGlowFactor", &rendererSettings.keyGlowFactor, 1.0f, 8.0f))
            {
                ShaderBind keyboardBind(*keyboardProgram);
                keyboardProgram->SetFloat("keyGlowFactor", rendererSettings.keyGlowFactor);
            }
            ImGui::Text("Keyboard FOV");
            ImGui::SameLine();
            ImGui::SliderFloat("##keyboardFov", &rendererSettings.keyboardFOV, 20.0f, 89.9f);

            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Saber"))
        {
            ImGui::Text("Color");
            ImGui::SameLine();
            if (ImGui::ColorEdit3("##saberColor", &rendererSettings.saberColor.x))
            {
                ShaderBind mistBind(*mistProgram);
                mistProgram->SetVec3("mistColor", rendererSettings.saberColor);
            }
            ImGui::Text("Brightness");
            ImGui::SameLine();
            ImGui::SliderFloat("##saberBrightness", &rendererSettings.saberBrightness, 0.0f, 20.0f);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Mist"))
        {
            ImGui::Text("Enable mist");
            ImGui::SameLine();
            ImGui::Checkbox("##enableMist", &rendererSettings.mistEnabled);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Particle system"))
        {
            ImGui::Text("Enable particles");
            ImGui::SameLine();
            ImGui::Checkbox("##enableParticlles", &rendererSettings.particlesEnabled);
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

    glBufferSubData(GL_ARRAY_BUFFER,
        0,
        count * sizeof(RenderNote),
        renderNotes.data());

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
    }
}

void MIDIRendererEnhanced::OnResize(int width, int height)
{
    AbstractMIDIRenderer::OnResize(width, height);
    this->width = width;
    this->height = height;

    float aspect = (float)width / (float)height;
    keyboardHeight = keyboardMaxZ * aspect;
    if (!initialized) return;
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