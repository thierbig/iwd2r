# The minimal PE32 that test_pe.cpp parses, written by make_test_pe.py.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/test_pe.bin"
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/make_test_pe.py" "${CMAKE_CURRENT_BINARY_DIR}/test_pe.bin"
    DEPENDS make_test_pe.py
    COMMENT "Writing test_pe.bin")
add_executable(iwd2r_tests silence.cpp test_marshal_ptr.cpp test_sanity.cpp test_win_types.cpp test_win_types_minmax.cpp test_cstring.cpp test_geometry.cpp test_list.cpp test_array_map.cpp test_sync.cpp test_file.cpp test_misc_mfc.cpp test_ini.cpp test_alias.cpp test_clock.cpp test_fs.cpp test_data_dir.cpp test_ddraw.cpp test_dsound.cpp test_window.cpp test_network.cpp test_gamespy.cpp test_version_affinity.cpp test_png.cpp test_script.cpp test_msvc_rand.cpp test_msvc_atol.cpp test_c2darray.cpp smoke_start_screen.cpp test_chitin_shutdown.cpp test_chitin_cmdline.cpp test_warp.cpp scenario_start_screen.cpp test_layout.cpp test_pe.cpp test_generators.cpp test_state_dump.cpp test_save_load.cpp test_render_api.cpp test_data_view.cpp data_view.cpp test_crash.cpp test_compose.cpp test_present.cpp test_projector.cpp test_rules.cpp test_cre.cpp test_vid.cpp test_gam.cpp test_dialog.cpp test_ai_script.cpp test_ai.cpp test_sprite_actions.cpp test_combat.cpp test_music.cpp test_move.cpp test_effects.cpp test_effects_iwd.cpp test_effects_save.cpp test_projectile.cpp test_visual_projectiles.cpp test_derived_spell.cpp test_rest_store.cpp test_save_screens.cpp test_character_screen.cpp test_action_bar.cpp test_spellbook_screen.cpp test_spell_list.cpp test_options_screen.cpp test_screens_items.cpp test_text_capture.cpp test_text_fonts.cpp test_text_overlay.cpp test_creature_render.cpp test_infinity.cpp test_ui_misc.cpp test_sp_host.cpp "${CMAKE_CURRENT_BINARY_DIR}/test_pe.bin" $<TARGET_OBJECTS:iwd2r_engine>)
target_include_directories(iwd2r_tests PRIVATE "${PROJECT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/compat")
# Engine headers (test_network.cpp) are not held to the strict warning level.
target_include_directories(iwd2r_tests SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/src" ${ZLIB_INCLUDE_DIRS})
target_link_libraries(iwd2r_tests PRIVATE iwd2r_compat iwd2r_platform ${ZLIB_LIBRARIES} iwd2r_oracle_pe Catch2::Catch2WithMain)
iwd2r_strict_warnings(iwd2r_tests)
add_dependencies(iwd2r_tests iwd2r)
target_compile_definitions(iwd2r_tests PRIVATE IWD2R_EXE="$<TARGET_FILE:iwd2r>"
    IWD2R_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    IWD2R_TEST_PE="${CMAKE_CURRENT_BINARY_DIR}/test_pe.bin"
    "IWD2R_VERSION_STRING=\"${IWD2R_VERSION_STRING}\"")
catch_discover_tests(iwd2r_tests PROPERTIES SKIP_REGULAR_EXPRESSION "\\[iwd2r-skip\\]" TIMEOUT 180)
