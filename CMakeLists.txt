# CMakeLists.txt - Using just zlib with its included minizip
cmake_minimum_required(VERSION 3.15)
project(FSLint VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# Force static linking for all dependencies fetched via FetchContent
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)

# Dependencies (libxml2, Catch2) have issues when installed alongside our static internal targets
# because they try to export targets that depend on our internal zlibstatic.
# We define a dummy install macro to skip all installation rules in dependencies.
macro(install)
endmacro()

# ============================================================================
# Compiler-specific options
# ============================================================================
if(MSVC)
    # MSVC (also applies to Clang-CL)
    add_compile_options(/W4 /permissive- /EHsc)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    # GCC, Clang, and Clang on Unix-like systems
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(Python3 COMPONENTS Interpreter Development)

# ============================================================================
# Generate VS Code launch.json from template
# ============================================================================
if(Python3_Interpreter_FOUND)
    execute_process(
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/generate_launch.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        RESULT_VARIABLE UPDATE_LAUNCH_RESULT
    )
    if(NOT UPDATE_LAUNCH_RESULT EQUAL 0)
        message(WARNING "Failed to update .vscode/launch.json from template.")
    endif()
endif()

# ============================================================================
# Clang-Tidy Integration
# ============================================================================
option(CLANG_TIDY_AS_ERRORS "Treat clang-tidy warnings as errors" OFF)

find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
find_program(RUN_CLANG_TIDY_EXE NAMES "run-clang-tidy" "run-clang-tidy.py")

