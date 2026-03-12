import re

with open('CMakeLists.txt', 'r') as f:
    content = f.read()

# 1. Remove the dummy install macro block
content = re.sub(r'# Dependencies \(libxml2, Catch2\) have issues.*?\nmacro\(install\)\s*endmacro\(\)', '', content, flags=re.DOTALL)

# 2. Remove CMAKE_INSTALL_PREFIX override
content = re.sub(r'# Use a separate install prefix for dependencies.*?\nset\(CMAKE_INSTALL_PREFIX \"${CMAKE_BINARY_DIR}/dependency_install\"\)', '', content, flags=re.DOTALL)

# 3. Add LIBXML2_WITH_ZLIB OFF
content = content.replace('set(LIBXML2_WITH_THREADS ON CACHE BOOL "" FORCE)',
                          'set(LIBXML2_WITH_THREADS ON CACHE BOOL "" FORCE)\nset(LIBXML2_WITH_ZLIB OFF CACHE BOOL "" FORCE)')

# 4. Use FetchContent_GetProperties/Populate/add_subdirectory(EXCLUDE_FROM_ALL) for dependencies
# This is the cleanest way to prevent dependency installation.

def make_excluded(name, has_cmake=True):
    global content
    if has_cmake:
        repl = f"""FetchContent_GetProperties({name})
if(NOT {name}_POPULATED)
    FetchContent_Populate({name})
    add_subdirectory(${{{name}_SOURCE_DIR}} ${{{name}_BINARY_DIR}} EXCLUDE_FROM_ALL)
endif()"""
    else:
        repl = f"""FetchContent_GetProperties({name})
if(NOT {name}_POPULATED)
    FetchContent_Populate({name})
endif()"""

    # Try to find current usage and replace it
    # For libxml2, it has a special block already
    if name == 'libxml2':
        content = re.sub(r'# We don\'t use FetchContent_MakeAvailable\(libxml2\).*?add_subdirectory.*?endif\(\)', repl, content, flags=re.DOTALL)
    else:
        content = content.replace(f'FetchContent_MakeAvailable({name})', repl)

make_excluded('zlib')
make_excluded('libxml2')

# Handle the group
content = content.replace('FetchContent_MakeAvailable(picosha2 Catch2 pybind11)', '')
content = re.sub(r'FetchContent_Declare\(\s*pybind11.*?v2\.12\.0\s*\)', r'\g<0>\n\n' +
                 \"\"\"FetchContent_GetProperties(picosha2)
if(NOT picosha2_POPULATED)
    FetchContent_Populate(picosha2)
endif()

set(CATCH_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(CATCH_INSTALL_HELPERS OFF CACHE BOOL "" FORCE)
FetchContent_GetProperties(Catch2)
if(NOT catch2_POPULATED)
    FetchContent_Populate(Catch2)
    add_subdirectory(  EXCLUDE_FROM_ALL)
endif()

set(PYBIND11_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_GetProperties(pybind11)
if(NOT pybind11_POPULATED)
    FetchContent_Populate(pybind11)
    add_subdirectory(  EXCLUDE_FROM_ALL)
endif()\"\"\", content, flags=re.DOTALL)

# 5. Visibility for Python module
content = content.replace('set_target_properties(fslint PROPERTIES OUTPUT_NAME "fslint")',
                          'set_target_properties(fslint PROPERTIES OUTPUT_NAME "fslint")\n    set_target_properties(fslint PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)')

# 6. Proper install rules
install_old = \"\"\"install(TARGETS FSLint-cli DESTINATION bin)
if(Python3_Interpreter_FOUND AND Python3_Development_FOUND)
    install(TARGETS fslint DESTINATION bin)
endif()
install(DIRECTORY standard DESTINATION bin)\"\"\"

install_new = \"\"\"install(TARGETS FSLint-cli DESTINATION bin)
install(DIRECTORY standard DESTINATION bin)

if(Python3_Interpreter_FOUND AND Python3_Development_FOUND)
    install(TARGETS fslint
            COMPONENT python_bindings
            LIBRARY DESTINATION .
            RUNTIME DESTINATION .
            ARCHIVE DESTINATION .)

    install(DIRECTORY standard
            COMPONENT python_bindings
            DESTINATION .)
endif()\"\"\"
content = content.replace(install_old, install_new)

with open('CMakeLists.txt', 'w') as f:
    f.write(content)
