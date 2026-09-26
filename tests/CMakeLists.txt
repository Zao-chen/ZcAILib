get_target_property(zcailib_qmake Qt6::qmake IMPORTED_LOCATION)
get_filename_component(zcailib_qt_bin "${zcailib_qmake}" DIRECTORY)
get_filename_component(zcailib_qt_prefix "${zcailib_qt_bin}" DIRECTORY)
foreach(link_target IN LISTS zcailib_install_targets)
    set(test_target "${link_target}_tests")
    add_executable(${test_target} aiprovider_test.cpp)
    target_link_libraries(${test_target} PRIVATE ${link_target} Qt6::Test)
    if(MSVC)
        target_compile_options(${test_target} PRIVATE /utf-8)
    endif()
    if(WIN32 AND link_target STREQUAL "ZcAiLib")
        add_custom_command(TARGET ${test_target} POST_BUILD COMMAND ${CMAKE_COMMAND}
            -E copy_if_different "$<TARGET_FILE:ZcAiLib>" "$<TARGET_FILE_DIR:${test_target}>")
    endif()
    add_test(NAME ${test_target} COMMAND ${CMAKE_COMMAND}
        "-DPROGRAM=$<TARGET_FILE:${test_target}>" "-DQT_BIN=${zcailib_qt_bin}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run-test.cmake")
    set_tests_properties(${test_target} PROPERTIES TIMEOUT 90)
endforeach()
if(ZCAILIB_INSTALL)
    add_test(NAME installed_consumer COMMAND ${CMAKE_COMMAND}
        "-DSDK_BINARY_DIR=${PROJECT_BINARY_DIR}" "-DCONFIG=$<CONFIG>"
        "-DQT_PREFIX=${zcailib_qt_prefix}" "-DGENERATOR=${CMAKE_GENERATOR}"
        "-DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/installed-consumer.cmake")
    set_tests_properties(installed_consumer PROPERTIES TIMEOUT 180)
endif()
