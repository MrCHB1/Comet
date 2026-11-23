cmake_minimum_required(VERSION 3.21)

cmake_policy(SET CMP0091 NEW)

if(NOT CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VCPKG_ROOT})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    elseif(EXISTS "${CMAKE_CURRENT_LIST_DIR}/external/vcpkg/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/external/vcpkg/scripts/buildsystems/vcpkg.cmake")
    else()
        message(FATAL_ERROR "vcpkg not found. Run: git submodule update --init external/vcpkg")
    endif()
endif()

project(Comet LANGUAGES C CXX)

# Match the default vcpkg x64-windows triplet: /MD in Release, /MDd in Debug.
set(CMAKE_MSVC_RUNTIME_LIBRARY
    "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# REFACTOR (compile-memory): bound the number of concurrently running compiler
# processes. Under Ninja every translation unit forks its own cl.exe, so a
# Release build of ~170 heavy ImGui/glm TUs can exhaust system RAM even on a
# large machine. The pool is sized to half the logical core count; override with
# -DCOMET_MAX_COMPILE_JOBS=<n> (0 means "auto").
# STRING (not BOOL) so the value stays numeric; a BOOL option would coerce 0 to
# OFF and make -DCOMET_MAX_COMPILE_JOBS=<n> comparisons ambiguous.
set(COMET_MAX_COMPILE_JOBS "0" CACHE STRING "Maximum concurrent compiler processes (0 = auto)")

if(CMAKE_GENERATOR MATCHES "Ninja")
    if(COMET_MAX_COMPILE_JOBS GREATER 0)
        set(_comet_compile_jobs ${COMET_MAX_COMPILE_JOBS})
    else()
        include(ProcessorCount)
        ProcessorCount(_comet_cores)
        if(_comet_cores EQUAL 0)
            set(_comet_cores 4)
        endif()
        math(EXPR _comet_compile_jobs "${_comet_cores} / 2")
    endif()
    if(_comet_compile_jobs LESS 1)
        set(_comet_compile_jobs 1)
    endif()

    # link=1 keeps at most one link.exe running: the linker is the other peak.
    set(CMAKE_JOB_POOLS "compile=${_comet_compile_jobs};link=1")
    set(CMAKE_JOB_POOL_COMPILE compile)
    set(CMAKE_JOB_POOL_LINK link)

    message(STATUS "Comet: Ninja compile job pool = ${_comet_compile_jobs}")
endif()

# glad v0.1.36 declares cmake_minimum_required < 3.5, which CMake 4 rejects.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
set(FETCHCONTENT_QUIET OFF)

find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(miniz CONFIG REQUIRED)

# RtMidi's vcpkg port installs headers and a library. Create a stable imported
# target instead of relying on a port-specific CMake target name.
find_path(RTMIDI_INCLUDE_DIR NAMES RtMidi.h PATH_SUFFIXES rtmidi REQUIRED)
find_library(RTMIDI_LIBRARY NAMES rtmidi RtMidi REQUIRED)
add_library(RtMidi::RtMidi UNKNOWN IMPORTED)
set_target_properties(RtMidi::RtMidi PROPERTIES
    IMPORTED_LOCATION "${RTMIDI_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${RTMIDI_INCLUDE_DIR}"
)

# The vcpkg ImGui port installs the core and selected backend headers.
# Keep the flat backend include style used by the existing source files.
find_path(IMGUI_BACKENDS_INCLUDE_DIR
    NAMES imgui_impl_glfw.h
    PATH_SUFFIXES backends
    REQUIRED
)

# These dependencies are intentionally still built from source because this
# project uses the existing versions / source layout directly.
FetchContent_Declare(
    hwy
    GIT_REPOSITORY https://github.com/google/highway.git
    GIT_TAG 1.4.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(hwy)

FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v0.1.36
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glad)

# Native File Dialog's legacy repository is built as a small local target.
FetchContent_Declare(
    nfd
    GIT_REPOSITORY https://github.com/mlabbe/nativefiledialog.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nfd)

add_library(nfd STATIC
    ${nfd_SOURCE_DIR}/src/nfd_common.c
)
if(WIN32)
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

FetchContent_Declare(
    imgui_knobs
    GIT_REPOSITORY https://github.com/altschuler/imgui-knobs.git
    GIT_TAG main
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui_knobs)

add_executable(Comet
    # --- Root / entry point -------------------------------------------------
    "Comet.cpp"
    Inter.hpp
    FiraCode.hpp
    icon32.hpp
    RLEvec.h
    Utils.cpp
    IntervalThread.cpp
    UpdateChecker.cpp

    # --- Vendored single-header libraries (repo root) ------------------------
    stb_image.cpp
    stb_image_write.cpp
    stb_image_write.h
    tiny_obj_loader.h

    # --- App ----------------------------------------------------------------
    App/MainWindow.cpp
    App/MIDIApp.cpp
    App/Fonts.cpp
    App/FontList.cpp
    App/Models.cpp
    App/Diagnosis/DiagnosisDialog.cpp
    App/Diagnosis/DiagnosisFieldView.cpp
    App/Diagnosis/DiagnosisPanel.cpp
    App/Dialog/DialogMacros.h
    App/Dialog/LoadingDialog.cpp
    App/Dialog/MessageDialog.cpp
    App/Dialog/RenderVideoDialog.cpp
    App/Dialog/SettingsDialog.cpp
    App/UI/Dialog.h
    App/UI/NavigationBar.cpp
    App/UI/Themes/Themes.cpp
    App/UI/Widgets/ThinSlider.cpp
    App/UI/Widgets/TimeRange.cpp
    App/VideoRender/RenderSettings.h

    # --- MIDI ----------------------------------------------------------------
    MIDI/AbstractMIDILoader.h
    MIDI/MIDIDefs.h
    MIDI/MIDILoader.cpp
    MIDI/MIDISequence.cpp
    MIDI/MultithreadedMIDILoader.cpp
    MIDI/ProgressInputStream.cpp
    MIDI/SavedInputStream.cpp
    MIDI/TempoMap.cpp
    MIDI/Events/MIDIMessageEvent.h
    MIDI/Events/NoteEvent.h
    MIDI/Events/TempoEvent.h
    MIDI/Events/TimeSignatureEvent.h
    MIDI/Events/TrackEvent.h
    MIDI/Sequence/SequenceFuncs.cpp
    MIDI/Sequence/SequenceOps.cpp
    MIDI/Timer/MIDITimer.cpp
    MIDI/Diagnosis/ADiagnosis.cpp
    MIDI/Diagnosis/BasicDiagnosis.cpp
    MIDI/Diagnosis/BlackMIDIDiagnosis.cpp
    MIDI/Diagnosis/Diagnoses.cpp
    MIDI/Diagnosis/DiagnosisException.h
    MIDI/Diagnosis/DiagnosisField.cpp
    MIDI/Diagnosis/DiagnosisMessage.h
    MIDI/Audio/AudioThread.cpp
    MIDI/Audio/MaestroEngine.cpp
    MIDI/Audio/MIDIAudio.cpp
    MIDI/Audio/MIDIOut.cpp
    MIDI/Audio/SoundfontList.cpp
    MIDI/Audio/PrerenderEngine/BASSMIDI.cpp
    MIDI/Audio/PrerenderEngine/PrerenderedEngine.cpp

    # --- IO ------------------------------------------------------------------
    IO/BufferedByteReader.cpp
    IO/InputStream.cpp

    # --- Config --------------------------------------------------------------
    Config/ConfigSection.cpp
    Config/MIDIPlayerConfig.cpp

    # --- ResourcePack --------------------------------------------------------
    ResourcePack/DefaultResourcePack.cpp
    ResourcePack/FolderResourcePack.cpp
    ResourcePack/ResourcePack.cpp
    ResourcePack/ResourcePackInfo.h
    ResourcePack/ResourcePackList.cpp
    ResourcePack/ResourcePackVerifier.h
    ResourcePack/SignatureState.h
    ResourcePack/ZipResourcePack.cpp

    # --- Render (renderers) --------------------------------------------------
    Render/RenderUtils.h
    Render/RenderView.h
    Render/RenderNote.h
    Render/BlurredQuadRenderer.h
    Render/ColorAsset.cpp
    Render/GPUImage.cpp
    Render/MIDIRenderer.cpp
    Render/MIDIRendererChannels.cpp
    Render/MIDIRendererCounter.cpp
    Render/MIDIRendererEnhanced.cpp
    Render/MIDIRendererMIDITrail.cpp
    Render/MIDIRendererPFA.cpp
    Render/MIDIRendererSynthesia.cpp
    Render/MIDIRendererVelocities.cpp

    # --- Render/RenderEngine -------------------------------------------------
    Render/RenderEngine/Buffers.cpp
    Render/RenderEngine/OBJMesh.cpp
    Render/RenderEngine/Shaders.cpp
    Render/RenderEngine/TextureArray.cpp

    # --- Render/Renderer -----------------------------------------------------
    Render/Renderer/AbstractMIDIRenderer.cpp
    Render/Renderer/KeyNoteIndex.h
    Render/Renderer/KeyNoteIndexData.h
    Render/Renderer/NoteCuller.cpp
    Render/Renderer/PrimitiveDrawer.cpp
    Render/Renderer/PrimitiveShaders.h
    Render/Renderer/QuadDrawer.h

    # --- Render/NoteCounter --------------------------------------------------
    Render/NoteCounter/AbstractOverlayRenderer.cpp
    Render/NoteCounter/NoteCounterInfo.cpp
    Render/NoteCounter/NoteCounterRenderer.cpp
    Render/NoteCounter/NoteCounterStyles.h
    Render/NoteCounter/StatsOverlayRenderer.cpp

    # --- FFmpeg --------------------------------------------------------------
    FFmpeg/FFmpegCommandBuilder.cpp
    FFmpeg/FFmpegGPU.cpp
    FFmpeg/FFmpegPipe.h

    # --- Concurrency ---------------------------------------------------------
    Concurrency/ThreadPool.cpp

    # --- ColorPalette --------------------------------------------------------
    ColorPalette/ColorPaletteList.cpp
  "App/UI/Layout/ForumLayout.cpp" "U32Vec.h")

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
    ${IMGUI_BACKENDS_INCLUDE_DIR}
)

target_link_libraries(Comet PRIVATE
    hwy
    imgui::imgui
    nfd
    glad
    glfw
    OpenGL::GL
    yaml-cpp::yaml-cpp
    glm::glm
    miniz::miniz
    RtMidi::RtMidi
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

    # MSVC pulls these in via #pragma comment(lib); MinGW needs them explicitly.
    if(MINGW)
        target_link_libraries(Comet PRIVATE winmm avrt)
    endif()

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

# REFACTOR (compile-memory): whole-program optimization was removed from the
# Release configuration. INTERPROCEDURAL_OPTIMIZATION_RELEASE injected /GL into
# every translation unit (the compiler keeps the cross-TU call graph in memory)
# and /LTCG into the link step (whole-program analysis), which together were the
# dominant cause of the machine running out of memory. Codegen now comes from
# the plain /O2 /Ob2 in CMAKE_CXX_FLAGS_RELEASE; /Ob3 was dropped because
# aggressive inlining multiplies the optimizer's in-memory IR for negligible
# runtime gain. The duplicate per-target /Zi and the unconditional /DEBUG link
# flag were replaced by /DEBUG:FASTLINK, which still produces a usable PDB but
# avoids the full debug-info merge that spiked linker memory.
if(MSVC)
    target_compile_options(Comet PRIVATE
        $<$<CONFIG:Release>:/bigobj>          # headroom for large generated/TMP-heavy TUs
        $<$<CONFIG:Release>:/permissive->     # cheaper, more constrained overload resolution
        $<$<CONFIG:Release>:/Zc:__cplusplus>  # correct __cplusplus for third-party headers
        $<$<CONFIG:Release>:/external:W0>     # do not analyse third-party header warnings
    )
    target_link_options(Comet PRIVATE
        $<$<CONFIG:Release>:/DEBUG:FASTLINK>
    )
    # TEMPORARY
    add_compile_options(/showIncludes)
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