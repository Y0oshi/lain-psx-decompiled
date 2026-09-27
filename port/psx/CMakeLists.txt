# PsyCross (vendored, MIT) plus Lain-specific additions: the PlayStation SDK
# reimplemented on SDL2 + OpenGL + OpenAL.
option(PSX_LAIN_EXT "Build the Lain additions (src/lain_*)" ON)
file(GLOB_RECURSE PSX_SRCS_C   CONFIGURE_DEPENDS src/*.c src/*.C)
file(GLOB_RECURSE PSX_SRCS_CPP CONFIGURE_DEPENDS src/*.cpp)
if(NOT PSX_LAIN_EXT)
    list(FILTER PSX_SRCS_C EXCLUDE REGEX "/src/lain_")
    list(FILTER PSX_SRCS_CPP EXCLUDE REGEX "/src/lain_")
endif()
set_source_files_properties(${PSX_SRCS_C} PROPERTIES LANGUAGE C)
set_source_files_properties(${PSX_SRCS_CPP} PROPERTIES LANGUAGE CXX)

add_library(psx STATIC ${PSX_SRCS_C} ${PSX_SRCS_CPP})
target_include_directories(psx PUBLIC include include/psx)
# lain: no PGXP. The game builds 2D primitives itself with plain short screen
# coordinates (as on the PS1); PGXP would need half floats in every one of them.
target_compile_definitions(psx PUBLIC USE_PGXP=0)
# macOS/BSD: no <malloc.h>; PsyCross headers use static_assert without <assert.h>.
target_include_directories(psx BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../compat)
target_compile_options(psx PRIVATE $<$<COMPILE_LANGUAGE:C>:-include assert.h>
                                   $<$<COMPILE_LANGUAGE:CXX>:-Wno-c++11-narrowing>)

# SDL2/OpenAL come from the system, or from LAIN_STATIC_DEPS (top-level CMakeLists).
if(NOT TARGET SDL2::SDL2)
    find_package(SDL2 REQUIRED)
endif()
find_package(OpenGL REQUIRED)
if(NOT TARGET OpenAL::OpenAL)
    if(APPLE)
        # Use openal-soft (AL/al.h), not Apple's deprecated OpenAL.framework.
        set(CMAKE_FIND_FRAMEWORK LAST)
        find_package(OpenAL REQUIRED)
        set(CMAKE_FIND_FRAMEWORK FIRST)
    else()
        find_package(OpenAL REQUIRED)
    endif()
    # FindOpenAL points at .../include/AL; PsyCross includes <AL/al.h>.
    get_filename_component(OPENAL_INCLUDE_PARENT "${OPENAL_INCLUDE_DIR}" DIRECTORY)
    target_include_directories(psx PUBLIC ${OPENAL_INCLUDE_PARENT})
endif()
target_link_libraries(psx PUBLIC SDL2::SDL2 OpenAL::OpenAL OpenGL::GL)
