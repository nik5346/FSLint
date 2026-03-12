cmake_minimum_required(VERSION 3.15)
project(Parent)
add_subdirectory(child EXCLUDE_FROM_ALL)
