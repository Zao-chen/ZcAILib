function(run_checked)
    execute_process(COMMAND ${ARGV} RESULT_VARIABLE result)
    if(NOT result STREQUAL "0")
        message(FATAL_ERROR "Installed SDK consumer check failed: ${result}")
    endif()
endfunction()
string(RANDOM LENGTH 10 ALPHABET 0123456789abcdef suffix)
set(test_dir "${SDK_BINARY_DIR}/consumer-${suffix}")
run_checked("${CMAKE_COMMAND}" --install "${SDK_BINARY_DIR}" --config "${CONFIG}"
    --prefix "${test_dir}/original")
file(RENAME "${test_dir}/original" "${test_dir}/relocated")
set(generator_args -G "${GENERATOR}")
if(GENERATOR_PLATFORM)
    list(APPEND generator_args -A "${GENERATOR_PLATFORM}")
endif()
run_checked("${CMAKE_COMMAND}" -S "${CMAKE_CURRENT_LIST_DIR}/consumer"
    -B "${test_dir}/build" ${generator_args}
    "-DCMAKE_BUILD_TYPE=${CONFIG}" "-DSDK_PREFIX=${test_dir}/relocated"
    "-DCMAKE_PREFIX_PATH=${QT_PREFIX}")
run_checked("${CMAKE_COMMAND}" --build "${test_dir}/build" --config "${CONFIG}" --parallel 2)
if(WIN32)
    set(ENV{PATH} "${test_dir}/relocated/bin;${QT_PREFIX}/bin;$ENV{PATH}")
elseif(UNIX AND NOT APPLE)
    # Qt is an external SDK dependency. ELF RUNPATH on the executable is not
    # transitive to dependencies of the relocated shared library.
    set(ENV{LD_LIBRARY_PATH} "${QT_PREFIX}/lib:$ENV{LD_LIBRARY_PATH}")
endif()
run_checked("${CMAKE_CTEST_COMMAND}" --test-dir "${test_dir}/build" -C "${CONFIG}" --output-on-failure)
