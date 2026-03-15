cmake_minimum_required(VERSION 3.16)
project(mugi_sdl2 C)
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(SDL2 REQUIRED)

# SDL2_mixer
find_package(PkgConfig)
if(PkgConfig_FOUND)
  pkg_check_modules(SDL2_MIXER REQUIRED SDL2_mixer)
else()
  find_library(SDL2_MIXER_LIBRARIES NAMES SDL2_mixer)
  find_path(SDL2_MIXER_INCLUDE_DIRS SDL_mixer.h PATH_SUFFIXES SDL2)
endif()

set(SOURCES
    src/main.c
    # ゲームコード (MSXから流用・最小修正)
    src/game/actpazle.c
    src/game/actpazle_password.c
    # HAL層
    src/hal/vdp_sdl2.c
    src/hal/input_sdl2.c
    src/hal/audio_sdl2.c
)

add_executable(mugi ${SOURCES})

target_include_directories(mugi PRIVATE
    include/
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_MIXER_INCLUDE_DIRS}
)

target_link_libraries(mugi PRIVATE
    ${SDL2_LIBRARIES}
    ${SDL2_MIXER_LIBRARIES}
)

if(WIN32)
    # Windowsではコンソール非表示、SDL2mainをリンク
    set_target_properties(mugi PROPERTIES WIN32_EXECUTABLE TRUE)
    # SDL2mainが SDL2::SDL2main として提供されている場合
    if(TARGET SDL2::SDL2main)
        target_link_libraries(mugi PRIVATE SDL2::SDL2main)
    endif()
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(mugi PRIVATE m)
endif()

target_compile_options(mugi PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/W3>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall -Wextra -Wno-unused-parameter>
)

# assets をビルドディレクトリにコピー
if(EXISTS "${CMAKE_SOURCE_DIR}/assets")
    file(COPY assets DESTINATION ${CMAKE_BINARY_DIR})
endif()

message(STATUS "Build: ${CMAKE_BUILD_TYPE}")
message(STATUS "SDL2: ${SDL2_INCLUDE_DIRS}")
message(STATUS "SDL2_mixer: ${SDL2_MIXER_INCLUDE_DIRS}")
