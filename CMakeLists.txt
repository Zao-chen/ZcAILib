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
set(ZCAILIB_SOURCES
    src/ZcAiLib_global.h
    src/aiprovider.h
    src/aiprovider.cpp
)

function(zcailib_configure_target target_name)
    target_link_libraries(${target_name} PUBLIC
        Qt6::Core
        Qt6::Network
    )

    target_include_directories(${target_name} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
    )

    set_target_properties(${target_name} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )
endfunction()

add_library(ZcAiLib SHARED ${ZCAILIB_SOURCES})
zcailib_configure_target(ZcAiLib)

target_compile_definitions(ZcAiLib PRIVATE ZCAILIB_LIBRARY)

set_target_properties(ZcAiLib PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

if(MINGW)
    set_target_properties(ZcAiLib PROPERTIES
        IMPORT_SUFFIX ".a"
    )
endif()

if(UNIX AND NOT APPLE)
    add_library(ZcAiLibStatic STATIC ${ZCAILIB_SOURCES})
    zcailib_configure_target(ZcAiLibStatic)

    target_compile_definitions(ZcAiLibStatic PUBLIC ZCAILIB_STATIC)

    set_target_properties(ZcAiLibStatic PROPERTIES
        OUTPUT_NAME ZcAiLib
    )
endif()

install(TARGETS ZcAiLib
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

if(TARGET ZcAiLibStatic)
    install(TARGETS ZcAiLibStatic
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()

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
