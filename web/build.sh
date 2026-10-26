#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
command -v emcc >/dev/null || { echo 'Emscripten não encontrado'; exit 1; }
mkdir -p web/dist
python3 web/prepare_input.py
python3 web/prepare_graphics.py
python3 web/prepare_title.py
python3 web/prepare_runtime.py
python3 web/prepare_menu.py
python3 web/prepare_save.py
python3 web/prepare_map.py
python3 web/prepare_units.py
python3 web/prepare_events.py
python3 web/prepare_chapter_triggers.py
python3 web/prepare_dialogue.py
python3 web/prepare_support.py
python3 web/prepare_battle.py
python3 web/prepare_tactical.py
python3 web/prepare_ai.py
python3 web/prepare_ai_vm.py
python3 web/prepare_ai_danger.py
python3 web/prepare_ai_combat.py
python3 web/prepare_recovery.py
python3 web/prepare_death.py
python3 web/prepare_audio.py
python3 web/prepare_progress.py
python3 web/prepare_background.py
python3 web/prepare_loot.py
python3 web/prepare_gameover.py
python3 web/prepare_shop.py
emcc src/rng.c src/proc.c web/bridge.c web/proc_bridge.c web/generated/title_logic.c web/graphics.c web/palette.c web/menu_video.c web/menu.c web/save.c web/map.c web/fog.c web/units.c web/events.c web/dialogue.c web/battle.c web/support.c web/tactical.c web/ai.c web/recovery.c web/death.c web/audio.c web/background.c web/gameover.c web/generated/fade.c web/generated/gameflow.c web/generated/menu_fades.c -include web/compat.h -Iinclude -Iweb \
  -O2 -fwrapv --no-entry -sMODULARIZE=1 -sEXPORT_NAME=createFE8Module \
  -sENVIRONMENT=web,node \
  -sEXPORTED_FUNCTIONS='["_fe8_fog_refresh","_fe8_fog_visible","_fe8_fog_enabled","_fe8_tactical_can_shop","_fe8_tactical_begin_shop","_fe8_tactical_shop_item","_fe8_tactical_shop_price","_fe8_tactical_shop_info","_fe8_tactical_shop_select","_fe8_tactical_shop_confirm","_fe8_gameover_confirm","_fe8_gameover_info","_fe8_gameover_pixels","_fe8_tactical_can_inventory","_fe8_tactical_begin_inventory","_fe8_tactical_inventory_select","_fe8_tactical_inventory_action","_fe8_tactical_inventory_discard","_fe8_tactical_inventory_info","_fe8_tactical_can_supply","_fe8_tactical_begin_supply","_fe8_tactical_supply_transfer","_fe8_tactical_supply_info","_fe8_item_stealable","_fe8_tactical_can_steal","_fe8_tactical_begin_steal","_fe8_tactical_steal_target","_fe8_tactical_steal_item","_fe8_tactical_steal_info","_fe8_battle_steal","_fe8_save_checkpoint_info","_fe8_save_checkpoint_commit","_fe8_map_obstacle_hp","_fe8_map_obstacle_max","_fe8_battle_obstacle_forecast","_fe8_battle_obstacle_attack","_fe8_tactical_can_chest","_fe8_tactical_chest","_fe8_tactical_can_door","_fe8_tactical_door","_fe8_tactical_can_take","_fe8_tactical_begin_take","_fe8_tactical_take","_fe8_tactical_can_give","_fe8_tactical_begin_give","_fe8_tactical_give","_fe8_tactical_can_rescue","_fe8_tactical_begin_rescue","_fe8_tactical_rescue","_fe8_tactical_can_drop","_fe8_tactical_begin_drop","_fe8_tactical_drop","_fe8_tactical_can_trade","_fe8_tactical_begin_trade","_fe8_tactical_trade_target","_fe8_tactical_trade_choose","_fe8_tactical_trade_swap","_fe8_tactical_trade_info","_fe8_event_item_info","_fe8_event_item_choose","_fe8_convoy_count","_fe8_convoy_item","_fe8_battle_can_heal","_fe8_battle_heal","_fe8_tactical_can_heal","_fe8_tactical_begin_heal","_fe8_tactical_heal","_fe8_tactical_can_item","_fe8_tactical_begin_item","_fe8_tactical_use_item","_fe8_tactical_can_visit","_fe8_tactical_visit","_fe8_tactical_can_talk","_fe8_tactical_begin_talk","_fe8_tactical_talk","_fe8_unit_change_faction","_fe8_background_set","_fe8_background_info","_fe8_background_render","_fe8_tactical_can_seize","_fe8_tactical_seize","_fe8_progress_info","_fe8_audio_map_song","_fe8_audio_count","_fe8_audio_read","_fe8_audio_pop","_fe8_audio_info","_InitRN","_NextRN","_NextRN_100","_Roll1RN","_Roll2RN","_SetLCGRNValue","_AdvanceGetLCGRNValue","_fe8_rng_selftest","_fe8_proc_selftest","_fe8_proc_demo_start","_fe8_proc_demo_frame","_fe8_input_reset","_fe8_input_frame","_fe8_input_held","_fe8_input_new","_fe8_input_repeat","_fe8_graphics_pixels","_fe8_title_pixels","_fe8_screen_pixels","_fe8_title_reset","_fe8_title_tick","_fe8_title_sound_request","_fe8_title_idle_frames","_fe8_title_pulse_color","_fe8_title_render","_fe8_game_reset","_fe8_game_tick","_fe8_game_state","_fe8_game_pixels","_fe8_game_volume_request","_fe8_game_proc_count","_fe8_palette_buffer","_fe8_fade_test_start","_fe8_fade_test_tick","_fe8_fade_active","_fe8_menu_selection","_fe8_menu_options","_fe8_menu_sound","_fe8_menu_request","_fe8_menu_difficulty","_fe8_menu_test_profile","_fe8_menu_save_error","_fe8_menu_operation","_fe8_menu_confirmation","_menu_video_errors","_menu_video_test_scene","_menu_video_pixels","_fe8_save_buffer","_fe8_save_import_buffer","_fe8_save_import","_fe8_save_revision","_fe8_save_global_valid","_fe8_save_valid","_fe8_save_create","_fe8_save_copy","_fe8_save_erase","_fe8_save_load","_fe8_save_chapter","_fe8_save_difficulty","_fe8_save_time","_fe8_save_last_slot","_fe8_save_errors","_fe8_save_import_warnings","_fe8_save_reset_empty","_fe8_save_loaded_playst","_fe8_save_suspend_slot","_fe8_map_start","_fe8_map_stop","_fe8_map_tick","_fe8_map_active","_fe8_map_pixels","_fe8_map_info","_fe8_map_terrain","_fe8_map_tile","_fe8_map_change","_fe8_map_change_at","_fe8_units_reset","_fe8_units_load","_fe8_units_restore","_fe8_units_count","_fe8_unit_at","_fe8_units_errors","_fe8_unit_info","_fe8_unit_move","_fe8_unit_move_index","_fe8_unit_move_queue_index","_fe8_units_tick","_fe8_units_busy","_fe8_units_render","_fe8_event_call","_fe8_event_flag","_fe8_event_phase","_fe8_event_post_action","_fe8_event_start","_fe8_event_reset","_fe8_event_busy","_fe8_event_info","_fe8_event_advance","_fe8_event_confirm","_fe8_event_preview","_fe8_chapter_start","_fe8_chapter_tick","_fe8_event_test_tick","_fe8_dialogue_info","_fe8_dialogue_test_render","_fe8_ai_script_table_probe","_fe8_ai_script_probe","_fe8_ai_script_info","_fe8_ai_offensive_probe","_fe8_ai_offensive_info","_fe8_ai_danger_probe","_fe8_ai_danger_at","_fe8_death_info","_fe8_recovery_phase","_fe8_recovery_info","_fe8_recovery_item","_fe8_unit_give","_fe8_ai_prepare","_fe8_ai_info","_fe8_support_turn","_fe8_support_info","_fe8_battle_simulate","_fe8_battle_simulation_info","_fe8_battle_attack","_fe8_battle_forecast","_fe8_battle_info","_fe8_battle_busy","_fe8_battle_start","_fe8_battle_set_script","_fe8_battle_reset","_fe8_tactical_next_target","_fe8_tactical_begin_attack","_fe8_tactical_attack","_fe8_tactical_forecast","_fe8_tactical_start","_fe8_tactical_select","_fe8_tactical_move","_fe8_tactical_cancel","_fe8_tactical_wait","_fe8_tactical_end_turn","_fe8_tactical_info","_fe8_tactical_cost"]' \
  -o web/dist/fe8.js
