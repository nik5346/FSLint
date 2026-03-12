import re

with open('CMakeLists.txt', 'r') as f:
    content = f.read()

# 1. Remove the macro(install) block
content = re.sub(r'# Dependencies \(libxml2, Catch2\) have issues.*?\nmacro\(install\)\s*endmacro\(\)', '', content, flags=re.DOTALL)

# 2. Remove CMAKE_INSTALL_PREFIX override
content = re.sub(r'# Use a separate install prefix for dependencies.*?\nset\(CMAKE_INSTALL_PREFIX \"${CMAKE_BINARY_DIR}/dependency_install\"\)', '', content, flags=re.DOTALL)

# 3. Suppress all CMAKE_SKIP_INSTALL_RULES blocks if they exist (clean slate)
content = re.sub(r'set\(CMAKE_SKIP_INSTALL_RULES (ON|OFF)\)', '', content)
content = re.sub(r'set\(CMAKE_SKIP_INSTALL_RULES_BACKUP.*?\)', '', content)

# 4. Use EXCLUDE_FROM_ALL for all dependencies
# zlib
content = content.replace('FetchContent_MakeAvailable(zlib)', """FetchContent_GetProperties(zlib)
if(NOT zlib_POPULATED)
    FetchContent_Populate(zlib)
    add_subdirectory(  EXCLUDE_FROM_ALL)
endif()""")

# libxml2
content = content.replace('set(LIBXML2_WITH_THREADS ON CACHE BOOL "" FORCE)', 'set(LIBXML2_WITH_THREADS ON CACHE BOOL "" FORCE)\nset(LIBXML2_WITH_ZLIB OFF CACHE BOOL "" FORCE)')
# Remove redundant libxml2 populates/MakeAvailable
content = re.sub(r'# We don\'t use FetchContent_MakeAvailable\(libxml2\).*?add_subdirectory.*?endif\(\)', '', content, flags=re.DOTALL)
content = content.replace('FetchContent_MakeAvailable(libxml2)', '')
libxml2_populate = """FetchContent_GetProperties(libxml2)
if(NOT libxml2_POPULATED)
    FetchContent_Populate(libxml2)
    add_subdirectory(  EXCLUDE_FROM_ALL)
endif()"""
content = content.replace('set(LIBXML2_WITH_ZLIB OFF CACHE BOOL "" FORCE)', 'set(LIBXML2_WITH_ZLIB OFF CACHE BOOL "" FORCE)\n' + libxml2_populate)

# PicoSHA2, Catch2, pybind11
content = content.replace('FetchContent_MakeAvailable(picosha2 Catch2 pybind11)', """FetchContent_GetProperties(picosha2)
if(NOT picosha2_POPULATED)
    FetchContent_Populate(picosha2)
endif()

FetchContent_GetProperties(Catch2)
if(NOT catch2_POPULATED)
    FetchContent_Populate(Catch2)
    add_subdirectory(  EXCLUDE_FROM_ALL)
endif()

FetchContent_GetProperties(pybind11)
if(NOT pybind11_POPULATED)
    FetchContent_Populate(pybind11)
    add_subdirectory(  EXCLUDE_FROM_ALL)
endif()""")

# 5. Fix Python module properties
if 'CXX_VISIBILITY_PRESET hidden' not in content:
    content = content.replace('set_target_properties(fslint PROPERTIES OUTPUT_NAME \"fslint\")', """set_target_properties(fslint PROPERTIES OUTPUT_NAME \"fslint\")
    set_target_properties(fslint PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )""")

# 6. Fix install section
install_old = """install(TARGETS FSLint-cli DESTINATION bin)
if(Python3_Interpreter_FOUND AND Python3_Development_FOUND)
    install(TARGETS fslint DESTINATION bin)
endif()
install(DIRECTORY standard DESTINATION bin)"""

install_new = """install(TARGETS FSLint-cli DESTINATION bin)
if(Python3_Interpreter_FOUND AND Python3_Development_FOUND)
    install(TARGETS fslint
            COMPONENT python_bindings
            LIBRARY DESTINATION .
            RUNTIME DESTINATION .
            ARCHIVE DESTINATION .)

    install(DIRECTORY standard
            COMPONENT python_bindings
            DESTINATION .)
endif()
install(DIRECTORY standard DESTINATION bin)"""

if 'COMPONENT python_bindings' not in content:
    content = content.replace(install_old, install_new)

# 7. Clean up double newlines and trailing spaces
content = re.sub(r'\n{3,}', '\n\n', content)

with open('CMakeLists.txt', 'w') as f:
    f.write(content)
