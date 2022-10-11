include(FetchContent)

FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v2.9.0
    GIT_SHALLOW ON
)

set_property(GLOBAL PROPERTY PUFFIN_TEST_SOURCES "")
set_property(GLOBAL PROPERTY PUFFIN_TEST_DEPS "")

#
# Helper to declare a Puffin module
#
function(puffin_declare_module)
    set(options "")
    set(oneValueArgs NAME)
    set(multiValueArgs HEADERS SOURCES FEATURES DEPS)

    cmake_parse_arguments(PUFFIN_MODULE "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(TMP_NAME ${PUFFIN_MODULE_NAME})
    first_letter_upper(TMP_NAME)

    set(PUFFIN_MODULE_TARGETS_NAME "Puffin${TMP_NAME}Targets")

    if ("${PUFFIN_MODULE_SOURCES}" STREQUAL "")
        set(PUFFIN_MODULE_IS_INTERFACE 1)
    else()
        set(PUFFIN_MODULE_IS_INTERFACE 0)
    endif()

    if(PUFFIN_MODULE_IS_INTERFACE)
        add_library("${PUFFIN_MODULE_NAME}" INTERFACE)

        target_include_directories("${PUFFIN_MODULE_NAME}"
            INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
            INTERFACE $<INSTALL_INTERFACE:include>
        )

        target_link_libraries("${PUFFIN_MODULE_NAME}"
            INTERFACE
                ${PUFFIN_MODULE_DEPS}
        )

        target_compile_features("${PUFFIN_MODULE_NAME}"
            INTERFACE
                ${PUFFIN_MODULE_FEATURES}
        )
    else()

    endif()

    install(DIRECTORY include/ DESTINATION include)

    install(TARGETS "${PUFFIN_MODULE_NAME}" EXPORT "${PUFFIN_MODULE_TARGETS_NAME}"
        RUNTIME DESTINATION bin
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        INCLUDES DESTINATION include
    )

    install(
        EXPORT
            "${PUFFIN_MODULE_TARGETS_NAME}"
        FILE
            "${PUFFIN_MODULE_TARGETS_NAME}.cmake"
        NAMESPACE
            puffin::
        DESTINATION
            lib/cmake/puffin
    )

    export(TARGETS "${PUFFIN_MODULE_NAME}" FILE "${PUFFIN_MODULE_TARGETS_NAME}.cmake" NAMESPACE puffin::)

    add_library("puffin::${PUFFIN_MODULE_NAME}" ALIAS "${PUFFIN_MODULE_NAME}")
endfunction()

#
# Helper to declare a sample
#
function(puffin_declare_samples)
    set(options "")
    set(oneValueArgs "NAME")
    set(multiValueArgs SOURCES DEPS)

    cmake_parse_arguments(PUFFIN_SAMPLE "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_executable("${PUFFIN_SAMPLE_NAME}" "${PUFFIN_SAMPLE_SOURCES}")
    target_link_libraries("${PUFFIN_SAMPLE_NAME}" ${PUFFIN_SAMPLE_DEPS})
    set_target_properties("${PUFFIN_SAMPLE_NAME}"
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/samples"
    )
endfunction()

#
# Helper to declare a test
#
function(puffin_declare_tests)
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs SOURCES DEPS)

    cmake_parse_arguments(PUFFIN_ADD_TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    get_property(SOURCES GLOBAL PROPERTY PUFFIN_TEST_SOURCES)
    get_property(DEPS GLOBAL PROPERTY PUFFIN_TEST_DEPS)

    foreach(src ${PUFFIN_ADD_TEST_SOURCES})
        list(APPEND SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
    endforeach()
        
    list(APPEND DEPS ${PUFFIN_ADD_TEST_DEPS})
    
    set_property(GLOBAL PROPERTY PUFFIN_TEST_SOURCES "${SOURCES}")
    set_property(GLOBAL PROPERTY PUFFIN_TEST_DEPS "${DEPS}")
endfunction()

#
# Helper to add main test host application
#
function(puffin_main_tests)
    FetchContent_GetProperties(catch2)
    if(NOT catch2_POPULATED)
        FetchContent_Populate(catch2)
        add_subdirectory(${catch2_SOURCE_DIR} ${catch2_BINARY_DIR})
    endif()
    
    get_property(SOURCES GLOBAL PROPERTY PUFFIN_TEST_SOURCES)
    get_property(DEPS GLOBAL PROPERTY PUFFIN_TEST_DEPS)

    add_executable("main_test" ${CMAKE_SOURCE_DIR}/tests/main_test.cpp "${SOURCES}")
    target_link_libraries("main_test" Catch2 "${DEPS}")

    set_target_properties("main_test"
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )

    add_test(NAME Puffin COMMAND "${CMAKE_BINARY_DIR}/tests/main_test")
endfunction()


function(first_letter_upper args)
    string(SUBSTRING "${${args}}" 0 1 FIRST_LETTER)
    string(TOUPPER ${FIRST_LETTER} FIRST_LETTER)
    string(REGEX REPLACE "^.(.*)" "${FIRST_LETTER}\\1" RESULT "${${args}}")
    set(${args} ${RESULT} PARENT_SCOPE)
endfunction()
