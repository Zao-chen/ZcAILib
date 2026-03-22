cmake_minimum_required(VERSION 3.16)

project(ZcAiLib VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Core Widgets Network REQUIRED)

# ========== 构建库 ==========
add_library(ZcAiLib SHARED
    src/ZcAiLib_global.h
    src/aiprovider.h
    src/aiprovider.cpp
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

install(TARGETS ZcAiLib
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES
    src/aiprovider.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    RENAME AiProvider.h
)

install(FILES
    src/ZcAiLib_global.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
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

# Windows: 自动复制 DLL 和部署 Qt 运行库
if(WIN32)
    add_custom_command(TARGET ZcAiLib-Example POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:ZcAiLib>
        $<TARGET_FILE_DIR:ZcAiLib-Example>
        COMMENT "Copying ZcAiLib.dll to example directory"
    )
    
    # 获取 Qt 的 bin 目录
    get_target_property(Qt6_BIN_DIR Qt6::Core LOCATION)
    get_filename_component(Qt6_BIN_DIR "${Qt6_BIN_DIR}" DIRECTORY)
    
    add_custom_command(TARGET ZcAiLib-Example POST_BUILD
        COMMAND "${Qt6_BIN_DIR}/windeployqt.exe"
        "$<TARGET_FILE:ZcAiLib-Example>"
        COMMENT "Deploying Qt runtime libraries"
    )
endif()
