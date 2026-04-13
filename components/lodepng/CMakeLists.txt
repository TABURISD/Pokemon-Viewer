idf_component_register(SRCS "lodepng.cpp" "lodepng_alloc.cpp"
                       INCLUDE_DIRS ".")
target_compile_definitions(${COMPONENT_LIB} PRIVATE LODEPNG_NO_COMPILE_ALLOCATORS)
