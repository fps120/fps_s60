cmake_minimum_required(VERSION 3.16)
project(CS16_3D VERSION 1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(SFML 3 COMPONENTS Graphics Network Audio System Window REQUIRED)
find_package(glm QUIET)
if(NOT glm_FOUND)
    include(FetchContent)
    FetchContent_Declare(glm GIT_REPOSITORY https://github.com/g-truc/glm.git GIT_TAG 0.9.9.8)
    FetchContent_MakeAvailable(glm)
endif()

add_executable(cs_client
    vendor/glad/glad.c
    client/main.cpp
    client/Game.cpp
    client/Renderer3D.cpp
    client/Map3D.cpp
    client/Player3D.cpp
    client/WeaponModel.cpp
    client/NetworkClient.cpp
)
target_include_directories(cs_client PRIVATE
    ${CMAKE_SOURCE_DIR}/shared
    ${CMAKE_SOURCE_DIR}/client
    ${CMAKE_SOURCE_DIR}/vendor
    ${OPENGL_INCLUDE_DIR}
)
target_link_libraries(cs_client OpenGL::GL glfw glm::glm
    SFML::Graphics SFML::Network SFML::Audio SFML::System SFML::Window)
target_compile_options(cs_client PRIVATE -Wall -O2)

add_executable(cs_server
    server/main.cpp
    server/Server.cpp
    server/ServerGame3D.cpp
)
target_include_directories(cs_server PRIVATE
    ${CMAKE_SOURCE_DIR}/shared
    ${CMAKE_SOURCE_DIR}/server)
target_link_libraries(cs_server SFML::Network SFML::System)
target_compile_options(cs_server PRIVATE -Wall -O2)

file(COPY ${CMAKE_SOURCE_DIR}/shaders DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/maps DESTINATION ${CMAKE_BINARY_DIR})
