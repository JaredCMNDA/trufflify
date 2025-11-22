cmake_minimum_required(VERSION 3.15)
project(trufflify)

# set C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# static build
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)

#mingw static linking because thats how i build
if(MINGW)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
endif()

# fetch sfml
include(FetchContent)
FetchContent_Declare(
        sfml
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG 2.6.1 # stable version
)

# download + build sfml
FetchContent_MakeAvailable(sfml)

# add executable
add_executable(trufflify main.cpp)

# link sfml
target_link_libraries(trufflify PRIVATE sfml-graphics sfml-window sfml-system)
if(WIN32)
    target_link_libraries(trufflify PRIVATE sfml-main)
    if(MINGW)
        target_link_libraries(trufflify PRIVATE winmm gdi32 opengl32)
    endif()
endif()

# copy trufflify secret sauce
file(GLOB SECRETSAUCE "${CMAKE_SOURCE_DIR}/secretsauce/truffle.png")

add_custom_command(TARGET trufflify POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        "${SECRETSAUCE}"
        "$<TARGET_FILE_DIR:trufflify>")


