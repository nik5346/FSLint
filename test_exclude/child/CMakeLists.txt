project(Child)
add_library(child_lib STATIC empty.c)
install(TARGETS child_lib DESTINATION lib)
