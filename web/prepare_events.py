from pathlib import Path
import re,json,hashlib,subprocess
root=Path(__file__).resolve().parent.parent;rom=(root/'baserom.gba').read_bytes();assert hashlib.sha1(rom).hexdigest()=='c25b145e37456171ada4b0d440bf88a19f4d509f'
symbols={}
for line in subprocess.check_output(['arm-none-eabi-nm','-S','fireemblem8.elf'],text=True,cwd=root).splitlines():
 p=line.split()
 if len(p)==4:symbols[p[3]]=(int(p[0],16),int(p[1],16))
byAddress={a:n for n,(a,size) in symbols.items()};names={n for n in symbols if n.startswith(('EventScr_Prologue_','EventScr_Ch1_','EventScr_Ch1Tut_','EventScr_CastleFelia_','EventScr_Ch2','EventScr_Ch3','EventScr_Ch4','EventScr_Ch5_','EventScr_Ch5x_','EventScr_Ch6_'))};names.update(re.findall(r'EventScr_\w+', (root/'src/events/ch4-eventscript.h').read_text()));names.update(re.findall(r'EventScr_\w+', (root/'src/events/ch5-eventscript.h').read_text()));names.update(n for n in ['EventScr_08592114','EventScr_CallOnTutorialMode','EventScr_RemoveBGIfNeeded','EventScr_GameOver'] if n in symbols)
names.update(re.findall(r'EventScr_\w+', (root/'src/events/ch5x-eventscript.h').read_text()))
names.update(re.findall(r'EventScr_\w+', (root/'src/events/ch6-eventscript.h').read_text()))
# Resolve only known event-data addresses. Embedded pointers are keys, never executable addresses.
while True:
 old=len(names)
 for name in list(names):
  a,size=symbols[name];data=rom[a-0x8000000:a-0x8000000+size]
  for p in range(0,len(data)-3,2):
   key=int.from_bytes(data[p:p+4],'little');target=byAddress.get(key)
   if target and target.startswith(('EventScr_','Event_')):names.add(target)
 if len(names)==old:break
