#include "MIDIRendererMIDITrail.h"
#include <algorithm>
#include "App/MIDIApp.h"
#include "MIDI/TempoMap.h"
#include "Utils.h"

const std::array<float, 24> CUBE_VERTICES = {
	0.0f, 1.0f, 0.0f,
	1.0f, 1.0f, 0.0f,
	1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.0f,

	0.0f, 1.0f, 1.0f,
	1.0f, 1.0f, 1.0f,
	1.0f, 0.0f, 1.0f,
	0.0f, 0.0f, 1.0f,
};

const std::array<unsigned int, 36> CUBE_INDICES = {
	// top
	0, 4, 1,
	1, 4, 5,
	// bottom
	3, 2, 7,
	2, 6, 7,
	// front
	0, 1, 3,
	1, 2, 3,
	// back
	5, 4, 7,
	7, 6, 5,
	// left
	0, 3, 4,
	3, 7, 4,
	// right
	1, 5, 2,
	2, 5, 6,
};

// will be shared between white and black key shaders, as they are similar
static const char* keyShaderVert = R"(#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in float kColorFac;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform float left;
uniform float right;
uniform float kbHeight;
uniform float pressFactor;

out float colorFac;

void main()
{
	// 0.99 factor for a really small gap between keys so the keys don't look like one big rectangle. this is a hacky solution but it works for now.
	float width = (right - left) * 0.94f;
	vec3 vPos = vec3(aPos.x * width + left, aPos.y * 0.012f, aPos.z * kbHeight);
	// translation for now
	vPos.y -= pressFactor * 0.006f;
	gl_Position = projection * view * model * vec4(vPos, 1.0);
	colorFac = kColorFac;
})";

static const char* keyShaderFrag = R"(#version 330 core
in float colorFac;

uniform uint meta;
out vec4 fragColor;

void main()
{
	// decode color from meta
	vec3 color = vec3(
		float((meta >> 16u) & 0xFFu) / 255.0,
		float((meta >> 8u) & 0xFFu) / 255.0,
		float(meta & 0xFFu) / 255.0
	);
	// other flags, like key press, etc. decoded
	// branchless color mixing based on flags
	float pressFac = float((meta >> 24u) & 0x1u);
	float blackFac = float((meta >> 25u) & 0x1u);
	vec3 finalColor = mix(vec3(1.0 - blackFac) * 0.8 + 0.2, color, pressFac);

	fragColor = vec4(finalColor * colorFac, 1.0);
})";

static const char* noteShaderVert = R"(#version 330 core
layout (location = 0) in vec3 aPos;

layout (location = 1) in float nLeft;
layout (location = 2) in float nRight;
layout (location = 3) in float nStart;
layout (location = 4) in float nEnd;
layout (location = 5) in uint nMeta;

flat out uint meta;
out vec3 fragPos;
out float noteY;

uniform float kbHeight;
uniform mat4 model;
uniform mat4 projection;
uniform mat4 view;

uniform bool eatNotes;

void main()
{
	float start = eatNotes ? max(nStart, 0.0) : nStart;
	float end = eatNotes ? max(nEnd, 0.0) : nEnd;

	float x = mix(nLeft, nRight, aPos.x);
	float z = mix(start, end, aPos.z);
	z = -z * (1.0 - kbHeight) + kbHeight;

	vec3 vPos = vec3(x, aPos.y * 0.007 - 0.003, z);
	gl_Position = projection * view * model * vec4(vPos, 1.0);
	meta = nMeta;
	fragPos = gl_Position.xyz;
	noteY = aPos.z;
})";

static const char* noteShaderFrag = R"(#version 330 core
flat in uint meta;

in vec3 fragPos;
out vec4 fragColor;
in float noteY;

uniform float noteTransparency;

void main()
{
	vec3 color = vec3(
		float((meta >> 16u) & 0xFFu) / 255.0,
		float((meta >> 8u) & 0xFFu) / 255.0,
		float(meta & 0xFFu) / 255.0
	);

	// vec3 normal = normalize(cross(dFdx(fragPos), dFdy(fragPos)));
	// vec3 lightDir = normalize(vec3(0.45, -1.0, 0.45));
    // float diffuse = max(dot(normal, lightDir), 0.0);
	// if (diffuse < 0.001) diffuse = max(dot(normal, -lightDir), 0.0);

	vec3 finalColor = mix(color, color + 0.5, float((meta >> 25u) & 1u));
	fragColor = vec4(finalColor, mix(1.0, 1.0 - noteY, noteTransparency));
})";

static const char* separatorLineVert = R"(#version 330 core

layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
	gl_Position = projection * view * model * vec4(aPos, 1.0);
})";

static const char* separatorLineFrag = R"(#version 330 core

out vec4 fragColor;

uniform float lineTransparency;

void main()
{
	fragColor = vec4(vec3(1.0), 1.0 - lineTransparency);
})";

static const char* measureLineVert = R"(#version 330 core
layout (location = 0) in float aPos;
layout (location = 1) in float aTime;

uniform float kbHeight;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
	float z = -aTime;
	z = z * (1.0 - kbHeight) + kbHeight;
	gl_Position = projection * view * model * vec4(aPos, 0.003, z, 1.0);
})";

static const char* auraVert = R"(#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in float size;
layout (location = 2) in float pos;
layout (location = 3) in uint aMeta;

flat out uint meta;
out vec2 uv;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float kbHeight;

void main()
{
	vec2 vPos = aPos * size;
	vPos.x += pos;
	gl_Position = projection * view * model * vec4(vPos, -kbHeight, 1.0);
	uv = aPos * 0.5 + 0.5;
	uv.y = 1.0 - uv.y;
	meta = aMeta;
})";

static const char* auraFrag = R"(#version 330 core

flat in uint meta;
in vec2 uv;

uniform sampler2D auraTexture;

