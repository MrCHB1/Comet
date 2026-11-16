cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0091 NEW)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

project(Comet LANGUAGES C CXX)

set(CMAKE_MSVC_RUNTIME_LIBRARY
    "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP2")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG")
endif()

set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
find_package(OpenGL REQUIRED)
set(FETCHCONTENT_QUIET OFF)

set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
if(WIN32)
    set(CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)
else()
    set(CURL_USE_OPENSSL ON CACHE BOOL "" FORCE)
endif()

include(FetchContent)

FetchContent_Declare(
    hwy
    GIT_REPOSITORY https://github.com/google/highway.git
    GIT_TAG 1.4.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(hwy)

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

FetchContent_Declare(
    imgui_knobs
    GIT_REPOSITORY https://github.com/altschuler/imgui-knobs.git
    GIT_TAG main
)

FetchContent_MakeAvailable(imgui_knobs)

set(_prev_shared ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    curl
    GIT_REPOSITORY https://github.com/curl/curl.git
    GIT_TAG curl-8_16_0
)

FetchContent_MakeAvailable(curl)
set(BUILD_SHARED_LIBS ${_prev_shared} CACHE BOOL "" FORCE)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
)

FetchContent_MakeAvailable(json)

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

target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

add_library(nfd STATIC
    ${nfd_SOURCE_DIR}/src/nfd_common.c)

if (WIN32)
    target_sources(nfd PRIVATE ${nfd_SOURCE_DIR}/src/nfd_win.cpp)
elseif(APPLE)
    target_sources(nfd PRIVATE ${nfd_SOURCE_DIR}/src/nfd_cocoa.m)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    target_sources(nfd PRIVATE ${nfd_SOURCE_DIR}/src/nfd_gtk.c)
    target_include_directories(nfd PRIVATE ${GTK3_INCLUDE_DIRS})
    target_link_libraries(nfd PRIVATE ${GTK3_LIBRARIES})
endif()

target_include_directories(nfd PUBLIC
    ${nfd_SOURCE_DIR}/src/include
)

