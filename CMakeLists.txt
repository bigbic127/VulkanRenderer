cmake_minimum_required(VERSION 3.29)
project(VulkanRenderer)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
#Library
find_package(Vulkan REQUIRED)#Vulkan
#FetchContent
include(FetchContent)
#GLFW
FetchContent_Declare(
    GLFW
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW true
    CMAKE_ARGS
        -DGLFW_BUILD_DOCS=OFF
        -DGLFW_BUILD_TESTS=OFF
        -DGLFW_BUILD_EXAMPLES=OFF
)
FetchContent_MakeAvailable(GLFW)
#GLM
FetchContent_Declare(
    GLM
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.3
    GIT_SHALLOW true
)
FetchContent_MakeAvailable(GLM)
#IMGUI
FetchContent_Declare(
    IMGUI
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.5-docking
    GIT_SHALLOW true
)
FetchContent_MakeAvailable(IMGUI)
set(IMGUI_SRC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_internal.h
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/imconfig.h
)
add_library(IMGUILIB STATIC ${IMGUI_SRC})
target_include_directories(IMGUILIB PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(IMGUILIB PUBLIC glfw)
#STB
FetchContent_Declare(
    STB
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
    GIT_SHALLOW 1
)
FetchContent_MakeAvailable(STB)
add_library(STBLIB INTERFACE)
target_include_directories(STBLIB INTERFACE
    ${stb_SOURCE_DIR}
)
#SourceCode
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
add_executable(${PROJECT_NAME} ${SOURCES})
#build
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        Vulkan::Vulkan
        glfw
        glm
        IMGUILIB
        STBLIB
    )
    set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY   "${CMAKE_SOURCE_DIR}/bin"
        OUTPUT_NAME_DEBUG   "${PROJECT_NAME}_debug"
        OUTPUT_NAME_RELEASE "${PROJECT_NAME}_release"
    )
    if (MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
    endif()
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/
    ${Vulkan_INCLUDE_DIRS}
)