# Collect all source files to be checked
file(GLOB_RECURSE TIDY_SOURCES
    ${CMAKE_SOURCE_DIR}/core/*.cpp
    ${CMAKE_SOURCE_DIR}/cli/*.cpp
)
list(LENGTH TIDY_SOURCES TIDY_SOURCES_COUNT)

if(CLANG_TIDY_EXE OR RUN_CLANG_TIDY_EXE)
    # Determine number of parallel jobs
    set(CLANG_TIDY_JOBS 0 CACHE STRING "Number of parallel clang-tidy jobs (0 for all cores)")
    if(CLANG_TIDY_JOBS EQUAL 0)
        include(ProcessorCount)
        ProcessorCount(N)
        if(N EQUAL 0)
            set(N 1)
        endif()
    else()
        set(N ${CLANG_TIDY_JOBS})
    endif()

    set(CLANG_TIDY_BIN ${CLANG_TIDY_EXE})
    set(CLANG_TIDY_ARGS
        -p ${CMAKE_BINARY_DIR}
        "-header-filter=(core|cli|tests)/.*"
    )
    if(CLANG_TIDY_AS_ERRORS)
        list(APPEND CLANG_TIDY_ARGS "--warnings-as-errors=*")
    endif()
    set(CLANG_TIDY_TARGET_FILES ${TIDY_SOURCES})

    if(RUN_CLANG_TIDY_EXE AND Python3_Interpreter_FOUND)
        if(WIN32)
            set(CLANG_TIDY_BIN ${Python3_EXECUTABLE} ${RUN_CLANG_TIDY_EXE})
        else()
            set(CLANG_TIDY_BIN ${RUN_CLANG_TIDY_EXE})
        endif()

        set(CLANG_TIDY_ARGS
            -p ${CMAKE_BINARY_DIR}
            -j ${N}
            -clang-tidy-binary ${CLANG_TIDY_EXE}
            "-header-filter=.*"
        )
        if(CLANG_TIDY_AS_ERRORS)
            list(APPEND CLANG_TIDY_ARGS "-warnings-as-errors=*")
        endif()

        # run-clang-tidy uses regex matching for files. To be cross-platform (handling both / and \),
        # we convert implementation file paths to relative regex patterns that are slash-agnostic.
        set(CLANG_TIDY_TARGET_FILES "")
        foreach(F ${TIDY_SOURCES})
            file(RELATIVE_PATH F_REL ${CMAKE_SOURCE_DIR} ${F})
            string(REPLACE "/" "." F_REGEX "${F_REL}")
            string(REPLACE "\\" "." F_REGEX "${F_REGEX}")
            list(APPEND CLANG_TIDY_TARGET_FILES "${F_REGEX}$")
        endforeach()
    endif()

    add_custom_target(clang-tidy
        COMMAND ${CLANG_TIDY_BIN}
            ${CLANG_TIDY_ARGS}
            ${CLANG_TIDY_TARGET_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy with ${N} jobs on ${TIDY_SOURCES_COUNT} files..."
        USES_TERMINAL
        COMMAND_EXPAND_LISTS
        VERBATIM
    )
else()
    add_custom_target(clang-tidy
        COMMAND ${CMAKE_COMMAND} -E echo "Error: clang-tidy not found"
        COMMAND ${CMAKE_COMMAND} -E false
    )
endif()

# ============================================================================
# Clang-Format Integration
# ============================================================================
find_program(CLANG_FORMAT_EXE NAMES "clang-format")

# Collect all source files
file(GLOB_RECURSE ALL_SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/core/*.cpp
    ${CMAKE_SOURCE_DIR}/core/*.h
    ${CMAKE_SOURCE_DIR}/cli/*.cpp
    ${CMAKE_SOURCE_DIR}/tests/*.cpp
)

if(CLANG_FORMAT_EXE)
    message(STATUS "clang-format found: ${CLANG_FORMAT_EXE}")

    # Add format target
    add_custom_target(clang-format
        COMMAND ${CLANG_FORMAT_EXE}
        -i
        -style=file
        ${ALL_SOURCE_FILES}
        COMMENT "Running clang-format on source files"
    )

    # Add format check target
    add_custom_target(clang-format-check
        COMMAND ${CLANG_FORMAT_EXE}
        --dry-run
        --Werror
        -style=file
        ${ALL_SOURCE_FILES}
        COMMENT "Checking formatting of source files"
    )
else()
    message(WARNING "clang-format not found")
    add_custom_target(clang-format
        COMMAND ${CMAKE_COMMAND} -E echo "Error: clang-format not found"
        COMMAND ${CMAKE_COMMAND} -E false
    )
    add_custom_target(clang-format-check
        COMMAND ${CMAKE_COMMAND} -E echo "Error: clang-format not found"
        COMMAND ${CMAKE_COMMAND} -E false
    )
endif()

include(FetchContent)

# ============================================================================
# XML-Format Integration
# ============================================================================
if(Python3_Interpreter_FOUND)
    add_custom_target(format-xml
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/format_xml.py
        COMMENT "Running XML formatter on test data"
    )

    add_custom_target(format-xml-check
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/format_xml.py --check
        COMMENT "Checking XML formatting on test data"
    )
else()
    add_custom_target(format-xml
        COMMAND ${CMAKE_COMMAND} -E echo "Error: Python3 not found"
        COMMAND ${CMAKE_COMMAND} -E false
    )
    add_custom_target(format-xml-check
        COMMAND ${CMAKE_COMMAND} -E echo "Error: Python3 not found"
        COMMAND ${CMAKE_COMMAND} -E false
    )
endif()

# ============================================================================
# Fetch ZLIB (includes minizip in contrib/)
# ============================================================================
FetchContent_Declare(
    zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG        v1.3.1
)

# Disable warnings for zlib compilation
set(CMAKE_C_FLAGS_BACKUP "${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS_BACKUP "${CMAKE_CXX_FLAGS}")

if(MSVC)
    # Pure MSVC
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /w")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /w")
else()
    # GCC, Clang, and Clang on Unix-like systems
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")
endif()

# Prevent zlib from building examples and adding tests
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(zlib)

# Restore warning flags after zlib
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_BACKUP}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_BACKUP}")

# Disable clang-tidy for zlib
if(TARGET zlibstatic)
    set_target_properties(zlibstatic PROPERTIES
        CXX_CLANG_TIDY ""
        C_CLANG_TIDY ""
    )
endif()
if(TARGET zlib)
    set_target_properties(zlib PROPERTIES
        CXX_CLANG_TIDY ""
        C_CLANG_TIDY ""
    )
endif()

# Get zlib source directory
FetchContent_GetProperties(zlib SOURCE_DIR ZLIB_SOURCE_DIR BINARY_DIR ZLIB_BINARY_DIR)

# Set ZLIB variables for libxml2 to find
set(ZLIB_FOUND TRUE)
set(ZLIB_INCLUDE_DIR "${ZLIB_SOURCE_DIR};${ZLIB_BINARY_DIR}")
set(ZLIB_INCLUDE_DIRS "${ZLIB_SOURCE_DIR};${ZLIB_BINARY_DIR}")
set(ZLIB_LIBRARY zlibstatic)
set(ZLIB_LIBRARIES zlibstatic)
set(ZLIB_VERSION_STRING "1.3.1")

# Create an imported target for ZLIB (needed by libxml2)
if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
endif()

# ============================================================================
# Build minizip from zlib's contrib directory
# ============================================================================
set(MINIZIP_DIR ${ZLIB_SOURCE_DIR}/contrib/minizip)

add_library(minizip STATIC
    ${MINIZIP_DIR}/unzip.c
    ${MINIZIP_DIR}/zip.c
    ${MINIZIP_DIR}/ioapi.c
)

target_include_directories(minizip PUBLIC
    ${MINIZIP_DIR}
    ${ZLIB_SOURCE_DIR}
    ${ZLIB_BINARY_DIR}  # For zconf.h
)

target_link_libraries(minizip PUBLIC zlibstatic)

# Disable warnings for minizip
if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Pure MSVC
    target_compile_options(minizip PRIVATE /w)
else()
    # GCC, Clang, or Clang on Windows
    target_compile_options(minizip PRIVATE -w)
endif()

# Disable clang-tidy for minizip
set_target_properties(minizip PROPERTIES
    CXX_CLANG_TIDY ""
    C_CLANG_TIDY ""
)

# Windows-specific
if(WIN32)
    target_sources(minizip PRIVATE ${MINIZIP_DIR}/iowin32.c)
    target_compile_definitions(minizip PRIVATE USE_FILE32API)
endif()

# ============================================================================
# Fetch LibXML2
# ============================================================================
FetchContent_Declare(
    libxml2
    GIT_REPOSITORY https://github.com/GNOME/libxml2.git
    GIT_TAG        v2.12.4
)

set(LIBXML2_WITH_TESTS OFF CACHE BOOL "" FORCE)
set(LIBXML2_WITH_PROGRAMS OFF CACHE BOOL "" FORCE)
set(LIBXML2_WITH_SHARED OFF CACHE BOOL "" FORCE)
set(LIBXML2_WITH_STATIC ON CACHE BOOL "" FORCE)
set(LIBXML2_WITH_ICONV OFF CACHE BOOL "" FORCE)
set(LIBXML2_WITH_ICU OFF CACHE BOOL "" FORCE)
set(LIBXML2_WITH_PYTHON OFF CACHE BOOL "" FORCE)
set(LIBXML2_WITH_ZLIB ON CACHE BOOL "" FORCE)
set(LIBXML2_WITH_LZMA OFF CACHE BOOL "" FORCE)
set(LIBXML2_WITH_HTTP OFF CACHE BOOL "" FORCE)
set(LIBXML2_WITH_FTP OFF CACHE BOOL "" FORCE)
set(LIBXML2_WITH_THREADS ON CACHE BOOL "" FORCE)
# Use a separate install prefix for dependencies to avoid export issues
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/dependency_install")


# Disable warnings for libxml2 compilation
set(CMAKE_C_FLAGS_BACKUP "${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS_BACKUP "${CMAKE_CXX_FLAGS}")

if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Pure MSVC
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /w")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /w")
else()
    # GCC, Clang, or Clang on Windows
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")
endif()

# We don't use FetchContent_MakeAvailable(libxml2) here because we need the
# dummy install macro to be active when add_subdirectory is called.
FetchContent_GetProperties(libxml2)
if(NOT libxml2_POPULATED)
    FetchContent_Populate(libxml2)
    add_subdirectory(${libxml2_SOURCE_DIR} ${libxml2_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

# Restore warning flags after libxml2
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_BACKUP}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_BACKUP}")

# Disable clang-tidy for libxml2 (the actual library target, not the alias)
if(TARGET xml2)
    set_target_properties(xml2 PROPERTIES
        CXX_CLANG_TIDY ""
        C_CLANG_TIDY ""
    )
endif()

# ============================================================================
# Fetch PicoSHA2
# ============================================================================
FetchContent_Declare(
    picosha2
    GIT_REPOSITORY https://github.com/okdshin/PicoSHA2.git
    GIT_TAG        b699e6c900be6e00152db5a3d123c1db42ea13d0  # Latest commit
)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.4.0
)

FetchContent_Declare(
  pybind11
  GIT_REPOSITORY https://github.com/pybind/pybind11.git
  GIT_TAG        v2.12.0
)

FetchContent_MakeAvailable(picosha2 Catch2 pybind11)

# ============================================================================
# Copy fmi and ssp files from source to build directory
# ============================================================================
file(GLOB_RECURSE FMI_FILES "${CMAKE_SOURCE_DIR}/standard/fmi/*")
foreach(FMI_FILES ${FMI_FILES})
    file(RELATIVE_PATH REL_PATH "${CMAKE_SOURCE_DIR}/standard/fmi/" ${FMI_FILES})
    get_filename_component(REL_DIR ${REL_PATH} DIRECTORY)

    # Copy to build FMI_FILES
    file(COPY ${FMI_FILES}
         DESTINATION "${CMAKE_BINARY_DIR}/standard/fmi/${REL_DIR}")
endforeach()

file(GLOB_RECURSE SSP_FILES "${CMAKE_SOURCE_DIR}/standard/ssp/*")
foreach(SSP_FILES ${SSP_FILES})
    file(RELATIVE_PATH REL_PATH "${CMAKE_SOURCE_DIR}/standard/ssp/" ${SSP_FILES})
    get_filename_component(REL_DIR ${REL_PATH} DIRECTORY)

    # Copy to build SSP_FILES
    file(COPY ${SSP_FILES}
         DESTINATION "${CMAKE_BINARY_DIR}/standard/ssp/${REL_DIR}")
endforeach()

# ============================================================================
# Core library
# ============================================================================
file(GLOB_RECURSE CORE_SOURCES "core/*.cpp")
include_directories(core/checker/directory)
include_directories(core/checker/build_description)
include_directories(core/checker/terminals_and_icons)
include_directories(core/checker/binary)
include_directories(core/checker/resources)
add_library(FSLint-core STATIC ${CORE_SOURCES})

file(GLOB_RECURSE HEADER_DIRS LIST_DIRECTORIES true "core/*")
foreach(DIR ${HEADER_DIRS})
    if(IS_DIRECTORY ${DIR})
        file(RELATIVE_PATH REL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/core" "${DIR}")
        target_include_directories(FSLint-core PUBLIC
            $<BUILD_INTERFACE:${DIR}>
            $<INSTALL_INTERFACE:include/${REL_DIR}>
        )
    endif()
endforeach()

target_include_directories(FSLint-core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core>
    $<INSTALL_INTERFACE:include>
)

# Mark external dependencies as SYSTEM to suppress warnings
target_include_directories(FSLint-core SYSTEM PRIVATE
    ${ZLIB_INCLUDE_DIRS}
    ${MINIZIP_DIR}
    ${picosha2_SOURCE_DIR}
)

target_include_directories(FSLint-core SYSTEM PRIVATE
    $<TARGET_PROPERTY:LibXml2::LibXml2,INTERFACE_INCLUDE_DIRECTORIES>
)

target_compile_definitions(FSLint-core PRIVATE
    PROJECT_VERSION="${CMAKE_PROJECT_VERSION}"
    PROJECT_VERSION_MAJOR=${CMAKE_PROJECT_VERSION_MAJOR}
    PROJECT_VERSION_MINOR=${CMAKE_PROJECT_VERSION_MINOR}
    PROJECT_VERSION_PATCH=${CMAKE_PROJECT_VERSION_PATCH}
)

target_link_libraries(FSLint-core
    PUBLIC
        $<BUILD_INTERFACE:LibXml2::LibXml2>
        $<INSTALL_INTERFACE:LibXml2::LibXml2>
    PRIVATE
        minizip
        zlibstatic
)

# Windows-specific libraries
if(WIN32)
    target_link_libraries(FSLint-core PRIVATE ws2_32)
endif()

# ============================================================================
# CLI executable
# ============================================================================
add_executable(FSLint-cli
    cli/main.cpp
)

if(EMSCRIPTEN)
    target_link_options(FSLint-cli PRIVATE
        "-sWASM=1"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sEXIT_RUNTIME=0"
        "-sMODULARIZE=1"
        "-sEXPORT_NAME=createFSLintModule"
        "-sINVOKE_RUN=0"
        "-sEXPORTED_RUNTIME_METHODS=['FS', 'callMain']"
        "--preload-file ${CMAKE_SOURCE_DIR}/standard@/standard"
    )
endif()

# ============================================================================
# Tests
# ============================================================================
enable_testing()

add_executable(FSLint-tests
    tests/model_description_tests.cpp
    tests/certificate_tests.cpp
    tests/certificate_management_tests.cpp
    tests/fmu_structure_tests.cpp
    tests/terminals_and_icons_tests.cpp
    tests/binary_checker_tests.cpp
    tests/recursive_checker_tests.cpp
    tests/schema_checker_tests.cpp
)

target_link_libraries(FSLint-tests PRIVATE
    FSLint-core
    Catch2::Catch2WithMain
)

# Mark external dependencies as SYSTEM to suppress warnings
target_include_directories(FSLint-tests SYSTEM PRIVATE
    $<TARGET_PROPERTY:Catch2::Catch2,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:Catch2::Catch2WithMain,INTERFACE_INCLUDE_DIRECTORIES>
)

# Add tests to CTest via Catch2 discovery
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(FSLint-tests)

if(Python3_Interpreter_FOUND)
    add_test(NAME PythonBindingsTest
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/python/test_bindings.py
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

# Copy test data to build directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/tests/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/tests)

# Check for Reference FMUs (required for binary tests)
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference_fmus/BouncingBall_20")
    message(WARNING "Reference FMUs for binary tests are missing. Some tests will be skipped. "
                    "Run 'scripts/download_reference_fmus.py' to download them.")
endif()

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT FSLint-cli)

target_link_libraries(FSLint-cli PRIVATE FSLint-core)

if(UNIX)
    if(APPLE)
        set_target_properties(FSLint-cli PROPERTIES INSTALL_RPATH "@loader_path")
    else()
        set_target_properties(FSLint-cli PROPERTIES INSTALL_RPATH "$ORIGIN")
    endif()
    set_target_properties(FSLint-cli PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
endif()

target_compile_definitions(FSLint-cli PRIVATE
    PROJECT_VERSION="${CMAKE_PROJECT_VERSION}"
    PROJECT_VERSION_MAJOR=${CMAKE_PROJECT_VERSION_MAJOR}
    PROJECT_VERSION_MINOR=${CMAKE_PROJECT_VERSION_MINOR}
    PROJECT_VERSION_PATCH=${CMAKE_PROJECT_VERSION_PATCH}
)

# ============================================================================
# Copy DLLs to output directory on Windows
# ============================================================================
# libxml2, zlib and Catch2 are linked statically or handled by discover_tests, so no need to copy DLLs manually here

# ============================================================================
# Python bindings
# ============================================================================
if(Python3_Interpreter_FOUND AND Python3_Development_FOUND)
    pybind11_add_module(fslint python/bindings.cpp)
    target_link_libraries(fslint PRIVATE FSLint-core)
    target_include_directories(fslint PRIVATE core)

    target_compile_definitions(fslint PRIVATE
        PROJECT_VERSION="${CMAKE_PROJECT_VERSION}"
        PROJECT_VERSION_MAJOR=${CMAKE_PROJECT_VERSION_MAJOR}
        PROJECT_VERSION_MINOR=${CMAKE_PROJECT_VERSION_MINOR}
        PROJECT_VERSION_PATCH=${CMAKE_PROJECT_VERSION_PATCH}
    )

    # Set output name to just 'fslint' (the .so/.pyd will have the appropriate suffix)
    set_target_properties(fslint PROPERTIES OUTPUT_NAME "fslint")
endif()

# ============================================================================
# Install
# ============================================================================
install(TARGETS FSLint-cli DESTINATION bin)
if(Python3_Interpreter_FOUND AND Python3_Development_FOUND)
    install(TARGETS fslint DESTINATION bin)
endif()
install(DIRECTORY standard DESTINATION bin)
