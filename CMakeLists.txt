# Out-of-tree ZMK code for this keyboard.
zephyr_library()
zephyr_library_sources(src/behaviors/behavior_auto_repeat.c)
zephyr_library_include_directories(${CMAKE_SOURCE_DIR}/include)
