from pathlib import Path
import os
os.chdir(Path(__file__).resolve().parent.parent)
build=Path('web/build.sh').read_text();start=build.index('\nemcc ')+1;end=build.index('\ncp web/index.html',start)
command=build[start:end].replace('-O2 -fwrapv','-O1 -g -fwrapv -fsanitize=address -sINITIAL_MEMORY=67108864 -sALLOW_MEMORY_GROWTH=1').replace('-o web/dist/fe8.js','-o work/web-asan/fe8.js')
Path('work/web-asan').mkdir(parents=True,exist_ok=True)
script='#!/usr/bin/env bash\nset -euo pipefail\n'+command+'\n'
for test in ['test','test_menu','test_save','test_save_menu','test_map','test_units','test_units_restore','test_events','test_dialogue','test_battle','test_tactical','test_attack','test_support','test_phase','test_ai_vm','test_ai_move','test_ai_danger','test_ai_simulation','test_ai_combat','test_ai_phase','test_recovery','test_death','test_audio','test_progress','test_background','test_chapter2','test_heal','test_drop','test_chapter2_end','test_chapter2_turns','test_trade','test_rescue','test_canto','test_transfer','test_loot','test_convoy','test_aid','test_rescue_death','test_chapter3','test_chapter3_variants','test_keys','test_obstacles','test_ai_chapter3','test_checkpoint','test_campaign','test_steal','test_supply','test_inventory','test_gameover','test_gameover_defeat','test_chapter4','test_magic','test_chapter4_variants','test_chapter5','test_shop','test_ai_chapter5','test_chapter5_variants','test_pillage_chapter5','test_chapter5x','test_ai_chapter5x','test_gameover_chapter5x','test_campaign5x']:script+='FE8_TEST_DIST="$PWD/work/web-asan" node web/'+test+'.cjs\n'
Path('work/check_menu_asan.sh').write_text(script)

