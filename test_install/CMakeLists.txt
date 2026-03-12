cmake_minimum_required(VERSION 3.15)
project(test_install)
macro(install)
  message("Dummy install called")
endmacro()
install(FILES CMakeLists.txt DESTINATION dummy)

macro(install)
  _install(${ARGN})
endmacro()

install(FILES CMakeLists.txt DESTINATION real)