out vec4 fragColor;

void main()
{
	vec3 color = vec3(
		float((meta >> 16u) & 0xFFu) / 255.0,
		float((meta >> 8u) & 0xFFu) / 255.0,
		float(meta & 0xFFu) / 255.0
	);

	vec4 out_ = vec4(color, 1.0) * texture(auraTexture, uv);

	fragColor = out_;
})";

void MIDIRendererMIDITrail::Initialize()
{
	AbstractMIDIRenderer::Initialize();

	#pragma region Notes
	notesProgram = ShaderProgram::Create(noteShaderVert, noteShaderFrag);

	notesVAO = std::make_unique<VertexArray>();
	notesVBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
	notesIBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
	notesEBO = std::make_unique<Buffer>(GL_ELEMENT_ARRAY_BUFFER);

	{
		VertexArrayBind vaoBind(*notesVAO);
		notesVBO->Bind();
		notesVBO->SetData(CUBE_VERTICES, GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);

		notesEBO->Bind();
		notesEBO->SetData(CUBE_INDICES, GL_STATIC_DRAW);

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

	whiteKeyProgram = ShaderProgram::Create(keyShaderVert, keyShaderFrag);
	blackKeyProgram = ShaderProgram::Create(keyShaderVert, keyShaderFrag);
	
	whiteKeyVAO = std::make_unique<VertexArray>();
	whiteKeyEBO = std::make_unique<Buffer>(GL_ELEMENT_ARRAY_BUFFER);

	blackKeyVAO = std::make_unique<VertexArray>();
	blackKeyEBO = std::make_unique<Buffer>(GL_ELEMENT_ARRAY_BUFFER);
	// too lazy to make shader for notes atm. will do later

	{
		VertexArrayBind vaoBind(*whiteKeyVAO);
		// vertex attributes
		whiteKeyVAO->SetFloatAttribute(0, 3, sizeof(float) * 3, 0);
		whiteKeyVAO->SetFloatAttribute(1, 1, sizeof(float), 0);
	}

	{
		VertexArrayBind vaoBind(*blackKeyVAO);
		// vertex attributes
		blackKeyVAO->SetFloatAttribute(0, 3, sizeof(float) * 3, 0);
	}

	std::vector<uint8_t> blackIDs;
	std::vector<uint8_t> whiteIDs;
	for (uint8_t key = 0; key < MIDI_KEYS; key++)
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

	// white key and black key models
	float whitekeylen = 5.0f;
	float blackkeylen = 6.9f;
	float lenfac = 0.64f;
	float blackKeyEnd = whitekeylen * lenfac;
	float left = 0.3f;
	float right = 0.7f;

	#pragma region White Key Model
	{
		std::array<float, 156> verts = {
			// front
			0, 0, whitekeylen,
			0, 0.7, whitekeylen,
			1, 0.7, whitekeylen,
			1, 0, whitekeylen,
			// front dark
			0, 0.7, whitekeylen,
			0, 1, whitekeylen,
			1, 1, whitekeylen,
			1, 0.7, whitekeylen,
			// top
			1, 1, blackKeyEnd,
			1, 1, whitekeylen,
			0, 1, whitekeylen,
			0, 1, blackKeyEnd,
			// notch
			0, 1, whitekeylen,
			0.03f, 0.95f, whitekeylen + 0.1,
			0.97f, 0.95f, whitekeylen + 0.1,
			1, 1, whitekeylen,

			0, 0.90, whitekeylen - 0.07,
			0.03, 0.95, whitekeylen - 0.1,
			0.97, 0.95, whitekeylen - 0.1,
			1, 0.90, whitekeylen - 0.07,
			// left
			0, 1, blackKeyEnd,//60
			0, 1, whitekeylen,
			0, 0, whitekeylen,
			0, 0, blackKeyEnd,
			//right
			1, 1, blackKeyEnd,//72
			1, 1, whitekeylen,
			1, 0, whitekeylen,
			1, 0, blackKeyEnd,
			//back
			left, 0, 0,//84
			left, 1, 0,
			right, 1, 0,
			right, 0, 0,

			//left2
			left, 1, 0,//96
			left, 1, blackKeyEnd,
			left, 0, blackKeyEnd,
			left, 0, 0,
			//right2
			right, 1, 0,//108
			right, 1, blackKeyEnd,
			right, 0, blackKeyEnd,
			right, 0, 0,
			//top
			left, 1, 0,//120
			left, 1, blackKeyEnd,
			right, 1, blackKeyEnd,
			right, 1, 0,
			//left inner
			0, 1, blackKeyEnd,//96
			left, 1, blackKeyEnd,
			left, 0, blackKeyEnd,
			0, 0, blackKeyEnd,
			//right inner
			1, 1, blackKeyEnd,//108
			right, 1, blackKeyEnd,
			right, 0, blackKeyEnd,
			1, 0, blackKeyEnd,
		};

		std::array<float, 52> colors = {
			//front
			0.7f,
			0.8f,
			0.8f,
			0.7f,
			//front dark
			0.6f,
			0.6f,
			0.6f,
			0.6f,
			//top
			1,
			1,
			1,
			1,
			//notch
			1.0f,
			0.9f,
			0.9f,
			1.0f,

			0.9f,
			0.9f,
			0.9f,
			0.9f,
			//left
			0.6f,
			0.6f,
			0.6f,
			0.6f,
			//right
			0.6f,
			0.6f,
			0.6f,
			0.6f,
			//back
			0.8f,
			0.8f,
			0.8f,
			0.8f,
			//left2
			0.6f,
			0.6f,
			0.6f,
			0.6f,
			//right2
			0.6f,
			0.6f,
			0.6f,
			0.6f,
			//top2
			1,
			1,
			1,
			1,
			//left inner
			0.6f,
			0.6f,
			0.6f,
			0.6f,
			//right inner
			0.6f,
			0.6f,
			0.6f,
			0.6f,
		};

		whiteKeyColVBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
		whiteKeyColVBO->SetData(colors, GL_STATIC_DRAW);

		int rightIndices[] = {
			7 * 12 + 6, 7 * 12 + 9,
			9 * 12, 9 * 12 + 3, 9 * 12 + 6, 9 * 12 + 9,
			10 * 12 + 6, 10 * 12 + 9,
			12 * 12 + 3, 12 * 12 + 6,
		};

		int leftIndices[] = {
			7 * 12, 7 * 12 + 3,
			8 * 12, 8 * 12 + 3, 8 * 12 + 6, 8 * 12 + 9,
			10 * 12, 10 * 12 + 3,
			11 * 12 + 3, 11 * 12 + 6,
		};

		std::array<int, 52> indices{};
		for (int i = 0; i < 52; i++)
		{
			indices[i] = i;
		}

		float offsets[] = {
			0, 0.6,
			0.2, 0.8,
			0.4, 1.0,
			0, 0.55,
			0.15, 0.7,
			0.3, 0.85,
			0.45, 1
		};

		std::vector<unsigned int> triIndices;
		for (unsigned int i = 0; i < 13; ++i) {
			unsigned int base = i * 4;
			triIndices.insert(triIndices.end(), {
				base, base + 1, base + 2,
				base + 2, base + 3, base
				});
		}

		VertexArrayBind vaoBind(*whiteKeyVAO);
		whiteKeyEBO->SetData(triIndices, GL_STATIC_DRAW);

		for (int i = 0; i < 7; i++)
		{
			auto& whiteKeyVBO = whiteKeyVBOs[i];
			for (int j = 0; j < sizeof(leftIndices) / sizeof(leftIndices[0]); j++) verts[leftIndices[j]] = offsets[i * 2];
			for (int j = 0; j < sizeof(rightIndices) / sizeof(rightIndices[0]); j++) verts[rightIndices[j]] = offsets[i * 2 + 1];
			whiteKeyVBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
			whiteKeyVBO->SetData(verts, GL_STATIC_DRAW);
		}

		for (int i = 0; i < 7; i++)
		{
			auto& whiteKeyVBO = whiteKeyVBOs[i + 7];
			for (int j = 0; j < sizeof(leftIndices) / sizeof(leftIndices[0]); j++) verts[leftIndices[j]] = offsets[i * 2];
			for (int j = 0; j < sizeof(rightIndices) / sizeof(rightIndices[0]); j++) verts[rightIndices[j]] = 1;
			whiteKeyVBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
			whiteKeyVBO->SetData(verts, GL_STATIC_DRAW);
		}

		for (int i = 0; i < 7; i++)
		{
			auto& whiteKeyVBO = whiteKeyVBOs[i + 14];
			for (int j = 0; j < sizeof(leftIndices) / sizeof(leftIndices[0]); j++) verts[leftIndices[j]] = 0;
			for (int j = 0; j < sizeof(rightIndices) / sizeof(rightIndices[0]); j++) verts[rightIndices[j]] = offsets[i * 2 + 1];
			whiteKeyVBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
			whiteKeyVBO->SetData(verts, GL_STATIC_DRAW);
		}
	}
	#pragma endregion

	#pragma region Black key model
	{
		std::array<float, 96> verts = {
			// front
			0, 0, blackKeyEnd,
			0, 1, blackKeyEnd - 0.4,
			1, 1, blackKeyEnd - 0.4,
			1, 0, blackKeyEnd,
			// top
			0, 1, blackKeyEnd - 0.4,
			0, 1, 0,
			1, 1, 0,
			1, 1, blackKeyEnd - 0.4,
			// left
			0, 0, 0,
			0, 0, blackKeyEnd,
			0, 1, blackKeyEnd - 0.4,
			0, 1, 0,
			// right
			1, 0, 0,
			1, 0, blackKeyEnd,
			1, 1, blackKeyEnd - 0.4,
			1, 1, 0,
			// back
			0, -1, 0,
			0, 1, 0,
			1, 1, 0,
			1, -1, 0,
			// left2
			0, 0, 0,
			0, 0, blackKeyEnd,
			0, -1, blackKeyEnd,
			0, -1, 0,
			// right2
			1, 0, 0,
			1, 0, blackKeyEnd,
			1, -1, blackKeyEnd,
			1, -1, 0,
			// front2
			0, 0, blackKeyEnd,
			0, -1, blackKeyEnd,
			1, -1, blackKeyEnd,
			1, 0, blackKeyEnd
		};

		std::array<float, 32> colors = {
			// front
			0.95f,
			1.00f,
			1.00f,
			0.95f,

			// top (bright front -> much darker rear)
			1.00f,
			0.42f,
			0.42f,
			1.00f,

			// left
			0.88f,
			1.00f,
			0.35f,
			0.18f,

			// right
			0.88f,
			1.00f,
			0.35f,
			0.18f,

			// back
			0.12f,
			0.12f,
			0.12f,
			0.12f,

			// left2
			0.40f,
			0.80f,
			0.15f,
			0.06f,

			// right2
			0.40f,
			0.80f,
			0.15f,
			0.06f,

			// front2
			0.95f,
			1.00f,
			1.00f,
			0.95f,
		};

		blackKeyColVBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
		blackKeyColVBO->SetData(colors, GL_STATIC_DRAW);

		for (int i = 0; i < verts.size(); i += 3)
		{
			// move the black key up a bit so it doesn't intersect with the white key
			if (verts[i + 1] > 0.9f)
			{
				verts[i + 1] = 1.7f;
			}
			else
			{
				verts[i + 1]++;
			}
		}

		std::array<int, 32> indices{};
		for (int i = 0; i < 32; i++)
		{
			indices[i] = i;
		}

		// triangle indices
		std::vector<unsigned int> triIndices;
		for (unsigned int i = 0; i < 8; ++i) {
			unsigned int base = i * 4;
			triIndices.insert(triIndices.end(), {
				base, base + 1, base + 2,
				base + 2, base + 3, base
				});
		}

		VertexArrayBind vaoBind(*blackKeyVAO);
		blackKeyEBO->SetData(triIndices, GL_STATIC_DRAW);

		blackKeyVBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
		blackKeyVBO->SetData(verts, GL_STATIC_DRAW);
	}
	#pragma endregion

	CalcKeyPosAndWidth();

	#pragma region Separator lines
	separatorProgram = ShaderProgram::Create(separatorLineVert, separatorLineFrag);
	separatorVAO = std::make_unique<VertexArray>();
	separatorVBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
	{
		VertexArrayBind vaoBind(*separatorVAO);
		separatorVBO->Bind();
		separatorVBO->SetData(separatorVerts, GL_STATIC_DRAW);
	
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	}
	UpdateSeparatorLines(settings.frontRenderCutoff, settings.backRenderCutoff);
	#pragma endregion

	#pragma region Measure lines
	measureProgram = ShaderProgram::Create(measureLineVert, separatorLineFrag);
	measureVAO = std::make_unique<VertexArray>();
	measureVBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
	measureIBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
	measureEBO = std::make_unique<Buffer>(GL_ELEMENT_ARRAY_BUFFER);

	std::vector<float> measureVerts{ 0.0, 1.0 };
	std::vector<int> measureIndices{ 0, 1 };
	{
		VertexArrayBind vaoBind(*measureVAO);
		
		measureVBO->Bind();
		measureVBO->SetData(measureVerts, GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, sizeof(float), (void*)0);

		measureEBO->Bind();
		measureEBO->SetData(measureIndices, GL_STATIC_DRAW);

		measureIBO->Bind();
		measureIBO->SetData(renderMeasureLines, GL_DYNAMIC_DRAW);

		measureVAO->SetFloatAttribute(1, 1, sizeof(RenderMeasureLine), offsetof(RenderMeasureLine, time));

		glVertexAttribDivisor(1, 1);
	}
	#pragma endregion

	#pragma region Auras
	auraProgram = ShaderProgram::Create(auraVert, auraFrag);
	auraVAO = std::make_unique<VertexArray>();
	auraVBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
	auraIBO = std::make_unique<Buffer>(GL_ARRAY_BUFFER);
	auraEBO = std::make_unique<Buffer>(GL_ELEMENT_ARRAY_BUFFER);

	std::vector<float> auraVerts{
		-1.0,  1.0,
		 1.0,  1.0,
		 1.0, -1.0,
		-1.0, -1.0
	};
	std::vector<int> auraIndices{
		0, 1, 3,
		1, 2, 3
	};

	{
		VertexArrayBind vaoBind(*auraVAO);

		auraVBO->Bind();
		auraVBO->SetData(auraVerts, GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

		auraEBO->Bind();
		auraEBO->SetData(auraIndices, GL_STATIC_DRAW);

		auraIBO->Bind();
		auraIBO->SetData(renderAuras, GL_DYNAMIC_DRAW);

		auraVAO->SetFloatAttribute(1, 1, sizeof(RenderAura), offsetof(RenderAura, size));
		auraVAO->SetFloatAttribute(2, 1, sizeof(RenderAura), offsetof(RenderAura, pos));
		auraVAO->SetIntAttribute(3, 1, sizeof(RenderAura), offsetof(RenderAura, meta));

		glVertexAttribDivisor(1, 1);
		glVertexAttribDivisor(2, 1);
		glVertexAttribDivisor(3, 1);
	}

	LoadAuraTexture();
	{
		ShaderBind shaderBind(*auraProgram);

		{
			TextureBind bind(*auraTexture, 0);
			auraProgram->SetInt("auraTexture", 0);
		}
	}
	#pragma endregion
}

void MIDIRendererMIDITrail::CalcKeyPosAndWidth()
{
	float noteWidth = (float)width / 75.0f;
	float noteWidthBlack = (float)width / 115.0f;
	float pos = 0.0f;

	for (int i = 0; i < MIDI_KEYS; i++)
	{
		keyPos[i] = pos / (float)width;
		pos += keyPosDiff[i % 12] * noteWidth;
	}

	int lastIdxWhite = -1;
	for (int j = 0; j < MIDI_KEYS; j++)
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

	separatorVerts.clear();

	separatorVerts.emplace_back(0.0f, 0.003f, 0.0f);
	separatorVerts.emplace_back(0.0f, 0.003f, -10.0f);
	separatorVerts.emplace_back(1.0f, 0.003f, 0.0f);
	separatorVerts.emplace_back(1.0f, 0.003f, -10.0f);

	separatorVerts.emplace_back(0.0f, -0.004f, 0.0f);
	separatorVerts.emplace_back(0.0f, -0.004f, -10.0f);
	separatorVerts.emplace_back(1.0f, -0.004f, 0.0f);
	separatorVerts.emplace_back(1.0f, -0.004f, -10.0f);
}

void MIDIRendererMIDITrail::LoadSequence(std::shared_ptr<MIDISequence> sequence)
{
	if (seq != sequence) AbstractMIDIRenderer::UnloadSequence();
	AbstractMIDIRenderer::LoadSequence(sequence);

	colors.LoadColors();
	seq = sequence;
	lastTime = 0;

	for (auto& id : startRenderIDs)
		id = 0;

	for (auto& id : endRenderIDs)
		id = 0;
}

void MIDIRendererMIDITrail::Render(double deltaTime)
{
	// Render the scene to the framebuffer
	UpdateMatrices();
	UpdateAuras(deltaTime);

	sceneFramebuffer->Bind();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if (settings.showOuterBorders)
	{
		RenderSeparatorLines();
	}

	if (settings.showMeasureLines)
	{
		RenderMeasureLines();
	}
	
	RenderNotes();
	glDisable(GL_BLEND);

	glEnable(GL_DEPTH_TEST);

	if (settings.showAura)
	{
		glDepthFunc(GL_ALWAYS);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		glDepthMask(GL_FALSE);
		RenderAuras();
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_BLEND);
		glDepthFunc(GL_LESS);
	}

	glDepthMask(GL_TRUE);

	if (settings.showKeyboard)
	{
		UpdateKeyboard(deltaTime);
		RenderKeyboard();
	}

	glDepthFunc(GL_LESS);
	glDisable(GL_DEPTH_TEST);
	sceneFramebuffer->Unbind();
}

void MIDIRendererMIDITrail::RenderSettings()
{
	ImGui::Text("Preset");
	ImGui::SameLine();
	if (ImGui::BeginCombo("##settingsPreset", ""))
	{
		if (ImGui::Selectable("Default"))
		{
			ResetSettings();
		}
		if (ImGui::Selectable("Top Down Above"))
		{
			settings.cameraFOV = 38.354;
			settings.cameraPos = glm::vec3(2.492, 8.105, -4.853);
			settings.cameraRotation = glm::vec3(90, -90, 0);

			settings.backRenderCutoff = 0.269f;
			settings.frontRenderCutoff = 10.0f;

			settings.showMeasureLines = false;
			settings.showOuterBorders = false;
		}
		if (ImGui::Selectable("Sideways"))
		{
			settings.cameraFOV = 59.327;
			settings.cameraPos = glm::vec3(0.499, 1.250, 0.233);
			settings.cameraRotation = glm::vec3(21.794, -42.300, -68.600);

			settings.backRenderCutoff = 0.469;
			settings.frontRenderCutoff = 30.0;

			settings.showMeasureLines = true;
			settings.showOuterBorders = true;
		}
		ImGui::EndCombo();
	}

	// parent function for reset button
	if (ImGui::BeginTabBar("##renderSettings"))
	{
		if (ImGui::BeginTabItem("Rendering"))
		{
			ImGui::Text("3D notes");
			ImGui::SameLine();
			ImGui::Checkbox("##3dNotes", &settings.notes3D);

			ImGui::Text("Eat notes");
			ImGui::SameLine();
			if (ImGui::Checkbox("##eatNotes", &settings.eatNotes))
			{
				if (settings.eatNotes)
					UpdateSeparatorLines(settings.frontRenderCutoff, 0);
				else
					UpdateSeparatorLines(settings.frontRenderCutoff, settings.backRenderCutoff);

				{
					ShaderBind shaderBind(*notesProgram);
					notesProgram->SetInt("eatNotes", settings.eatNotes ? 1 : 0);
				}
			}

			ImGui::Text("Note transparency");
			ImGui::SameLine();
			ImGui::SliderFloat("##noteTransparency", &settings.noteTransparency, 0.0f, 1.0f);

			bool updateSeparatorLines = false;
			ImGui::Spacing();
			ImGui::Text("Front render cutoff");
			ImGui::SameLine();
			updateSeparatorLines = ImGui::SliderFloat("##frontRenderCutoff", &settings.frontRenderCutoff, 1.0f, 40.0f);

			ImGui::Text("Back render cutoff");
			ImGui::SameLine();
			if (ImGui::SliderFloat("##backRenderCutoff", &settings.backRenderCutoff, 0.0f, 20.0f))
			{
				for (auto& id : startRenderIDs)
				{
					id = 0;
				}

				for (auto& id : endRenderIDs)
				{
					id = 0;
				}

				updateSeparatorLines = true;
			}

			if (updateSeparatorLines)
			{
				UpdateSeparatorLines(settings.frontRenderCutoff, settings.eatNotes ? 0 : settings.backRenderCutoff);
			}

			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Keyboard"))
		{
			ImGui::Text("Show keyboard");
			ImGui::SameLine();
			ImGui::Checkbox("##showKeyboard", &settings.showKeyboard);

			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Camera"))
		{
			ImGui::DragFloat3("Camera position", glm::value_ptr(settings.cameraPos), 0.1f);
			ImGui::DragFloat3("Camera rotation", glm::value_ptr(settings.cameraRotation), 0.1f);
			ImGui::SliderFloat("Camera FOV", &settings.cameraFOV, 1.0f, 120.0f);

			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Measure Lines & Borders"))
		{
			ImGui::Text("Show measure lines");
			ImGui::SameLine();
			ImGui::Checkbox("##showMeasureLines", &settings.showMeasureLines);

			ImGui::Text("Show outer borders");
			ImGui::SameLine();
			ImGui::Checkbox("##showBorders", &settings.showOuterBorders);

			ImGui::Text("Line transparency");
			ImGui::SameLine();
			ImGui::SliderFloat("##lineTransparency", &settings.lineTransparency, 0.0f, 1.0f);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Aura"))
		{
			ImGui::Text("Show aura");
			ImGui::SameLine();
			ImGui::Checkbox("##showAura", &settings.showAura);

			ImGui::Text("Aura size");
			ImGui::SameLine();
			ImGui::SliderFloat("##auraSize", &settings.auraSize, 0.03f, 0.08f);
			ImGui::EndTabItem();

			std::filesystem::path path = settings.auraTexture;
			Utils::AddFilePickerField("Aura texture", path, "png,jpg,jpeg");
			if (path != settings.auraTexture)
			{
				settings.auraTexture = path;
				LoadAuraTexture();
			}
		}
		ImGui::EndTabBar();
	}
	AbstractMIDIRenderer::RenderSettings();
}

void MIDIRendererMIDITrail::ResetSettings()
{
	settings = MIDITrailSettings();
	LoadAuraTexture();
}

void MIDIRendererMIDITrail::UpdateKeyboard(double deltaTime)
{
	for (int i = 0; i < MIDI_KEYS; i++)
	{
		auto& kbMeta = keyMetas[i];
		auto& kbData = keyboardData[i];
		bool pressed = kbMeta.pressed;

		kbData.left = keyPos[i];
		kbData.right = keyPos[i] + keyWidth[i];
		kbData.meta = keyMetas[i].GetMeta();

		auto& pressFactor = kbData.pressFactor;
		// TODO: smoothing
		if (pressed)
		{
			pressFactor = std::min<float>(1.0f, pressFactor + settings.noteDownSpeed * deltaTime);
		}
		else
		{
			pressFactor = std::max<float>(0.0f, pressFactor - settings.noteUpSpeed * deltaTime);
		}
	}
}

void MIDIRendererMIDITrail::RenderKeyboard()
{
	// render the keyboard using the white and black key shaders
	if (!whiteKeyProgram || !blackKeyProgram) return;
	
	size_t whiteIdx = 0;
	size_t blackIdx = 0;

	for (int i = 0; i < MIDI_KEYS; i++)
	{
		auto& kbData = keyboardData[i];

		if (KEY_IS_BLACK(i))
		{
			ShaderBind blackKeyBind(*blackKeyProgram);
			blackKeyProgram->SetFloat("kbHeight", keyboardHeight);

			blackKeyProgram->SetMat4("model", model);
			blackKeyProgram->SetMat4("view", view);
			blackKeyProgram->SetMat4("projection", projection);

			blackKeyProgram->SetFloat("left", kbData.left);
			blackKeyProgram->SetFloat("right", kbData.right);
			blackKeyProgram->SetFloat("pressFactor", kbData.pressFactor);
			blackKeyProgram->SetUInt("meta", kbData.meta); // black key color
			
			blackKeyVAO->Bind();
			blackKeyEBO->Bind();

			blackKeyVBO->Bind();
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
			
			blackKeyColVBO->Bind();
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(float), (void*)0);

			glDrawElements(GL_TRIANGLES, 6 * 8, GL_UNSIGNED_INT, 0);

			blackIdx++;
		}
		else
		{
			ShaderBind whiteKeyBind(*whiteKeyProgram);
			whiteKeyProgram->SetFloat("kbHeight", keyboardHeight);

			size_t wIdx = whiteIdx % 7;
			if (i == 127) wIdx = 13; // last white key has no right neighbor, so use the last VBO

			whiteKeyProgram->SetMat4("model", model);
			whiteKeyProgram->SetMat4("view", view);
			whiteKeyProgram->SetMat4("projection", projection);

			whiteKeyProgram->SetFloat("left", kbData.left);
			whiteKeyProgram->SetFloat("right", kbData.right);
			whiteKeyProgram->SetFloat("pressFactor", kbData.pressFactor);
			whiteKeyProgram->SetUInt("meta", kbData.meta); // black key color
			
			whiteKeyVAO->Bind();
			whiteKeyEBO->Bind();
			whiteKeyVBOs[wIdx]->Bind();

			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
			
			whiteKeyColVBO->Bind();
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(float), (void*)0);
			
			glDrawElements(GL_TRIANGLES, 6 * 13, GL_UNSIGNED_INT, 0);

			whiteIdx++;
		}
	}

	// reset metas
	for (int i = 0; i < MIDI_KEYS; i++)
	{
		keyMetas[i].MarkPressed(false);
	}
}

void MIDIRendererMIDITrail::RenderNotes()
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
	noteCounterInfo->tick = time >= 0 ? time : 0;
	noteCounterInfo->timeSeconds = playbackSeconds;
	noteCounterInfo->bpm = bpm;

	ShaderBind shaderBind(*notesProgram);

	VertexArrayBind notesVAOBind(*notesVAO);
	BufferBind notesIBOBind(*notesIBO);
	BufferBind notesVBOBind(*notesVBO);
	BufferBind notesEBOBind(*notesEBO);

	notesProgram->SetFloat("kbHeight", keyboardHeight);
	notesProgram->SetMat4("model", model);
	notesProgram->SetMat4("view", view);
	notesProgram->SetMat4("projection", projection);
	notesProgram->SetFloat("noteTransparency", settings.noteTransparency);

	size_t noteID = 0;
	size_t notesPassed = 0;
	size_t polyphony = 0;
	size_t batchCount = 0;

	const double accTime = isTimeBased ? playbackSeconds : time;
	const double viewRegion = isTimeBased ? (double)renderView->viewTicks / 1000 : (double)renderView->viewTicks;
	const double invViewRegion = 1.0 / viewRegion;
	const double invTimeMultiplier = 1.0 / (double)TIME_BASED_MULTIPLIER;

	double backCutoffTime = settings.eatNotes ? accTime : (isTimeBased
		? accTime - (viewRegion * settings.backRenderCutoff)
		: accTime - (renderView->viewTicks * settings.backRenderCutoff));

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
					? (double)(n.tick + n.gate) * invTimeMultiplier
					: (double)(n.tick + n.gate);

				if (noteEnd > backCutoffTime) break; // Note is still on screen
				++noteBegin;
			}
		}
		else if (lastTime > time)
		{
			while (noteBegin > 0)
			{
				auto& n = notesNote[noteBegin - 1];
				double noteEnd = isTimeBased
					? (double)(n.tick + n.gate) * invTimeMultiplier
					: (double)(n.tick + n.gate);

				if (noteEnd <= backCutoffTime) break;
				--noteBegin;
			}
		}

		auto searchStart = notesNote.begin() + noteBegin;
		auto endIt = notesNote.end();

		if (isTimeBased)
		{
			double targetSecs = playbackSeconds + viewRegion * settings.frontRenderCutoff;
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
			long targetTick = time + renderView->viewTicks * settings.frontRenderCutoff;
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

		bool isBlack = KEY_IS_BLACK(id);
		for (auto note = notesNote.begin() + noteBegin; note != notesNote.begin() + noteEnd; ++note)
		{
			auto& n = *note;
			double noteStart = 0.0;
			double noteEnd = 0.0;

			if (isTimeBased)
			{
				noteStart = (double)note->tick * invTimeMultiplier;
				noteEnd = (double)(note->tick + note->gate) * invTimeMultiplier;
			}
			else
			{
				noteStart = note->tick;
				noteEnd = note->tick + note->gate;
			}

			if (noteEnd <= backCutoffTime)
			{
				notesPassed++;
				continue;
			}

			if (noteEnd <= accTime)
			{
				notesPassed++;
			}

			bool isNoteActive = noteStart <= accTime && noteEnd >= accTime;
			if (isNoteActive)
			{
				keyMetas[n.note].MarkPressed(true);
				keyMetas[n.note].color = colors.GetColor(note->track, note->channel);

				notesPassed++;
				polyphony++;

				double tempoFrameStep = 1.0 / 60.0;
				double maxAuraLen = 0.5;

				double noteStartSecs = isTimeBased ? noteStart : tempoMap->TicksToSecsFromMap(seq->resolution, note->tick);

				double factor = 0.0;
				double framesSinceStart = (playbackSeconds - noteStartSecs) / tempoFrameStep;
				double factor2 = std::pow(std::max(10.0 - framesSinceStart, 0.0), 2.0) / 600;

				factor = 0.5;

				float size = static_cast<float>(factor + factor2);

				if (auraData[id].size < size)
				{
					auraData[id].size = size;
					auraData[id].meta = keyMetas[id].GetMeta();
					auraData[id].active = true;
				}
			}
			

			renderNotes[noteID++] = RenderNote(
				keyPos[id],
				keyPos[id] + keyWidth[id],
				(float)((noteStart - accTime) * invViewRegion),
				(float)((noteEnd - accTime) * invViewRegion),
				colors.GetColor(note->track, note->channel) | (isBlack << 24) | (isNoteActive << 25)
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

	noteCounterInfo->notesPassed = static_cast<uint64_t>(notesPassed);
	noteCounterInfo->polyphony = static_cast<uint64_t>(polyphony);

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
		noteCounterInfo->notesPerSecond = static_cast<uint64_t>(notesPassed) - notesOneSecondAgo;
	}
	else
	{
		noteCounterInfo->notesPerSecond = 0;
	}

	lastTime = time;
}

void MIDIRendererMIDITrail::UploadNoteBuffer(size_t count)
{
	if (!notesIBO) return;

	notesIBO->Bind();

	glBufferSubData(GL_ARRAY_BUFFER,
		0,
		count * sizeof(RenderNote),
		renderNotes.data());

	glDrawElementsInstanced(
		GL_TRIANGLES,
		settings.notes3D ? 36 : 6,
		GL_UNSIGNED_INT,
		nullptr,
		count
	);
}

void MIDIRendererMIDITrail::RenderSeparatorLines()
{
	ShaderBind bind(*separatorProgram);
	VertexArrayBind vaoBind(*separatorVAO);
	BufferBind vboBind(*separatorVBO);

	separatorProgram->SetMat4("model", model);
	separatorProgram->SetMat4("view", view);
	separatorProgram->SetMat4("projection", projection);
	separatorProgram->SetFloat("lineTransparency", settings.lineTransparency);

	glDrawArrays(GL_LINES, 0, settings.notes3D ? 8 : 4);
}

void MIDIRendererMIDITrail::UpdateSeparatorLines(float start, float end)
{
	VertexArrayBind vaoBind(*separatorVAO);
	BufferBind vboBind(*separatorVBO);

	for (int i = 0; i < separatorVerts.size(); i += 2)
	{
		separatorVerts[i].z = end;
		separatorVerts[i + 1].z = -start;
	}

	separatorVBO->SetSubData(0, separatorVerts);
}

void MIDIRendererMIDITrail::RenderMeasureLines()
{
	if (!seq) return;
	auto* renderView = app->GetRenderView();
	if (!renderView) return;

	TempoMap* tempoMap = seq->GetTempoMap();
	if (!tempoMap) return;

	ShaderBind bind(*measureProgram);
	VertexArrayBind vaoBind(*measureVAO);
	BufferBind iboBind(*measureIBO);
	BufferBind vboBind(*measureVBO);
	BufferBind eboBind(*measureEBO);

	measureProgram->SetMat4("model", model);
	measureProgram->SetMat4("view", view);
	measureProgram->SetMat4("projection", projection);
	measureProgram->SetFloat("kbHeight", keyboardHeight);
	measureProgram->SetFloat("lineTransparency", settings.lineTransparency);

	const double playbackSeconds = app->GetTimer()->Elapsed();
	const long currentTicks = tempoMap->SecsToTicksFromMap(seq->resolution, playbackSeconds);

	const double currentTime = isTimeBased ? playbackSeconds : (double)currentTicks;
	const double viewRegion = isTimeBased
		? (double)renderView->viewTicks / 1000.0
		: (double)renderView->viewTicks;

	const double invViewRegion = 1.0 / viewRegion;
	const double renderEnd = currentTime + (viewRegion * settings.frontRenderCutoff);

	std::vector<TimeSignatureEvent>& timeSigEvents = seq->timeSignatures;
	if (timeSigEvents.empty()) return;

	long measureTime = 0;
	size_t measureID = 0;

	auto measurePos = [&](long ticks) -> double
		{
			return isTimeBased
				? tempoMap->TicksToSecsFromMap(seq->resolution, ticks)
				: (double)ticks;
		};

	for (size_t i = 0; i < timeSigEvents.size(); i++)
	{
		TimeSignatureEvent* timeSignature = &timeSigEvents[i];
		TimeSignatureEvent* futureTimeSignature =
			(i + 1 < timeSigEvents.size()) ? &timeSigEvents[i + 1] : nullptr;

		const long measureInc = static_cast<long>(seq->resolution * 4) / timeSignature->denominator;
		int beat = 0;

		if (futureTimeSignature)
		{
			while (measureTime < futureTimeSignature->tick)
			{
				const double pos = measurePos(measureTime);

				if (beat == 0 && pos > currentTime && pos < renderEnd)
				{
					renderMeasureLines[measureID++] =
						RenderMeasureLine((float)((pos - currentTime) * invViewRegion));

					if (measureID >= MEASURE_LINE_BUFFER_SIZE)
					{
						measureIBO->Bind();
						glBufferSubData(GL_ARRAY_BUFFER, 0,
							MEASURE_LINE_BUFFER_SIZE * sizeof(RenderMeasureLine),
							renderMeasureLines.data());
						glDrawElementsInstanced(GL_LINES, 2, GL_UNSIGNED_INT, nullptr, MEASURE_LINE_BUFFER_SIZE);
						measureID = 0;
					}
				}

				measureTime += measureInc;
				beat++;
				if (beat >= timeSignature->numerator) beat = 0;
			}
		}
		else
		{
			while (measurePos(measureTime) < renderEnd)
			{
				const double pos = measurePos(measureTime);

				if (beat == 0 && pos > currentTime)
				{
					renderMeasureLines[measureID++] =
						RenderMeasureLine((float)((pos - currentTime) * invViewRegion));

					if (measureID >= MEASURE_LINE_BUFFER_SIZE)
					{
						measureIBO->Bind();
						glBufferSubData(GL_ARRAY_BUFFER, 0,
							MEASURE_LINE_BUFFER_SIZE * sizeof(RenderMeasureLine),
							renderMeasureLines.data());
						glDrawElementsInstanced(GL_LINES, 2, GL_UNSIGNED_INT, nullptr, MEASURE_LINE_BUFFER_SIZE);
						measureID = 0;
					}
				}

				measureTime += measureInc;
				beat++;
				if (beat >= timeSignature->numerator) beat = 0;
			}
		}
	}

	if (measureID > 0)
	{
		measureIBO->Bind();
		glBufferSubData(GL_ARRAY_BUFFER, 0,
			measureID * sizeof(RenderMeasureLine),
			renderMeasureLines.data());
		glDrawElementsInstanced(GL_LINES, 2, GL_UNSIGNED_INT, nullptr, measureID);
	}
}

void MIDIRendererMIDITrail::RenderAuras()
{
	if (!auraProgram || !auraVAO) return;

	size_t activeAuras = 0;

	for (int i = 0; i < MIDI_KEYS; i++)
	{
		const auto& data = auraData[i];
		if (!data.active) continue;

		float centerPos = keyPos[i] + (keyWidth[i] * 0.5f);
		float visualSize = data.size * settings.auraSize * 1.5f;
		renderAuras[activeAuras++] = RenderAura(visualSize, centerPos, data.meta);
	}

	if (activeAuras == 0) return;

	ShaderBind bind(*auraProgram);
	VertexArrayBind vaoBind(*auraVAO);
	BufferBind vboBind(*auraVBO);
	BufferBind eboBind(*auraEBO);
	BufferBind iboBind(*auraIBO);

	TextureBind auraTex(*auraTexture, 0);

	auraProgram->SetMat4("model", model);
	auraProgram->SetMat4("view", view);
	auraProgram->SetMat4("projection", projection);
	auraProgram->SetFloat("kbHeight", keyboardHeight);

	glBufferSubData(
		GL_ARRAY_BUFFER,
		0,
		activeAuras * sizeof(RenderAura),
		renderAuras.data()
	);

	glDrawElementsInstanced(
		GL_TRIANGLES,
		6, 
		GL_UNSIGNED_INT,
		nullptr,
		activeAuras
	);
}

void MIDIRendererMIDITrail::UpdateAuras(double deltaTime)
{
	for (int i = 0; i < MIDI_KEYS; i++)
	{
		AuraData& data = auraData[i];
		data.size = 0.0f;
		data.active = false;
	}
}

void MIDIRendererMIDITrail::LoadAuraTexture()
{
	std::filesystem::path& path = settings.auraTexture;
	std::shared_ptr<std::ifstream> stream = std::make_shared<std::ifstream>(path, std::ios::binary);

	auraTexture = std::make_unique<GPUImage>(stream);
}

glm::mat4 MIDIRendererMIDITrail::GetViewMatrixFromEuler()
{
	glm::vec3 cameraRotation = settings.cameraRotation;
	glm::vec3 cameraPos = settings.cameraPos;

	glm::mat4 view = glm::mat4(1.0f);
	view = glm::rotate(view, glm::radians(cameraRotation.x), glm::vec3(1.0f, 0.0f, 0.0f)); // pitch
	view = glm::rotate(view, glm::radians(cameraRotation.y), glm::vec3(0.0f, 1.0f, 0.0f)); // yaw
	view = glm::rotate(view, glm::radians(cameraRotation.z), glm::vec3(0.0f, 0.0f, 1.0f)); // roll
	view = glm::translate(view, -cameraPos);
	return view;
}

void MIDIRendererMIDITrail::UpdateMatrices()
{
	float cameraFOV = settings.cameraFOV;

	projection = glm::perspective(glm::radians(cameraFOV), (float)width / (float)height, 0.1f, 100.0f);
	view = GetViewMatrixFromEuler();
	model = glm::mat4(1.0f);
}

void MIDIRendererMIDITrail::ResetRenderer()
{
	for (auto& key : keyboardData)
	{
		key.pressFactor = 0;
	}

	for (auto& key : keyMetas)
	{
		key.MarkPressed(false);
	}
}