cmake_minimum_required(VERSION 3.16)

project(ZcAiLib VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Core Widgets Network REQUIRED)

# ========== 构建库 ==========
add_library(ZcAiLib SHARED
    src/ZcAiLib_global.h
    src/AiProvider.h
    src/AiProvider.cpp
)

target_link_libraries(ZcAiLib PUBLIC
    Qt6::Core
    Qt6::Network
)

target_compile_definitions(ZcAiLib PRIVATE ZCAILIB_LIBRARY)

target_include_directories(ZcAiLib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

set_target_properties(ZcAiLib PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

# ========== 构建示例程序 ==========
add_executable(ZcAiLib-Example
    example/main.cpp
    example/mainwindow.h
    example/mainwindow.cpp
    example/mainwindow.ui
)

target_link_libraries(ZcAiLib-Example PRIVATE
    Qt6::Widgets
    ZcAiLib
)

# Windows: 自动复制 DLL
if(WIN32)
    add_custom_command(TARGET ZcAiLib-Example POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:ZcAiLib>
        $<TARGET_FILE_DIR:ZcAiLib-Example>
        COMMENT "Copying ZcAiLib.dll to example directory"
    )
endif()
