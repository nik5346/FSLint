import re

with open('CMakeLists.txt', 'r') as f:
    content = f.read()

# 1. Remove macro(install) block
content = re.sub(r'# Dependencies \(libxml2, Catch2\) have issues.*?\nmacro\(install\)\s+endmacro\(\)', '', content, flags=re.DOTALL)

# 2. Add LIBXML2_WITH_ZLIB OFF and unified FetchContent for libxml2
content = content.replace('set(LIBXML2_WITH_THREADS ON CACHE BOOL \"\" FORCE)',
                          'set(LIBXML2_WITH_THREADS ON CACHE BOOL \"\" FORCE)\nset(LIBXML2_WITH_ZLIB OFF CACHE BOOL \"\" FORCE)')

# Replace the manual libxml2 population with a simpler MakeAvailable, protected later
content = re.sub(r'# We don\'t use FetchContent_MakeAvailable\(libxml2\).*?add_subdirectory.*?endif\(\)', '', content, flags=re.DOTALL)

# 3. Suppress install rules for all dependencies
content = content.replace('FetchContent_MakeAvailable(zlib)',
                          'set(CMAKE_SKIP_INSTALL_RULES ON)\nFetchContent_MakeAvailable(zlib)\nset(CMAKE_SKIP_INSTALL_RULES OFF)')

content = content.replace('FetchContent_MakeAvailable(picosha2 Catch2 pybind11)',
                          'set(PYBIND11_INSTALL OFF CACHE BOOL \"\" FORCE)\nset(CATCH_INSTALL_HELPERS OFF CACHE BOOL \"\" FORCE)\nset(CATCH_BUILD_TESTING OFF CACHE BOOL \"\" FORCE)\n\nset(CMAKE_SKIP_INSTALL_RULES ON)\nFetchContent_MakeAvailable(libxml2 picosha2 Catch2 pybind11)\nset(CMAKE_SKIP_INSTALL_RULES OFF)')

# 4. Update Python module properties
py_mod_old = """    # Set output name to just 'fslint' (the .so/.pyd will have the appropriate suffix)
    set_target_properties(fslint PROPERTIES OUTPUT_NAME "fslint")
endif()"""
py_mod_new = """    # Set output name to just 'fslint' (the .so/.pyd will have the appropriate suffix)
    set_target_properties(fslint PROPERTIES OUTPUT_NAME "fslint")

    # Hide all internal symbols to prevent conflicts and keep the wheel clean.
    # This is crucial for passing 'auditwheel repair' on Linux.
    set_target_properties(fslint PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
endif()"""
content = content.replace(py_mod_old, py_mod_new)

# 5. Update install section
install_old = """install(TARGETS FSLint-cli DESTINATION bin)
if(Python3_Interpreter_FOUND AND Python3_Development_FOUND)
    install(TARGETS fslint DESTINATION bin)
endif()
install(DIRECTORY standard DESTINATION bin)"""
install_new = """install(TARGETS FSLint-cli DESTINATION bin)
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
content = content.replace(install_old, install_new)

with open('CMakeLists.txt', 'w') as f:
    f.write(content)
