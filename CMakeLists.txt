zephyr_include_directories(include)

if(CONFIG_ZMK_SPACEMOUSE)
  target_sources(app PRIVATE src/sm_engine.c src/spacemouse.c)
  target_sources_ifdef(CONFIG_ZMK_USB app PRIVATE src/usb_hid.c)
  target_sources_ifdef(CONFIG_ZMK_BLE app PRIVATE src/hog.c)
  target_sources_ifdef(CONFIG_ZMK_INPUT_PROCESSOR_SPACEMOUSE app PRIVATE src/input_processor_spacemouse.c)
endif()
