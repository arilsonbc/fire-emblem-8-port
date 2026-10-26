from pathlib import Path
import re,hashlib,subprocess
root=Path(__file__).resolve().parent.parent
rom=(root/'baserom.gba').read_bytes();assert hashlib.sha1(rom).hexdigest()=='c25b145e37456171ada4b0d440bf88a19f4d509f'
def function(file,name):
 s=(root/file).read_text();m=re.search(r'(?:void|bool|u32|u8|s8|int|struct Unit \*) '+name+r'\([^\n]*\)\s*\{',s);assert m,name;i=m.end();depth=1
 while depth:
  if s[i]=='{':depth+=1
  if s[i]=='}':depth-=1
  i+=1
 return s[m.start():i]
(root/'web/generated/progress_units.h').write_text(function('src/bmio.c','ChapterChangeUnitCleanup'))
(root/'web/generated/progress_events.h').write_text(function('src/eventscr.c','Event2A_MoveToChapter')+'\n'+function('src/eventscr.c','Event33_CheckUnitVarious'))
parts=[function('src/bmunit.c',n) for n in ['UnitAutolevelPenalty','UnitApplyBonusLevels']]
(root/'web/generated/progress_levels.h').write_text('\n'.join(parts))
text=subprocess.check_output(['arm-none-eabi-nm','-S','fireemblem8.elf'],cwd=root,text=True);a=int(re.search(r'^(\w+) \w+ . gChapterDataTable$',text,re.M)[1],16)-0x8000000
rows=[]
for i in range(79):
 r=rom[a+i*148:a+(i+1)*148];v=int.from_bytes(r[20:22],'little');rows.append('{'+','.join(map(str,[r[12],r[18],v&15,v>>4&15,v>>8&15]))+'}')
(root/'web/generated/progress_chapters.h').write_text('static const struct {int fog,weather,easy,hard,normal;} progressChapters[]={'+','.join(rows)+'};')
(root/'web/generated/progress_gold.h').write_text(function('src/bmgold.c','GetPartyGoldAmount').replace('GetPartyGoldAmount()', 'GetPartyGoldAmount(void)')+'\n'+function('src/bmgold.c','SetPartyGoldAmount'))
(root/'web/generated/progress_convoy.h').write_text(function('src/bmcontainer.c','GetConvoyItemCount')+'\n'+function('src/bmcontainer.c','AddItemToConvoy'))
print('Progress: original chapter cleanup, transition/check handlers and difficulty-level routines')

(root/'web/generated/progress_seize.h').write_text(function('src/bmdifficulty.c','CanUnitSeize')+'\n'+function('src/bmmenu.c','UnitActionMenu_CanSeize'))
(root/'web/generated/progress_faction.h').write_text(function('src/bmunit.c','UnitChangeFaction'))
(root/'web/generated/progress_visit.h').write_text(function('src/bmmenu.c','VisitCommandUsability'))
(root/'web/generated/progress_loca.h').write_text(function('src/eventinfo.c','EvCheck05_LOCA')+'\n'+function('src/eventinfo.c','EvCheck03_CHAR'))

(root/'web/generated/progress_icons.h').write_text(function('src/eventcall.c','UnsetEventId_0x84'))

# Keys: original event matching and item eligibility/consumption, native presentation.
parts=['struct EvCheck07 {u32 unk0;u16 givenItem,givenMoney;u8 x,y;u16 cmdId;};','struct EvCheck08 {u32 unk0,script,unk8;};']
parts += [function('src/eventinfo.c',n).replace('#if !NONMATCHING',' #if 0') for n in ['EvCheck07_CHES','EvCheck08_DOOR']]
(root/'web/generated/progress_keyevents.h').write_text('\n'.join(parts))
parts=[function('src/bmitem.c' if n=='GetUnitItemSlot' else 'src/bmunit.c',n) for n in ['GetUnitItemSlot','GetUnitKeyItemSlotForTerrain','UnitUpdateUsedItem']]
parts += [function('src/bmitemuse.c','CanUnitUseChestKeyItem')]
parts.append(function('src/bmusemind.c','ExecKeyItem').replace('gBattleTarget.statusOut = -1;','// Native key action has no BattleUnit presentation.'))
(root/'web/generated/progress_keys.h').write_text('\n'.join(f[:f.index('{')].strip()+';' for f in parts)+'\n'+'\n'.join(parts))

(root/'web/generated/progress_cutscene.h').write_text(function('src/bmunit.c','ClearCutsceneUnits'))
shrink=function('src/bmcontainer.c','ShrinkConvoyItemList').replace('u16 * buffer = (void*) gGenericBuffer;','u16 nativeBuffer[CONVOY_ITEM_COUNT+1]={0};\n    u16 * buffer = nativeBuffer; // Deterministic zero tail replaces shared scratch-buffer residue.')
(root/'web/generated/progress_convoy_remove.h').write_text(shrink+'\n'+function('src/bmcontainer.c','RemoveItemFromConvoy'))

(root/'web/generated/fog_routines.h').write_text(function('src/bmunit.c','GetUnitFogViewRange')+'\n'+function('src/bmidoten.c','MapAddInRange'))
