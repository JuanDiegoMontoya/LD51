# the project's main CMakeLists file

cmake_minimum_required(VERSION 3.15)

project(LD51)

set(CMAKE_CXX_STANDARD 20)

set(LD51_source_files
	"src/GAssert.cpp"
	"src/utils/LoadFile.cpp"
	"src/ecs/Entity.cpp" 
	"src/ecs/Scene.cpp"
	"src/ecs/systems/System.cpp"
	"src/ecs/systems/core/LifetimeSystem.cpp"
	"src/Renderer.cpp"
	"src/main.cpp"
	"src/Application.cpp" 
	"src/Input.cpp"
	"src/ecs/systems/RenderingSystem.cpp"
	"src/ecs/systems/DebugSystem.cpp"
	"src/ecs/systems/game/ParticleSystem.cpp"
)

set(LD51_header_files
	"src/GAssert.h"
	"src/Exception.h" 
	"src/utils/EventBus.h"
	"src/utils/LoadFile.h" 
	"src/utils/Timer.h" 
	"src/ecs/Entity.h"
	"src/ecs/Scene.h"
	"src/ecs/components/core/Lifetime.h"
	"src/ecs/components/core/Tag.h"
	"src/ecs/systems/core/LifetimeSystem.h"
	"src/ecs/systems/System.h"
	"src/ecs/components/core/Transform.h"
	"src/ecs/components/core/Sprite.h"
	"src/ecs/events/AxisBindingBase.h"
	"src/Renderer.h"
	"src/Application.h"
	"src/Input.h"
	"src/ecs/systems/RenderingSystem.h"
	"src/ecs/systems/DebugSystem.h"
	"src/ecs/components/DebugDraw.h"
	"src/ecs/systems/game/ParticleSystem.h"
	"src/ecs/events/AddParticles.h"
)

add_executable(LD51_game
	${LD51_source_files}
	${LD51_header_files}
)

# Determine whether we're compiling with clang++
string(FIND "${CMAKE_CXX_COMPILER}" "clang++" COMPILER_CLANGPP)
if(COMPILER_CLANGPP GREATER -1)
    set(COMPILER_CLANGPP 1)
else()
    set(COMPILER_CLANGPP 0)
endif()

# enable roughly equivalent (and strict!) compiler warnings depending on the compiler
target_compile_options(LD51_game PUBLIC
	$<$<OR:$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>,${COMPILER_CLANGPP}>:
	-Wall
	-Wextra
	-pedantic-errors
	-Wno-missing-field-initializers
	-Wno-unused-result
	>
	$<$<CXX_COMPILER_ID:MSVC>:
	-W4
	-WX
	-permissive-
	-wd4324 # disable warning indicating that a structure was padded
	>
)

find_package(OpenGL REQUIRED)

# enable asan for debug builds
if (DEBUG)
    target_compile_options(LD51_game PUBLIC -fsanitize=address)
endif()

add_subdirectory(external)

target_include_directories(LD51_game PUBLIC	src	vendor)

target_link_libraries(LD51_game glm EnTT::EnTT fwog glfw lib_glad imgui stb)

add_custom_target(copy_assets ALL COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/data/assets ${CMAKE_CURRENT_BINARY_DIR}/assets)
add_dependencies(LD51_game copy_assets)