parts=[];manifest=[];entries=[]
for index,name in enumerate(sorted(names)):
 a,size=symbols[name];data=rom[a-0x8000000:a-0x8000000+size];assert size%2==0
 ident='event_script_'+str(index);parts.append('static const _Alignas(4) u16 '+ident+'[]={'+','.join(str(int.from_bytes(data[p:p+2],'little')) for p in range(0,size,2))+'};');entries.append('{'+str(a)+','+ident+','+str(size//2)+'}')
 manifest.append({'name':name,'address':a,'halfwords':size//2,'sha256':hashlib.sha256(data).hexdigest()})
parts.append('static const struct {u32 address;const u16 *data;int length;} eventScripts[]={'+','.join(entries)+'};')
for name in ['EventScr_Prologue_BeginningScene','EventScr_Prologue_GiveRapier','EventScr_Prologue_ONeillSpawn','EventScr_Prologue_OneEnemyLeft','EventScr_Prologue_ONeillAttack','EventScr_Prologue_EndingScene','EventScr_Ch1_BeginningScene','EventScr_Ch1_EndingScene','EventScr_CastleFelia_BeginningScene','EventScr_Ch2_BeginningScene','EventScr_Ch2_EndingScene','EventScr_Ch3_BeginningScene','EventScr_Ch3_EndingScene','EventScr_Ch4_BeginningScene','EventScr_Ch5_BeginningScene','EventScr_Ch5_EndingScene','EventScr_Ch5x_BeginningScene','EventScr_Ch5x_EndingScene','EventScr_Ch6_BeginningScene','EventScr_Ch6_EndingScene']:
 parts.append('#define '+name+'_ADDRESS '+str(symbols[name][0]))
for name in ['EventListScr_Prologue_Turn','EventListScr_Prologue_Misc','EventListScr_Ch1_Turn','EventListScr_Ch1_Misc','EventListScr_Ch1_Location','EventListScr_Ch1_Character','EventListScr_Ch2_Turn','EventListScr_Ch2_Misc','EventListScr_Ch2_Location','EventListScr_Ch2_Character','EventListScr_Ch3_Turn','EventListScr_Ch3_Misc','EventListScr_Ch3_Location','EventListScr_Ch3_Character','EventListScr_Ch4_Turn','EventListScr_Ch4_Misc','EventListScr_Ch4_Location','EventListScr_Ch4_Character','EventListScr_Ch5_Turn','EventListScr_Ch5_Misc','EventListScr_Ch5_Location','EventListScr_Ch5_Character','EventListScr_Ch5x_Turn','EventListScr_Ch5x_Misc','EventListScr_Ch5x_Location','EventListScr_Ch5x_Character','EventListScr_Ch6_Turn','EventListScr_Ch6_Misc','EventListScr_Ch6_Location','EventListScr_Ch6_Character']:
 a,size=symbols[name];data=rom[a-0x8000000:a-0x8000000+size];assert size%4==0;parts.append('static const u32 '+name+'[]={'+','.join(str(int.from_bytes(data[i:i+4],'little')) for i in range(0,size,4))+'};')
(root/'web/generated/event_assets.h').write_text('\n'.join(parts));(root/'web/generated/event_assets.json').write_text(json.dumps(manifest,indent=2))
s=(root/'include/event.h').read_text();types=['#ifndef FE8_EVENT_TYPES_H','#define FE8_EVENT_TYPES_H','#include "unit_types.h"','#include "menu_types.h"']
for name in ['EventExecType','EventStateFlags','EventCommandReturnCode','EventSlotIdx']:types.append(re.search(r'enum '+name+r'\s*\{.*?\};',s,re.S).group())
st=(root/'include/eventscript.h').read_text()
for name in ['event_cmd_idx','event_sub_cmd_idx']:types.append(re.search(r'enum '+name+r'\s*\{.*?\};',st,re.S).group())
types+=re.findall(r'^#define (?:EVT_SUB_CMD\w*|EVT_CMD\w*|EVENT_IS_SKIPPING)[^\n]*',s,re.M)
types+=['struct EventEngineProc {const u16 *pEventCurrent,*pEventIdk,*pEventStart;u16 evStateBits,evStallTimer;u8 execType,chapterIndex;};','struct EventQueue {const u16 *evt1,*evt2;};','#define TRUE 1','#define FALSE 0','#endif'];(root/'web/generated/event_types.h').write_text('\n'.join(types))
def fn(name):
 source=(root/'src/eventscr.c').read_text();m=re.search(r'u8 '+name+r'\([^\n]*\)\s*\{',source);assert m,name
 p=m.end();depth=1
 while depth:
  if source[p]=='{':depth+=1
  if source[p]=='}':depth-=1
  p+=1
 return source[m.start():p]
functions=[]
for name in ['Event00_NULL','Event01_End','Event02_EvBitAndIdMod','Event03_CheckEvBitOrId','Event04_CheckRandom','Event05_SetSlot','Event06_SlotOperation','Event07_SlotQueueOperations','Event08_Label','Event09_Goto','Event0A_Call','Event0C_Branch','Event0E_STAL','Event10_ModifyEvBit','Event11_SetIgnoredKeys']:
 f=fn(name)
 if name=='Event01_End':f=f.replace('(u16 *)EventScr_08592114','ResolveScript('+str(symbols['EventScr_08592114'][0])+')')
 if name=='Event09_Goto':
  start=f.index('    u16 arg =');f=f[:start]+'''    const u16 *label=FindLabel(proc->pEventIdk,(u16)EVT_CMD_ARGV(proc->pEventCurrent)[0]);
    if(!label)return EVC_ERROR;
    proc->pEventCurrent=label;return EVC_ADVANCE_CONTINUE;
}'''
 if name=='Event0A_Call':f=f.replace('    for (i = 7;','    const u16 *target=ResolveScript(dst);if(!target||gEventActiveQueue[7].evt1)return EVC_ERROR;\n    for (i = 7;').replace('(const u16 *)dst','target')
 functions.append(f)
(root/'web/generated/event_routines.h').write_text('\n'.join(f[:f.index('{')].strip()+';' for f in functions)+'\n'+'\n'.join(functions))
# Original Event32 definition construction; ROM character access is relocated
# through the validated native table. Loading/MU presentation crosses one boundary.
spawn=fn('Event32_SpawnSingleUnit')
spawn=spawn.replace('    unitDef.charIndex = pid;', '''    const struct CharacterData *character=GetCharacterData(pid);
    if(!character||!GetClassData(character->defaultClass)||x<0||y<0||x>63||y>63||(subcmd>2&&subcmd!=15))return EVC_ERROR;
    unitDef.charIndex = pid;''')
spawn=spawn.replace('gCharacterData[pid - 1].','character->')
spawn=spawn.replace('    LoadUnit_800F704(&unitDef, 0, 0, subcmd == 0xf);','    if(fe8_unit_spawn_definition(&unitDef,subcmd==0xf)<0)return EVC_ERROR;')
(root/'web/generated/event_spawn.h').write_text(spawn)
deathMessages=set()
a,n=symbols['gDefeatTalkList'];table=rom[a-0x8000000:a-0x8000000+n]
for j in range(0,n,12):
 row=table[j:j+12];pid=int.from_bytes(row[:2],'little')
 if pid==65535:break
 if pid in [1,2,3,4,5,6,7,8,9,10,12,13,15,16,17,19,20,32,66,106,70,71,72,73,74,104] and row[3] in [0,1,2,3,4,5,6,7,255]:
  msg=int.from_bytes(row[6:8],'little')
  if msg:deathMessages.add(msg)
texts=(root/'texts/texts.txt').read_text();messages={};pageCounts=[];pauseRows=[]
for m in re.finditer(r'^## MSG_([\dA-F]+)\n(.*?)(?=^## |\Z)',texts,re.M|re.S):
 ident=int(m.group(1),16)
 if not(0x900<=ident<=0x9f5 or ident in [0x657,0x012,0x211,0xBFF,0xC00,0xC01] or ident==0x664 or ident==0x662 or ident in deathMessages):continue
 original=m.group(2).strip();pages=[];stops=[]
 for page in re.split(r'\[A\]|(?=\[BreakTalk\])',original):
  if '[BreakTalk]' in page:stops.append(len(pages))
  page=page.replace('[LF]','\n');page=re.sub(r'\[[^]]*\]','',page);page=re.sub(r'\n{2,}','\n',page).strip()
  if page:pages.append(page)
 for stop in stops:pauseRows.append('{'+str(ident)+','+str(stop)+'}')
 messages[str(ident)]=pages;pageCounts.append('{'+str(ident)+','+str(len(pages))+'}')
(root/'web/event-texts.js').write_text('// Original English messages; control codes withheld from this temporary DOM renderer.\nglobalThis.FE8EventTexts='+json.dumps(messages,ensure_ascii=False)+';\n')
with (root/'web/generated/event_assets.h').open('a') as out:
 out.write('\nstatic const struct {int id,page;} eventTextPauses[]={'+','.join(pauseRows+['{-1,-1}'])+'};')
 out.write('\nstatic const struct {int id,count;} eventTextCounts[]={'+','.join(pageCounts)+'};')
print('Events: '+str(len(names))+' original scripts, 14 native original handlers, '+str(len(messages))+' original messages')


allMessages={int(m.group(1),16):re.sub(r'\[[^]]*\]','',m.group(2)).strip().replace('\n',' ') for m in re.finditer(r'^## MSG_([\dA-F]+)\n(.*?)(?=^## |\Z)',texts,re.M|re.S)}
characters=symbols['gCharacterData'];items=symbols['gItemData']
unitNames={};itemNames={}
for offset in range(0,characters[1],52):
 row=rom[characters[0]-0x8000000+offset:characters[0]-0x8000000+offset+52];unitNames[str(int.from_bytes(row[:2],'little'))]=allMessages.get(int.from_bytes(row[:2],'little'),'')
for offset in range(0,items[1],36):
 row=rom[items[0]-0x8000000+offset:items[0]-0x8000000+offset+36];itemNames[str(offset//36)]={'name':allMessages.get(int.from_bytes(row[:2],'little'),''),'uses':row[20]}
with (root/'web/event-texts.js').open('a') as out:out.write('globalThis.FE8UnitNames='+json.dumps(unitNames,ensure_ascii=False)+';\nglobalThis.FE8ItemNames='+json.dumps(itemNames,ensure_ascii=False)+';\n')

source=(root/'src/bmlib.c').read_text()
def fadeFn(name):
 m=re.search(r'(?:void|bool) '+name+r'\([^\n]*\)\s*\{',source);assert m,name
 pos=m.end();depth=1
 while depth:
  if source[pos]=='{':depth+=1
  if source[pos]=='}':depth-=1
  pos+=1
 return source[m.start():pos]
fadeParts=[]
for name in ['FadeToBlack_OnInit','FadeToCommon_OnLoop','FadeFromBlack_OnInit','FadeFromCommon_OnLoop','FadeToWhite_OnInit','FadeFromWhite_OnInit']:fadeParts.append(fadeFn(name))
for name in ['ProcScr_FadeToBlack','ProcScr_FadeFromBlack','ProcScr_FadeToWhite','ProcScr_FadeFromWhite']:fadeParts.append(re.search(r'struct ProcCmd CONST_DATA '+name+r'\[\] =.*?\};',source,re.S).group())
for name in ['StartLockingFadeToBlack','StartLockingFadeFromBlack','StartLockingFadeToWhite','StartLockingFadeFromWhite']:
 f=fadeFn(name).replace('parent);','EventFadeParent());');f=f.replace('    struct Proc * proc =','    (void)parent;\n    struct Proc * proc =');fadeParts.append(f)
fadeParts.append(fn('Event17_Fade'))
(root/'web/generated/event_fades.h').write_text('\n'.join(fadeParts))

# Known native ASMC and tactical menu override state; never call an arbitrary ROM pointer.
guide=[];base=symbols['gGuideTable'][0]-0x8000000
for i in range(256):
 row=rom[base+i*12:base+(i+1)*12];guide.append('{.title='+str(row[0])+',.displayFlag='+str(int.from_bytes(row[6:8],'little'))+'}')
 if row[0]==12:break
else:raise RuntimeError('Guide terminator missing')
source=(root/'src/bmguide.c').read_text();m=re.search(r'void BmGuideTextSetAllGreen\(void\)\s*\{.*?\n\}',source,re.S);assert m
parts=['#define BmGuideTextSetAllGreen_ADDRESS '+str(symbols['BmGuideTextSetAllGreen'][0]),'struct GuideEnt {u8 title;u16 displayFlag;};','static const struct GuideEnt gGuideTable[]={'+','.join(guide)+'};',m.group().replace('struct GuideEnt * it','const struct GuideEnt * it'),fn('Event3D_MenuOverride')]
(root/'web/generated/event_guide.h').write_text('\n'.join(parts))

with (root/'web/generated/event_assets.h').open('a') as out:out.write('\n#define UnitDef_Event_Ch4Ally_ADDRESS '+str(symbols['UnitDef_Event_Ch4Ally'][0])+'\n')

with (root/'web/generated/event_assets.h').open('a') as out:out.write('\n#define UnitDef_Event_Ch5Ally_ADDRESS '+str(symbols['UnitDef_Event_Ch5Ally'][0])+'\n')

with (root/'web/event-texts.js').open('a') as out:out.write('globalThis.FE8ChapterNumber=id=>id===6?"5":id===5?"5x":id>=7&&id<=9?String(id-1):String(id);\n')

# Original party separation callbacks, with only character constants relocated.
party=(root/'src/events/ch5x-eventscript.h').read_text();constants=(root/'include/constants/characters.h').read_text();callbacks=[]
for name in ['HandleCh5xUnits_Start','HandleCh5xUnits_End']:
 m=re.search(r'void '+name+r'\(void\)\s*\{',party);pos=m.end();depth=1
 while depth:
  if party[pos]=='{':depth+=1
  if party[pos]=='}':depth-=1
  pos+=1
 f=party[m.start():pos]
 for token in set(re.findall(r'CHARACTER_\w+',f)):
  value=re.search(r'\b'+token+r'\s*=\s*(0x[0-9A-Fa-f]+|[0-9]+)',constants)[1];f=f.replace(token,value)
 callbacks.extend(['#define '+name+'_ADDRESS '+str(symbols[name][0]),f])
(root/'web/generated/event_party.h').write_text('\n'.join(callbacks))

with (root/'web/generated/event_assets.h').open('a') as out:out.write('\n#define UnitDef_Event_Ch6Ally_ADDRESS '+str(symbols['UnitDef_Event_Ch6Ally'][0])+'\n')