add_executable(Comet
    # --- Root / entry point -------------------------------------------------
    "Comet.cpp"
    Inter.hpp
    FiraCode.hpp
    icon32.hpp
    RLEvec.h
    Utils.cpp
    IntervalThread.h
    IntervalThread.cpp
    UpdateChecker.h
    UpdateChecker.cpp

    # --- Vendored single-header libraries (repo root) ------------------------
    stb_image.cpp
    stb_image_write.cpp
    stb_image_write.h
    tiny_obj_loader.h

    # --- App ----------------------------------------------------------------
    App/MainWindow.cpp
    App/MIDIApp.cpp
    App/Fonts.h
    App/Fonts.cpp
    App/FontList.h
    App/FontList.cpp
    App/Models.h
    App/Models.cpp
    App/Diagnosis/DiagnosisDialog.h
    App/Diagnosis/DiagnosisDialog.cpp
    App/Diagnosis/DiagnosisFieldView.h
    App/Diagnosis/DiagnosisFieldView.cpp
    App/Diagnosis/DiagnosisPanel.h
    App/Diagnosis/DiagnosisPanel.cpp
    App/Dialog/DialogMacros.h
    App/Dialog/LoadingDialog.h
    App/Dialog/LoadingDialog.cpp
    App/Dialog/MessageDialog.h
    App/Dialog/MessageDialog.cpp
    App/Dialog/RenderVideoDialog.h
    App/Dialog/RenderVideoDialog.cpp
    App/Dialog/SettingsDialog.h
    App/Dialog/SettingsDialog.cpp
    App/UI/Dialog.h
    App/UI/NavigationBar.h
    App/UI/NavigationBar.cpp
    App/UI/Themes/Themes.h
    App/UI/Themes/Themes.cpp
    App/UI/Widgets/ThinSlider.h
    App/UI/Widgets/ThinSlider.cpp
    App/UI/Widgets/TimeRange.h
    App/UI/Widgets/TimeRange.cpp
    App/VideoRender/RenderSettings.h

    # --- MIDI ----------------------------------------------------------------
    MIDI/AbstractMIDILoader.h
    MIDI/MIDIDefs.h
    MIDI/MIDILoader.h
    MIDI/MIDILoader.cpp
    MIDI/MIDISequence.h
    MIDI/MIDISequence.cpp
    MIDI/MultithreadedMIDILoader.h
    MIDI/MultithreadedMIDILoader.cpp
    MIDI/ProgressInputStream.cpp
    MIDI/SavedInputStream.cpp
    MIDI/TempoMap.h
    MIDI/TempoMap.cpp
    MIDI/Events/MIDIMessageEvent.h
    MIDI/Events/NoteEvent.h
    MIDI/Events/TempoEvent.h
    MIDI/Events/TimeSignatureEvent.h
    MIDI/Events/TrackEvent.h
    MIDI/Sequence/SequenceFuncs.h
    MIDI/Sequence/SequenceFuncs.cpp
    MIDI/Timer/MIDITimer.h
    MIDI/Timer/MIDITimer.cpp
    MIDI/Diagnosis/ADiagnosis.h
    MIDI/Diagnosis/ADiagnosis.cpp
    MIDI/Diagnosis/BasicDiagnosis.cpp
    MIDI/Diagnosis/BlackMIDIDiagnosis.h
    MIDI/Diagnosis/BlackMIDIDiagnosis.cpp
    MIDI/Diagnosis/Diagnoses.h
    MIDI/Diagnosis/Diagnoses.cpp
    MIDI/Diagnosis/DiagnosisException.h
    MIDI/Diagnosis/DiagnosisField.cpp
    MIDI/Diagnosis/DiagnosisMessage.h
    MIDI/Audio/AudioThread.h
    MIDI/Audio/AudioThread.cpp
    MIDI/Audio/MaestroEngine.cpp
    MIDI/Audio/MIDIAudio.h
    MIDI/Audio/MIDIAudio.cpp
    MIDI/Audio/MIDIOut.h
    MIDI/Audio/MIDIOut.cpp
    MIDI/Audio/SoundfontList.cpp
    MIDI/Audio/PrerenderEngine/BASSMIDI.h
    MIDI/Audio/PrerenderEngine/BASSMIDI.cpp
    MIDI/Audio/PrerenderEngine/PrerenderedEngine.h
    MIDI/Audio/PrerenderEngine/PrerenderedEngine.cpp

    # --- IO ------------------------------------------------------------------
    IO/BufferedByteReader.h
    IO/BufferedByteReader.cpp
    IO/InputStream.h
    IO/InputStream.cpp

    # --- Config --------------------------------------------------------------
    Config/ConfigSection.h
    Config/ConfigSection.cpp
    Config/MIDIPlayerConfig.h
    Config/MIDIPlayerConfig.cpp

    # --- ResourcePack --------------------------------------------------------
    ResourcePack/DefaultResourcePack.h
    ResourcePack/DefaultResourcePack.cpp
    ResourcePack/FolderResourcePack.h
    ResourcePack/FolderResourcePack.cpp
    ResourcePack/ResourcePack.h
    ResourcePack/ResourcePack.cpp
    ResourcePack/ResourcePackInfo.h
    ResourcePack/ResourcePackList.h
    ResourcePack/ResourcePackList.cpp
    ResourcePack/ResourcePackVerifier.h
    ResourcePack/SignatureState.h
    ResourcePack/ZipResourcePack.h
    ResourcePack/ZipResourcePack.cpp

    # --- Render (renderers) --------------------------------------------------
    Render/RenderUtils.h
    Render/RenderView.h
    Render/BlurredQuadRenderer.h
    Render/ColorAsset.h
    Render/ColorAsset.cpp
    Render/GPUImage.h
    Render/GPUImage.cpp
    Render/MIDIRenderer.h
    Render/MIDIRenderer.cpp
    Render/MIDIRendererChannels.h
    Render/MIDIRendererChannels.cpp
    Render/MIDIRendererCounter.cpp
    Render/MIDIRendererEnhanced.h
    Render/MIDIRendererEnhanced.cpp
    Render/MIDIRendererEnhancedShaders.h
    Render/MIDIRendererMIDITrail.h
    Render/MIDIRendererMIDITrail.cpp
    Render/MIDIRendererPFA.h
    Render/MIDIRendererPFA.cpp
    Render/MIDIRendererSynthesia.h
    Render/MIDIRendererSynthesia.cpp
    Render/MIDIRendererVelocities.h
    Render/MIDIRendererVelocities.cpp

    # --- Render/RenderEngine -------------------------------------------------
    Render/RenderEngine/Buffers.h
    Render/RenderEngine/Buffers.cpp
    Render/RenderEngine/OBJMesh.h
    Render/RenderEngine/OBJMesh.cpp
    Render/RenderEngine/Shaders.h
    Render/RenderEngine/Shaders.cpp
    Render/RenderEngine/TextureArray.h
    Render/RenderEngine/TextureArray.cpp

    # --- Render/Renderer -----------------------------------------------------
    Render/Renderer/AbstractMIDIRenderer.cpp
    Render/Renderer/KeyNoteIndex.h
    Render/Renderer/KeyNoteIndexData.h
    Render/Renderer/NoteCuller.h
    Render/Renderer/NoteCuller.cpp
    Render/Renderer/PrimitiveDrawer.h
    Render/Renderer/PrimitiveDrawer.cpp
    Render/Renderer/PrimitiveShaders.h
    Render/Renderer/QuadDrawer.h

    # --- Render/NoteCounter --------------------------------------------------
    Render/NoteCounter/AbstractOverlayRenderer.h
    Render/NoteCounter/AbstractOverlayRenderer.cpp
    Render/NoteCounter/NoteCounterInfo.h
    Render/NoteCounter/NoteCounterInfo.cpp
    Render/NoteCounter/NoteCounterRenderer.h
    Render/NoteCounter/NoteCounterRenderer.cpp
    Render/NoteCounter/NoteCounterStyles.h
    Render/NoteCounter/StatsOverlayRenderer.h
    Render/NoteCounter/StatsOverlayRenderer.cpp

    # --- FFmpeg --------------------------------------------------------------
    FFmpeg/FFmpegCommandBuilder.h
    FFmpeg/FFmpegCommandBuilder.cpp
    FFmpeg/FFmpegGPU.h
    FFmpeg/FFmpegGPU.cpp
    FFmpeg/FFmpegPipe.h

    # --- Concurrency ---------------------------------------------------------
    Concurrency/ThreadPool.h
    Concurrency/ThreadPool.cpp

    # --- ColorPalette --------------------------------------------------------
    ColorPalette/ColorPaletteList.h
    ColorPalette/ColorPaletteList.cpp
)

