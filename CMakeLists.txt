# Out-of-tree ZMK code for this keyboard.
zephyr_library()
# Behaviors only exist on the central; split peripherals have no keymap to link against.
if(NOT CONFIG_ZMK_SPLIT OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
  zephyr_library_sources(src/behaviors/behavior_auto_repeat.c)
endif()
zephyr_library_include_directories(${CMAKE_SOURCE_DIR}/include)
