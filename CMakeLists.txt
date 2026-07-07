cmake_minimum_required(VERSION 3.8)

project(Comet LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
set(FETCHCONTENT_QUIET OFF)

include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.9
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
    nfd
    GIT_REPOSITORY https://github.com/mlabbe/nativefiledialog.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(nfd)

FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(yaml-cpp)

FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v0.1.36
)

FetchContent_MakeAvailable(glad)

FetchContent_Declare(
	glm
	GIT_REPOSITORY	https://github.com/g-truc/glm.git
	GIT_TAG 1.0.3
)

FetchContent_MakeAvailable(glm)

FetchContent_Declare(
    miniz
    GIT_REPOSITORY https://github.com/richgel999/miniz.git
    GIT_TAG master
)

FetchContent_MakeAvailable(miniz)

FetchContent_Declare(
    rtmidi
    GIT_REPOSITORY https://github.com/thestk/rtmidi.git
    GIT_TAG master
)

FetchContent_MakeAvailable(rtmidi)

file(GLOB IMGUI_SRC
    ${imgui_SOURCE_DIR}/*.cpp
)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui PUBLIC glfw opengl32)

add_library(nfd STATIC
    ${nfd_SOURCE_DIR}/src/nfd_common.c)

if (WIN32)
    target_sources(nfd PRIVATE ${nfd_SOURCE_DIR}/src/nfd_win.cpp)
endif()

target_include_directories(nfd PUBLIC
    ${nfd_SOURCE_DIR}/src/include
)

add_executable(Comet
    "Comet.cpp"
    App/MainWindow.cpp
    App/MIDIApp.cpp
    MIDI/SavedInputStream.cpp
    MIDI/ProgressInputStream.cpp
    Inter.hpp
    FiraCode.hpp
    MIDI/Diagnosis/ADiagnosis.h
    MIDI/Diagnosis/ADiagnosis.cpp
    MIDI/Diagnosis/BasicDiagnosis.cpp
    App/Diagnosis/DiagnosisDialog.cpp
    App/Diagnosis/DiagnosisDialog.h
    App/UI/Dialog.h
    stb_image_write.h
    tiny_obj_loader.h
    ColorPalette/ColorPaletteList.h
 "MIDI/Diagnosis/DiagnosisField.cpp" "Utils.cpp" "MIDI/Diagnosis/Diagnoses.h" "MIDI/Diagnosis/Diagnoses.cpp" "App/Diagnosis/DiagnosisPanel.h" "App/Diagnosis/DiagnosisPanel.cpp" "App/Diagnosis/DiagnosisFieldView.h" "App/Diagnosis/DiagnosisFieldView.cpp"  "IntervalThread.h" "IntervalThread.cpp" "MIDI/AbstractMIDILoader.h" "MIDI/MIDISequence.h" "MIDI/Events/MIDIMessageEvent.h" "MIDI/Events/TrackEvent.h" "MIDI/Events/TempoEvent.h" "MIDI/Events/NoteEvent.h" "MIDI/MIDISequence.cpp" "MIDI/Diagnosis/BlackMIDIDiagnosis.h" "MIDI/Diagnosis/BlackMIDIDiagnosis.cpp" "MIDI/Diagnosis/DiagnosisMessage.h" "MIDI/Diagnosis/DiagnosisException.h" "IO/BufferedByteReader.h" "IO/BufferedByteReader.cpp" "IO/InputStream.h" "IO/InputStream.cpp" "ResourcePack/ResourcePack.h" "Render/RenderUtils.h" "ResourcePack/ResourcePackInfo.h" "ResourcePack/SignatureState.h" "ResourcePack/ResourcePack.cpp" "ResourcePack/FolderResourcePack.h" "ResourcePack/FolderResourcePack.cpp" "Config/ConfigSection.h" "Config/ConfigSection.cpp" "ResourcePack/ResourcePackVerifier.h" "ResourcePack/DefaultResourcePack.h" "ResourcePack/DefaultResourcePack.cpp" "Config/MIDIPlayerConfig.h" "Render/ColorAsset.h" "Render/GPUImage.h" "Render/GPUImage.cpp" "Render/ColorAsset.cpp" "Render/MIDIRenderer.h" "Render/MIDIRenderer.cpp" "Render/RenderEngine/Shaders.h" "Render/RenderEngine/Shaders.cpp" "Render/RenderEngine/Buffers.h" "Render/RenderEngine/Buffers.cpp" "Render/Renderer/QuadDrawer.h" "Render/Renderer/PrimitiveDrawer.h" "Render/Renderer/PrimitiveDrawer.cpp" "MIDI/MIDILoader.h" "MIDI/MIDILoader.cpp" "App/Dialog/LoadingDialog.h" "App/Dialog/LoadingDialog.cpp" "MIDI/Sequence/SequenceFuncs.h" "MIDI/Sequence/SequenceFuncs.cpp" "Render/RenderView.h" "MIDI/MIDIDefs.h" "MIDI/Timer/MIDITimer.h" "MIDI/Timer/MIDITimer.cpp" "MIDI/TempoMap.h" "MIDI/TempoMap.cpp" "App/UI/NavigationBar.h" "App/UI/NavigationBar.cpp" "Render/NoteCounter/NoteCounterInfo.h" "Render/NoteCounter/NoteCounterRenderer.h" "Render/NoteCounter/NoteCounterStyles.h" "Render/NoteCounter/NoteCounterRenderer.cpp" "Render/Renderer/PrimitiveShaders.h" "Render/BlurredQuadRenderer.h" "App/Dialog/RenderVideoDialog.h" "App/Dialog/RenderVideoDialog.cpp" "App/VideoRender/RenderSettings.h" "FFmpeg/FFmpegPipe.h" "Render/NoteCounter/NoteCounterInfo.cpp" "App/Dialog/SettingsDialog.h" "App/Dialog/SettingsDialog.cpp" "ResourcePack/ZipResourcePack.h" "ResourcePack/ZipResourcePack.cpp" "ResourcePack/ResourcePackList.h" "ResourcePack/ResourcePackList.cpp" "FFmpeg/FFmpegGPU.h" "FFmpeg/FFmpegGPU.cpp" "FFmpeg/FFmpegCommandBuilder.h" "FFmpeg/FFmpegCommandBuilder.cpp" "App/Fonts.h" "App/Fonts.cpp" "ColorPalette/ColorPaletteList.cpp" "MIDI/Audio/MIDIOut.h" "MIDI/Audio/MIDIOut.cpp" "MIDI/Audio/AudioThread.h" "MIDI/Audio/AudioThread.cpp" "Config/MIDIPlayerConfig.cpp" "MIDI/MultithreadedMIDILoader.h" "MIDI/MultithreadedMIDILoader.cpp" "Render/MIDIRendererEnhanced.h" "Render/MIDIRendererEnhanced.cpp" "Render/Renderer/AbstractMIDIRenderer.cpp" "Render/RenderEngine/OBJMesh.h" "Render/RenderEngine/OBJMesh.cpp" "App/Models.h" "App/Models.cpp" "App/UI/Themes/Themes.h" "App/UI/Themes/Themes.cpp"  "Render/MIDIRendererMIDITrail.h" "Render/MIDIRendererMIDITrail.cpp" "MIDI/Events/TimeSignatureEvent.h")

 target_include_directories(Comet PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
 )

target_link_libraries(Comet PRIVATE
    imgui
    nfd
    glad
    glfw
    opengl32
    yaml-cpp
    glm::glm
    miniz
    rtmidi
)

target_compile_options(Comet PRIVATE 
    $<$<CONFIG:Release>:/O2>
    $<$<CONFIG:Release>:/GL>
    $<$<CONFIG:Release>:/Ot>
    $<$<CONFIG:Release>:/Ob3>
)

target_link_options(Comet PRIVATE 
    $<$<CONFIG:Release>:/LTCG>
)

add_custom_command(
    TARGET Comet POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:Comet>/assets
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET Comet PROPERTY CXX_STANDARD 20)
endif()