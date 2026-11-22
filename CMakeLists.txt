cmake_minimum_required(VERSION 3.20)
project(rtrt LANGUAGES C CXX)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "rtrt builds for 64-bit Windows only")
endif()

find_package(Vulkan REQUIRED)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(LIBS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs)
set(SCRIPTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/scripts)
set(IMGUI_DIR ${LIBS_DIR}/imgui-master)

file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS ${SRC_DIR}/*.cpp ${SRC_DIR}/*.h)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)

# Compiles one Slang file with compile_shaders.bat into the listed SPIR-V files.
set(SHADER_OUTPUTS)
function(add_shader shader)
    set(spv_names ${ARGN})
    list(TRANSFORM spv_names PREPEND ${SRC_DIR}/spv/)
    add_custom_command(
        OUTPUT ${spv_names}
        COMMAND ${SCRIPTS_DIR}/compile_shaders.bat ${shader}
        WORKING_DIRECTORY ${SRC_DIR}
        DEPENDS ${SRC_DIR}/shaders/${shader} ${SRC_DIR}/shaders/shared_structs.h
                ${SRC_DIR}/shaders/rng.slang ${SCRIPTS_DIR}/compile_shaders.bat
        COMMENT "Compiling Slang shader ${shader}"
    )
    set(SHADER_OUTPUTS ${SHADER_OUTPUTS} ${spv_names} PARENT_SCOPE)
endfunction()

add_shader(denoise.slang denoise.comp.slang.spv)
add_shader(post.slang post.vert.slang.spv post.frag.slang.spv)
add_shader(scanline.slang scanline.vert.slang.spv scanline.frag.slang.spv)
add_shader(raytrace.slang raytrace.rgen.slang.spv raytrace.rmiss.slang.spv
           raytraceShadow.rmiss.slang.spv raytrace.rchit.slang.spv)
add_custom_target(shaders DEPENDS ${SHADER_OUTPUTS} SOURCES ${SCRIPTS_DIR}/compile_shaders.bat)

add_executable(rtrt ${APP_SOURCES} ${IMGUI_SOURCES})
add_dependencies(rtrt shaders)

target_compile_features(rtrt PRIVATE cxx_std_17)
target_compile_definitions(rtrt PRIVATE WIN64 _CONSOLE GLM_FORCE_RADIANS)
target_compile_options(rtrt PRIVATE /W3 /sdl /permissive-)
target_include_directories(rtrt PRIVATE
    ${SRC_DIR}
    ${LIBS_DIR}
    ${LIBS_DIR}/glm
    ${LIBS_DIR}/glfw/include
    ${IMGUI_DIR}
    ${LIBS_DIR}/assimp/include
)
target_link_libraries(rtrt PRIVATE
    Vulkan::Vulkan
    ${LIBS_DIR}/assimp/64/assimp.lib
    ${LIBS_DIR}/glfw/lib-vc2019/glfw3.lib
)
# The prebuilt glfw3.lib uses the release CRT.
target_link_options(rtrt PRIVATE $<$<CONFIG:Debug>:/NODEFAULTLIB:MSVCRT>)
set_target_properties(rtrt PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

add_custom_command(TARGET rtrt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LIBS_DIR}/assimp/64/assimp.dll $<TARGET_FILE_DIR:rtrt>
)

set_target_properties(rtrt PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${SRC_DIR})
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT rtrt)
source_group(TREE ${SRC_DIR} PREFIX src FILES ${APP_SOURCES})
source_group(imgui FILES ${IMGUI_SOURCES})
