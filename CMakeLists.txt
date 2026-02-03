# CMakeLists.txt - Using just zlib with its included minizip
cmake_minimum_required(VERSION 3.15)
project(FSLint VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

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

# ============================================================================
# Clang-Tidy Integration
# ============================================================================
find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
if(CLANG_TIDY_EXE)
    message(STATUS "Found clang-tidy: ${CLANG_TIDY_EXE}")

    # Collect all source files to check
    file(GLOB_RECURSE TIDY_SOURCES
        ${CMAKE_SOURCE_DIR}/lib/*.cpp
        ${CMAKE_SOURCE_DIR}/lib/*.h
        ${CMAKE_SOURCE_DIR}/cli/*.cpp
        ${CMAKE_SOURCE_DIR}/tests/*.cpp
    )

    # Add custom target to run clang-tidy
    add_custom_target(clang-tidy
        COMMAND ${CLANG_TIDY_EXE}
            -p ${CMAKE_BINARY_DIR}
            --config-file=${CMAKE_SOURCE_DIR}/.clang-tidy
            "--header-filter=(lib|cli|tests)/.*"
            --system-headers=false
            ${TIDY_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy on source files..."
        VERBATIM
        USES_TERMINAL
        COMMAND_EXPAND_LISTS
    )
else()
    message(WARNING "clang-tidy not found!")
endif()

# ============================================================================
# Clang-Format Integration
# ============================================================================
find_program(CLANG_FORMAT_EXE NAMES "clang-format")
if(CLANG_FORMAT_EXE)
    message(STATUS "clang-format found: ${CLANG_FORMAT_EXE}")

    # Collect all source files
    file(GLOB_RECURSE ALL_SOURCE_FILES
        ${CMAKE_SOURCE_DIR}/lib/*.cpp
        ${CMAKE_SOURCE_DIR}/lib/*.h
        ${CMAKE_SOURCE_DIR}/cli/*.cpp
        ${CMAKE_SOURCE_DIR}/tests/*.cpp
    )

    # Add format target
    add_custom_target(clang-format
        COMMAND ${CLANG_FORMAT_EXE}
        -i
        -style=file
        ${ALL_SOURCE_FILES}
        COMMENT "Running clang-format on source files"
    )
else()
    message(WARNING "clang-format not found")
endif()

include(FetchContent)

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
    # GCC, Clang, Clang-CL, and other Unix-like compilers
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")
endif()

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
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

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
set(LIBXML2_WITH_ICONV OFF CACHE BOOL "" FORCE)
set(LIBXML2_WITH_PYTHON OFF CACHE BOOL "" FORCE)
set(LIBXML2_WITH_ZLIB ON CACHE BOOL "" FORCE)
set(LIBXML2_WITH_LZMA OFF CACHE BOOL "" FORCE)
set(LIBXML2_WITH_HTTP OFF CACHE BOOL "" FORCE)
set(LIBXML2_WITH_FTP OFF CACHE BOOL "" FORCE)

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

FetchContent_MakeAvailable(libxml2)

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

FetchContent_MakeAvailable(picosha2 Catch2)

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
file(GLOB_RECURSE LIB_SOURCES "lib/*.cpp")
add_library(FSLint-lib STATIC ${LIB_SOURCES})

file(GLOB_RECURSE HEADER_DIRS LIST_DIRECTORIES true "lib/*")
foreach(DIR ${HEADER_DIRS})
    if(IS_DIRECTORY ${DIR})
        target_include_directories(FSLint-lib PUBLIC
            $<BUILD_INTERFACE:${DIR}>
        )
    endif()
endforeach()

target_include_directories(FSLint-lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib>
    $<INSTALL_INTERFACE:include>
)

# Mark external dependencies as SYSTEM to suppress warnings
target_include_directories(FSLint-lib SYSTEM PRIVATE
    ${ZLIB_INCLUDE_DIRS}
    ${MINIZIP_DIR}
    ${picosha2_SOURCE_DIR}
)

target_include_directories(FSLint-lib SYSTEM PRIVATE
    $<TARGET_PROPERTY:LibXml2::LibXml2,INTERFACE_INCLUDE_DIRECTORIES>
)

target_compile_definitions(FSLint-lib PRIVATE
    PROJECT_VERSION="${CMAKE_PROJECT_VERSION}"
    PROJECT_VERSION_MAJOR=${CMAKE_PROJECT_VERSION_MAJOR}
    PROJECT_VERSION_MINOR=${CMAKE_PROJECT_VERSION_MINOR}
    PROJECT_VERSION_PATCH=${CMAKE_PROJECT_VERSION_PATCH}
)

target_link_libraries(FSLint-lib PUBLIC
    LibXml2::LibXml2
    minizip
    zlibstatic
)

# Windows-specific libraries
if(WIN32)
    target_link_libraries(FSLint-lib PUBLIC ws2_32)
endif()

# ============================================================================
# CLI executable
# ============================================================================
add_executable(FSLint-cli
    cli/main.cpp
)

# ============================================================================
# Tests
# ============================================================================
enable_testing()

add_executable(FSLint-tests
    tests/model_description_tests.cpp
)

target_link_libraries(FSLint-tests PRIVATE
    FSLint-lib
    Catch2::Catch2WithMain
)

# Mark external dependencies as SYSTEM to suppress warnings
target_include_directories(FSLint-tests SYSTEM PRIVATE
    $<TARGET_PROPERTY:Catch2::Catch2,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:Catch2::Catch2WithMain,INTERFACE_INCLUDE_DIRECTORIES>
)

# Add test to CTest
add_test(NAME ModelDescriptionTests COMMAND FSLint-tests)

# Copy test data to build directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/tests/data DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/tests)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT FSLint-cli)

target_link_libraries(FSLint-cli PRIVATE FSLint-lib)

target_compile_definitions(FSLint-cli PRIVATE
    PROJECT_VERSION="${CMAKE_PROJECT_VERSION}"
    PROJECT_VERSION_MAJOR=${CMAKE_PROJECT_VERSION_MAJOR}
    PROJECT_VERSION_MINOR=${CMAKE_PROJECT_VERSION_MINOR}
    PROJECT_VERSION_PATCH=${CMAKE_PROJECT_VERSION_PATCH}
)

# ============================================================================
# Copy DLLs to output directory on Windows
# ============================================================================
if(WIN32)
    add_custom_command(TARGET FSLint-cli POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:LibXml2::LibXml2>
            $<TARGET_FILE_DIR:FSLint-cli>
        COMMENT "Copying libxml2 DLL to output directory"
    )

    # Also copy zlib DLL if it exists
    if(TARGET zlib)
        add_custom_command(TARGET FSLint-cli POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:zlib>
                $<TARGET_FILE_DIR:FSLint-cli>
            COMMENT "Copying zlib DLL to output directory"
        )
    endif()

    if(TARGET Catch2)
        add_custom_command(TARGET FSLint-tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:Catch2>
                $<TARGET_FILE_DIR:FSLint-tests>
            COMMENT "Copying Catch2 DLL to output directory"
        )
    endif()

    if(TARGET Catch2::Catch2WithMain)
        add_custom_command(TARGET FSLint-tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:Catch2::Catch2WithMain>
                $<TARGET_FILE_DIR:FSLint-tests>
            COMMENT "Copying Catch2Main DLL to output directory"
        )
    endif()

    if(TARGET LibXml2::LibXml2)
        add_custom_command(TARGET FSLint-tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:LibXml2::LibXml2>
                $<TARGET_FILE_DIR:FSLint-tests>
            COMMENT "Copying libxml2 DLL to output directory"
        )
    endif()

    if(TARGET zlib)
        add_custom_command(TARGET FSLint-tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:zlib>
                $<TARGET_FILE_DIR:FSLint-tests>
            COMMENT "Copying zlib DLL to output directory"
        )
    endif()
endif()

# ============================================================================
# Install
# ============================================================================
install(TARGETS FSLint-lib FSLint-cli
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY lib/ DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# Install schema files
install(DIRECTORY ${CMAKE_BINARY_DIR}/schema/
    DESTINATION share/FSLint-cli/schema
)