target_precompile_headers(Comet PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/pch.h"
)

set_source_files_properties(
    ResourcePack/FolderResourcePack.cpp
    PROPERTIES
        SKIP_PRECOMPILE_HEADERS ON
)

 target_include_directories(Comet PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

 target_sources(Comet PRIVATE
    ${imgui_knobs_SOURCE_DIR}/imgui-knobs.cpp
)

  target_include_directories(Comet PRIVATE
    ${imgui_knobs_SOURCE_DIR}
)

target_link_libraries(Comet PRIVATE
    hwy
    imgui
    nfd
    glad
    glfw
    OpenGL::GL
    yaml-cpp
    glm::glm
    miniz
    rtmidi
    CURL::libcurl
    nlohmann_json::nlohmann_json
)

if(WIN32)
    target_include_directories(Comet PRIVATE
        ${CMAKE_SOURCE_DIR}/external/bass
        ${CMAKE_SOURCE_DIR}/external/bassmidi
    )

    # Link directly to the import .lib files via absolute paths
    target_link_libraries(Comet PRIVATE
        "${CMAKE_SOURCE_DIR}/external/bass/bass.lib"
        "${CMAKE_SOURCE_DIR}/external/bassmidi/bassmidi.lib"
    )

    add_custom_command(TARGET Comet POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/external/bass/bass.dll"
            $<TARGET_FILE_DIR:Comet>

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/external/bassmidi/bassmidi.dll"
            $<TARGET_FILE_DIR:Comet>
    )
endif()

# <Windows.h> defines min/max as function-like macros, which mangle any
# std::min()/std::max() call in a TU that (transitively) includes it.
# NOMINMAX suppresses those macros; nothing in Comet relies on them now.
if(WIN32)
    target_compile_definitions(Comet PRIVATE NOMINMAX)
endif()

# libstdc++ parallel algorithms (std::execution) require linking Intel TBB.
# ${CMAKE_DL_LIBS} provides dlopen/dlsym used to load OmniMIDI at runtime.
if(NOT WIN32)
    find_package(TBB QUIET)
    if(TBB_FOUND)
        target_link_libraries(Comet PRIVATE TBB::tbb)
    else()
        target_link_libraries(Comet PRIVATE tbb)
    endif()
    target_link_libraries(Comet PRIVATE ${CMAKE_DL_LIBS})
endif()

if(MSVC)
    target_compile_options(Comet PRIVATE
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Release>:/GL>
        $<$<CONFIG:Release>:/Ot>
        $<$<CONFIG:Release>:/Ob3>
    )

    # target_link_options(Comet PRIVATE
    #     $<$<CONFIG:Release>:/LTCG>
    # )
else()
    target_compile_options(Comet PRIVATE
        $<$<CONFIG:Release>:-O3>
    )
endif()

if (WIN32)
    target_sources(Comet PRIVATE Comet.rc)
endif()

add_custom_command(
    TARGET Comet POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:Comet>/assets
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET Comet PROPERTY CXX_STANDARD 20)
endif()
