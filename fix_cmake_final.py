import re
import os

with open('CMakeLists.txt', 'r') as f:
    content = f.read()

# 1. Remove the macro(install) block
content = re.sub(r'# Dependencies \(libxml2, Catch2\) have issues.*?\nmacro\(install\)\s*endmacro\(\)', '', content, flags=re.DOTALL)

# 2. Remove CMAKE_INSTALL_PREFIX override
content = re.sub(r'# Use a separate install prefix for dependencies.*?\nset\(CMAKE_INSTALL_PREFIX \"${CMAKE_BINARY_DIR}/dependency_install\"\)', '', content, flags=re.DOTALL)

# 3. Suppress all CMAKE_SKIP_INSTALL_RULES lines if they exist
content = re.sub(r'set\(CMAKE_SKIP_INSTALL_RULES.*?\)\n', '', content)

# 4. Define helper for EXCLUDE_FROM_ALL
def replace_make_available(name, has_cmake=True):
    global content
    if has_cmake:
        replacement = f"""FetchContent_GetProperties({name})
if(NOT {name}_POPULATED)
    FetchContent_Populate({name})
    add_subdirectory(${{{name}_SOURCE_DIR}} ${{{name}_BINARY_DIR}} EXCLUDE_FROM_ALL)
endif()"""
    else:
        replacement = f"""FetchContent_GetProperties({name})
if(NOT {name}_POPULATED)
    FetchContent_Populate({name})
endif()"""

    # This is a bit risky if multiple are in one call, but we have them mostly separate now or can fix it.
    content = content.replace(f'FetchContent_MakeAvailable({name})', replacement)

# Fix zlib
replace_make_available('zlib')

# Fix libxml2
# Ensure LIBXML2_WITH_THREADS is there as an anchor
libxml2_settings = """set(LIBXML2_WITH_THREADS ON CACHE BOOL "" FORCE)
set(LIBXML2_WITH_ZLIB OFF CACHE BOOL "" FORCE) # Avoid export conflicts with internal zlib
"""
content = content.replace('set(LIBXML2_WITH_THREADS ON CACHE BOOL "" FORCE)', libxml2_settings)

# Remove any existing libxml2 population
content = re.sub(r'FetchContent_GetProperties\(libxml2\).*?add_subdirectory.*?endif\(\)', '', content, flags=re.DOTALL)
content = content.replace('FetchContent_MakeAvailable(libxml2)', '')

# Insert libxml2 population after settings
libxml2_populate = """FetchContent_GetProperties(libxml2)
if(NOT libxml2_POPULATED)
    FetchContent_Populate(libxml2)
    add_subdirectory(  EXCLUDE_FROM_ALL)
endif()"""
content = content.replace('set(LIBXML2_WITH_ZLIB OFF CACHE BOOL "" FORCE) # Avoid export conflicts with internal zlib',
                          'set(LIBXML2_WITH_ZLIB OFF CACHE BOOL "" FORCE) # Avoid export conflicts with internal zlib\n' + libxml2_populate)

# Fix PicoSHA2, Catch2, pybind11
# First remove the combined call if it exists
content = content.replace('FetchContent_MakeAvailable(picosha2 Catch2 pybind11)', '')

# Add them individually
deps_block = """# PicoSHA2 is header-only
FetchContent_GetProperties(picosha2)
if(NOT picosha2_POPULATED)
    FetchContent_Populate(picosha2)
endif()

set(CATCH_INSTALL_HELPERS OFF CACHE BOOL "" FORCE)
set(CATCH_BUILD_TESTING OFF CACHE BOOL "" FORCE)
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
endif()"""

# Anchor for insertion
content = content.replace('FetchContent_Declare(\n  pybind11', deps_block + '\nFetchContent_Declare(\n  pybind11')
# Wait, I put it before the Declare. Let's fix that.
content = content.replace(deps_block + '\nFetchContent_Declare(\n  pybind11', '') # revert
content = re.sub(r'FetchContent_Declare\(\s*pybind11.*?v2\.12\.0\s*\)', r'\g<0>\n\n' + deps_block, content, flags=re.DOTALL)

# 5. Visibility
if 'CXX_VISIBILITY_PRESET hidden' not in content:
    content = content.replace('set_target_properties(fslint PROPERTIES OUTPUT_NAME "fslint")',
                              'set_target_properties(fslint PROPERTIES OUTPUT_NAME "fslint")\n    set_target_properties(fslint PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)')

# 6. Install section
# Ensure we don't have duplicate sections
content = re.sub(r'# ============================================================================\n# Install.*?install\(DIRECTORY standard DESTINATION bin\)', '', content, flags=re.DOTALL)

install_section = """# ============================================================================
# Install
# ============================================================================
install(TARGETS FSLint-cli DESTINATION bin)
if(Python3_Interpreter_FOUND AND Python3_Development_FOUND)
    # When building as a Python wheel, we want to install the module to the root
    # of the package. scikit-build-core handles the destination.
    install(TARGETS fslint
            COMPONENT python_bindings
            LIBRARY DESTINATION .
            RUNTIME DESTINATION .
            ARCHIVE DESTINATION .)

    # Include the 'standard' directory in the Python package for schema validation
    install(DIRECTORY standard
            COMPONENT python_bindings
            DESTINATION .)
endif()
install(DIRECTORY standard DESTINATION bin)"""
content += '\n' + install_section

# Cleanup redundant bits
content = content.replace('FetchContent_MakeAvailable(picosha2 Catch2 pybind11)', '')

with open('CMakeLists.txt', 'w') as f:
    f.write(content)