cp web/index.html web/dist/index.html
cp web/clock.js web/dist/clock.js
cp web/audio.js web/audio-engine.js web/audio-worklet.js web/dist/
cp web/storage.js web/dist/storage.js
cp web/storage-ui.js web/dist/storage-ui.js
cp web/event-texts.js web/dist/event-texts.js
cp web/chapter-ui.js web/dist/chapter-ui.js
node web/test.cjs
node web/test_menu.cjs
node web/test_save.cjs
node web/test_save_menu.cjs
node web/test_map.cjs
node web/test_units.cjs
node web/test_units_restore.cjs
node web/test_events.cjs
node web/test_dialogue.cjs
node web/test_battle.cjs
node web/test_tactical.cjs
node web/test_attack.cjs
node web/test_support.cjs
node web/test_phase.cjs
node web/test_ai_vm.cjs
node web/test_ai_move.cjs
node web/test_ai_danger.cjs
node web/test_ai_simulation.cjs
node web/test_ai_combat.cjs
node web/test_ai_phase.cjs
node web/test_recovery.cjs
node web/test_death.cjs
node web/test_audio.cjs
node web/test_progress.cjs
node web/test_background.cjs
node web/test_chapter2.cjs
node web/test_heal.cjs
node web/test_drop.cjs
node web/test_chapter2_end.cjs
node web/test_chapter2_turns.cjs
node web/test_trade.cjs
node web/test_rescue.cjs
node web/test_canto.cjs
node web/test_transfer.cjs
node web/test_loot.cjs
node web/test_convoy.cjs
node web/test_aid.cjs
node web/test_rescue_death.cjs
node web/test_chapter3.cjs
node web/test_chapter3_variants.cjs
node web/test_keys.cjs
node web/test_obstacles.cjs
node web/test_ai_chapter3.cjs
node web/test_checkpoint.cjs
node web/test_campaign.cjs
node web/test_steal.cjs
node web/test_supply.cjs
node web/test_inventory.cjs
node web/test_gameover.cjs
node web/test_gameover_defeat.cjs
node web/test_chapter4.cjs
node web/test_magic.cjs
node web/test_chapter4_variants.cjs
node web/test_chapter5.cjs
node web/test_shop.cjs
node web/test_ai_chapter5.cjs
node web/test_chapter5_variants.cjs
node web/test_pillage_chapter5.cjs

node web/test_chapter5x.cjs
node web/test_ai_chapter5x.cjs
node web/test_gameover_chapter5x.cjs

node web/test_campaign5x.cjs

echo 'WebAssembly compilado e testes aprovados. Saída: web/dist'
