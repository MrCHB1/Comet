#pragma once

// REFACTOR (compile-memory): this header is the single shared precompiled
// header for the whole `Comet` target (~170 translation units), so every include
// added here is parsed by every compiler process. It was trimmed to the
// facilities that nearly all translation units actually use.
//
// Deliberately kept:
//   - <algorithm>/<array>/<atomic>/<chrono>/<cstdint>/<cstring>/<memory>/<mutex>/
//     <optional>/<string>/<type_traits>/<vector>: used pervasively.
//   - <filesystem>, <fstream>, <iostream>, <sstream>: still widely used (85
//     std::filesystem hits alone), so removing them would force explicit
//     includes across ~25+ files for little front-end gain.
//   - "imgui.h": verified pervasive (App/, Render/, MIDI/Audio/, Config/ and
//     ResourcePack/ all use ImGui or ImVec4), so a separate GUI-only PCH was
//     evaluated and rejected: it would have covered almost no translation unit.
//
// Removed (each consumer now includes the header it needs explicitly):
//   - <variant>                          -> Utils.h (declares std::variant)
//   - "stb_image.h"                      -> stb_image.cpp, App/MainWindow.cpp,
//                                           Render/ColorAsset.cpp,
//                                           Render/GPUImage.cpp,
//                                           Render/RenderEngine/TextureArray.cpp,
//                                           ColorPalette/ColorPaletteList.cpp
//   - "stb_image_write.h"                -> stb_image_write.cpp,
//                                           Render/GPUImage.cpp,
//                                           ColorPalette/ColorPaletteList.cpp
// stb_image.h alone is several thousand lines of macros and tables; pulling it
// out of the PCH removes it from every translation unit that never decodes an
// image.

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "imgui.h"
