find_package(Threads REQUIRED)
# miniaudio's implementation: third-party code, so not held to the strict
# warning level (MSVC /W4 warns in it).
add_library(iwd2r_miniaudio STATIC miniaudio_impl.cpp)
target_link_libraries(iwd2r_miniaudio PUBLIC Threads::Threads miniaudio ${CMAKE_DL_LIBS})
# The presenter's SPIR-V blobs (platform/shaders/*.spv, made by
# tools/shaders.py and committed) as byte arrays, so builds never need glslang.
set(IWD2R_SHADERS quad.vert nearest.frag sharp.frag smooth.frag fsr_easu.frag fsr_rcas.frag nis.frag overlay.frag)
set(shader_table "")
set(shader_arrays "")
foreach(name IN LISTS IWD2R_SHADERS)
    set(spv "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${name}.spv")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${spv}")
    file(READ "${spv}" hex HEX)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
    string(MAKE_C_IDENTIFIER "${name}" id)
    string(APPEND shader_arrays "const unsigned char k_${id}[] = { ${bytes} };\n")
    string(APPEND shader_table "    { \"${name}\", k_${id}, sizeof(k_${id}) },\n")
endforeach()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/present_shaders.cpp.in"
    "// Generated from platform/shaders/*.spv by platform/CMakeLists.txt.\n"
    "#include \"platform/present_backend.h\"\n\n#include <cstring>\n\n"
    "namespace platform::present {\n\nnamespace {\n\n${shader_arrays}\n"
    "struct Entry {\n    const char* name;\n    const unsigned char* code;\n    size_t size;\n};\n\n"
    "const Entry kShaders[] = {\n${shader_table}};\n\n}\n\n"
    "bool ShaderCode(const char* name, const unsigned char** code, size_t* size)\n{\n"
    "    for (const Entry& e : kShaders) {\n        if (std::strcmp(e.name, name) == 0) {\n"
    "            *code = e.code;\n            *size = e.size;\n            return true;\n        }\n    }\n"
    "    return false;\n}\n\n}\n")
configure_file("${CMAKE_CURRENT_BINARY_DIR}/present_shaders.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/present_shaders.cpp" COPYONLY)
# The scalable text's TrueType faces (third_party/fonts, SIL OFL; see
# docs/THIRD_PARTY.md) as byte arrays, so the game never looks for font files
# at run time. The order is platform/text_fonts.cpp's FaceId.
set(IWD2R_TEXT_FACES CrimsonPro-Bold.ttf Marcellus-Regular.ttf MarcellusSC-Regular.ttf UncialAntiqua-Regular.ttf)
set(face_arrays "")
set(face_table "")
set(face_count 0)
foreach(name IN LISTS IWD2R_TEXT_FACES)
    set(ttf "${PROJECT_SOURCE_DIR}/third_party/fonts/${name}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${ttf}")
    file(READ "${ttf}" hex HEX)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
    string(REGEX REPLACE "((0x[0-9a-f][0-9a-f],)(0x[0-9a-f][0-9a-f],)(0x[0-9a-f][0-9a-f],)(0x[0-9a-f][0-9a-f],)(0x[0-9a-f][0-9a-f],)(0x[0-9a-f][0-9a-f],)(0x[0-9a-f][0-9a-f],)(0x[0-9a-f][0-9a-f],))" "\\1\n" bytes "${bytes}")
    string(APPEND face_arrays "const unsigned char k_face${face_count}[] = {\n${bytes}};\n")
    string(APPEND face_table "    { \"${name}\", k_face${face_count}, sizeof(k_face${face_count}) },\n")
    math(EXPR face_count "${face_count} + 1")
endforeach()
if(face_count EQUAL 0)
    set(face_table "    { \"\", nullptr, 0 },\n")
endif()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/text_faces.cpp.in"
    "// Generated from third_party/fonts by platform/CMakeLists.txt.\n"
    "#include \"platform/text_fonts.h\"\n\n"
    "namespace platform::text {\n\nnamespace {\n\n${face_arrays}\n"
    "const Face kFaces[] = {\n${face_table}};\n\n}\n\n"
    "const Face* EmbeddedFaces(int* count)\n{\n    *count = ${face_count};\n    return kFaces;\n}\n\n}\n")
configure_file("${CMAKE_CURRENT_BINARY_DIR}/text_faces.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/text_faces.cpp" COPYONLY)
add_library(iwd2r_platform STATIC platform_version.cpp threads.cpp clock.cpp fs.cpp data_dir.cpp test_knobs.cpp ddraw_shim.cpp dsound_shim.cpp compose.cpp gpu_probe.cpp present.cpp present_classic.cpp present_gpu.cpp present_nis.cpp "${CMAKE_CURRENT_BINARY_DIR}/present_shaders.cpp" text_layer.cpp text_fonts.cpp text_atlas.cpp text_overlay.cpp "${CMAKE_CURRENT_BINARY_DIR}/text_faces.cpp" window.cpp window_keys.cpp win32_stubs.cpp gl_compat.cpp affinity.cpp png.cpp script_core.cpp script.cpp state_dump.cpp replay.cpp crash.cpp)
target_include_directories(iwd2r_platform PUBLIC "${PROJECT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(iwd2r_platform PUBLIC Threads::Threads iwd2r_compat SDL3::SDL3-static miniaudio iwd2r_miniaudio ${ZLIB_LIBRARIES})
# The screenshot writer (png.cpp) uses the vendored zlib.
target_include_directories(iwd2r_platform SYSTEM PRIVATE ${ZLIB_INCLUDE_DIRS})
iwd2r_strict_warnings(iwd2r_platform)

set_property(SOURCE win32_stubs.cpp APPEND PROPERTY COMPILE_DEFINITIONS
    "IWD2R_VERSION_STRING=\"${IWD2R_VERSION_STRING}\"")
