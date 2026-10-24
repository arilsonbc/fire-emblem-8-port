from pathlib import Path
import re,hashlib,json,subprocess
root=Path(__file__).resolve().parent.parent;rom=(root/'baserom.gba').read_bytes();assert hashlib.sha1(rom).hexdigest()=='c25b145e37456171ada4b0d440bf88a19f4d509f'
symbols={}
for line in subprocess.check_output(['arm-none-eabi-nm','-S','fireemblem8.elf'],text=True,cwd=root).splitlines():
 p=line.split()
 if len(p)==4:symbols[p[3]]=(int(p[0],16),int(p[1],16))
 elif len(p)==3 and p[2]=='gPal_MapSprite':symbols[p[2]]=(int(p[0],16),128)
 elif len(p)==3 and p[2]=='gGfx_MiscUiGraphics':symbols[p[2]]=(int(p[0],16),0)
def raw(name):a,n=symbols[name];return rom[a-0x8000000:a-0x8000000+n]
def lz(pointer):
 p=pointer-0x8000000;assert rom[p]==16;size=int.from_bytes(rom[p+1:p+4],'little');p+=4;out=bytearray()
 while len(out)<size:
  flags=rom[p];p+=1
  for b in range(7,-1,-1):
   if len(out)>=size:break
   if flags&(1<<b):
    a,c=rom[p:p+2];p+=2;d=((a&15)<<8|c)+1;assert d<=len(out)
    for _ in range((a>>4)+3):
     if len(out)<size:out.append(out[-d])
   else:out.append(rom[p]);p+=1
 return bytes(out)
def struct(file,name):return re.search(r'struct '+name+r'\s*\{.*?\}\s*(?:BITPACKED)?;', (root/file).read_text(),re.S).group()
def fn(file,name):
 s=(root/file).read_text();m=re.search(r'(?:inline )?(?:const struct \w+\*|struct \w+\*|s8|u8|void|int) '+name+r'\([^\n]*\)\s*\{',s);assert m,name
 p=m.end();depth=1
 while depth:
  if s[p]=='{':depth+=1
  if s[p]=='}':depth-=1
  p+=1
 return s[m.start():p].replace('inline ','')
types=['#ifndef FE8_UNIT_TYPES_H','#define FE8_UNIT_TYPES_H','#include "save_types.h"','#include "../palette.h"','#include "constants/classes.h"','enum {UNIT_ITEM_COUNT=5,UNIT_DEFINITION_ITEM_COUNT=4,UNIT_LEVEL_MAX=20,UNIT_EXP_DISABLED=255,UDEF_AIIDX_MAX=4,FACTION_BLUE=0,FACTION_GREEN=64,FACTION_RED=128};','struct Vec2 {short x,y;};']
for name in ['CharacterData','ClassData','Unit','UnitDefinition']:types.append(struct('include/bmunit.h',name))
types.append(struct('include/muctrl.h','REDA'))
types.append(struct('include/bmreliance.h','SupportData'))
header=(root/'include/bmunit.h').read_text();types+=re.findall(r'enum\s*\{[^}]*\b(?:US_NONE|CA_NONE)\b[^}]*\};',header,re.S)
types+=re.findall(r'^#define UNIT_(?!IS_GORGON|IS_PHANTOM)[^\n]*',header,re.M)
types.append(re.search(r'enum(?: \w+)?\s*\{[^}]*PACKED_US_DEAD[^}]*\};',(root/'include/bmsave.h').read_text(),re.S).group())
types+=['_Static_assert(sizeof(struct Unit)==72,"Native WASM unit ABI");','#endif'];(root/'web/generated/unit_types.h').write_text('\n'.join(types))
parts=[]
def field(name,value):return '.'+name+'='+str(value)
chars=raw('gCharacterData');classes=raw('gClassData');support=[]
for table,data,stride,fields in [('characterData',chars,52,[('nameTextId',0,2),('descTextId',2,2),('number',4,1),('defaultClass',5,1),('portraitId',6,2),('affinity',9,1)]+[(n,11+k,1) for k,n in enumerate(['baseLevel','baseHP','basePow','baseSkl','baseSpd','baseDef','baseRes','baseLck','baseCon'])]+[(n,28+k,1) for k,n in enumerate(['growthHP','growthPow','growthSkl','growthSpd','growthDef','growthRes','growthLck'])]+[('attributes',40,4)]),('classData',classes,84,[('nameTextId',0,2),('descTextId',2,2),('number',4,1),('promotion',5,1),('SMSId',6,1),('slowWalking',7,1),('classRelativePower',26,1)]+[(n,11+k,1) for k,n in enumerate(['baseHP','basePow','baseSkl','baseSpd','baseDef','baseRes','baseCon','baseMov','maxHP','maxPow','maxSkl','maxSpd','maxDef','maxRes','maxCon'])]+[(n,27+k,1) for k,n in enumerate(['growthHP','growthPow','growthSkl','growthSpd','growthDef','growthRes','growthLck'])]+[('attributes',40,4)])]:
 entries=[]
 for i in range(len(data)//stride):
  row=data[i*stride:(i+1)*stride];values=[field(n,int.from_bytes(row[o:o+l],'little',signed=n.startswith(('base','max')))) for n,o,l in fields]
  rankOff=20 if table=='characterData' else 44;values.append('.baseRanks={'+','.join(map(str,row[rankOff:rankOff+8]))+'}')
  if table=='characterData':
   ptr=int.from_bytes(row[44:48],'little')
   if ptr:
    dataSupport=rom[ptr-0x8000000:ptr-0x8000000+22];assert len(dataSupport)==22 and dataSupport[21]<=7
    name='supportData_'+str(i);parts.append('static const struct SupportData '+name+'={.characters={'+','.join(map(str,dataSupport[:7]))+'},.supportExpBase={'+','.join(map(str,dataSupport[7:14]))+'},.supportExpGrowth={'+','.join(map(str,dataSupport[14:21]))+'},.supportCount='+str(dataSupport[21])+'};');values.append('.pSupportData=&'+name)
  if table=='classData':
   pointers=[]
   for weather in range(3):
    ptr=int.from_bytes(row[56+weather*4:60+weather*4],'little');cost=rom[ptr-0x8000000:ptr-0x8000000+65] if ptr else bytes([255]*65);name='moveCosts_'+str(i)+'_'+str(weather);parts.append('static const s8 '+name+'[]={'+','.join(str(int.from_bytes(bytes([v]),'little',signed=True)) for v in cost)+'};');pointers.append(name)
   values.append('.pMovCostTable={'+','.join(pointers)+'}')
   for k,fieldName in enumerate(['pTerrainAvoidLookup','pTerrainDefenseLookup','pTerrainResistanceLookup']):
    ptr=int.from_bytes(row[68+k*4:72+k*4],'little');terrain=rom[ptr-0x8000000:ptr-0x8000000+65] if ptr else bytes(65);name='terrainBonus_'+str(i)+'_'+str(k);parts.append('static const s8 '+name+'[]={'+','.join(str(int.from_bytes(bytes([v]),'little',signed=True)) for v in terrain)+'};');values.append('.'+fieldName+'='+name)
  entries.append('{'+','.join(values)+'}')
 parts.append('static const struct '+('CharacterData' if table=='characterData' else 'ClassData')+' '+table+'[]={'+','.join(entries)+'};')
items=raw('gItemData');parts.append('static const struct {u8 uses;u32 attributes;} itemData[]={'+','.join('{'+str(items[i+20])+','+str(int.from_bytes(items[i+8:i+12],'little'))+'}' for i in range(0,len(items),36))+'};')
# Relocate original REDA and definition pointers into native constant data.
defTables=[];manifest=[]
chapter4Definitions=set(re.findall(r"UnitDef_\w+",(root/"src/events/ch4-eventscript.h").read_text()))|{"UnitDef_Event_Ch4Ally"}
chapter5Definitions=set(re.findall(r"UnitDef_\w+",(root/"src/events/ch5-eventscript.h").read_text()))|{"UnitDef_Event_Ch5Ally"}
chapter5xDefinitions=set(re.findall(r"UnitDef_\w+",(root/"src/events/ch5x-eventscript.h").read_text()))|{"UnitDef_Event_Ch5xAlly"}
chapter6Definitions=set(re.findall(r"UnitDef_\w+",(root/"src/events/ch6-eventscript.h").read_text()))|{"UnitDef_Event_Ch6Ally"}
for name,(address,size) in symbols.items():
 if not (name.startswith('UnitDef_Event_Prologue') or name in ['UnitDef_Event_Ch1Ally','UnitDef_Event_Ch1Enemy','UnitDef_Event_Ch1NPC','UnitDef_Event_Ch1AllyReinforce','UnitDef_Event_Ch1EnemyReinforce','UnitDef_088B43D0','UnitDef_Event_Ch2Ally','UnitDef_088B4344','UnitDef_088B440C','UnitDef_088B4434','UnitDef_088B4470','UnitDef_088B44AC','UnitDef_Event_Ch3Ally','UnitDef_088B463C','UnitDef_088B4718','UnitDef_088B476C','UnitDef_088B47E4'] or name in chapter4Definitions or name in chapter5Definitions or name in chapter5xDefinitions or name in chapter6Definitions):continue
 data=raw(name);entries=[]
 for i in range(0,len(data),20):
  r=data[i:i+20]
  if not r[0]:break
  pos=int.from_bytes(r[4:6],'little');count=r[7];ptr=int.from_bytes(r[8:12],'little');redas=[]
  for j in range(count):
   d=rom[ptr-0x8000000+j*8:ptr-0x8000000+(j+1)*8];v=int.from_bytes(d[:4],'little');redas.append('{.x='+str(v&63)+',.y='+str((v>>6)&63)+',.flags='+str((v>>12)&63)+',.a='+str(d[3])+',.b='+str(int.from_bytes(d[4:6],'little'))+',.delayFrames='+str(int.from_bytes(d[6:8],'little'))+'}')
  redaName='unit_reda_'+str(len(manifest))+'_'+str(i);parts.append('static const struct REDA '+redaName+'[]={'+','.join(redas or ['{0}'])+'};')
  values=dict(charIndex=r[0],classIndex=r[1],leaderCharIndex=r[2],autolevel=r[3]&1,allegiance=(r[3]>>1)&3,level=r[3]>>3,xPosition=pos&63,yPosition=(pos>>6)&63,genMonster=(pos>>12)&1,itemDrop=(pos>>13)&1,sumFlag=(pos>>14)&1,redaCount=count)
  entries.append('{'+','.join(field(k,v) for k,v in values.items())+',.redas='+redaName+',.items={'+','.join(map(str,r[12:16]))+'},.ai={'+','.join(map(str,r[16:20]))+'}}')
 parts.append('static const struct UnitDefinition '+name+'[]={'+','.join(entries+['{0}'])+'};');defTables.append('{'+str(address)+','+name+','+str(len(entries))+'}');manifest.append({'name':name,'address':address,'count':len(entries),'sha256':hashlib.sha256(data).hexdigest()})
parts.append('static const struct {u32 address;const struct UnitDefinition *data;int count;} unitDefinitions[]={'+','.join(defTables)+'};')
sprites=raw('unit_icon_wait_table');spriteEntries=[];seen={};spriteBytes={}
for i in range(0,len(sprites),8):
 r=sprites[i:i+8];size=int.from_bytes(r[2:4],'little');ptr=int.from_bytes(r[4:8],'little');assert size<3
 if ptr not in seen:
  data=lz(ptr);name='standingSprite_'+str(len(seen));parts.append('static const u8 '+name+'[]={'+','.join(map(str,data))+'};');seen[ptr]=name;spriteBytes[ptr]=len(data)
 spriteEntries.append('{'+str(size)+','+seen[ptr]+','+str(spriteBytes[ptr])+'}')
parts.append('static const struct {int size;const u8 *data;int bytes;} standingSprites[]={'+','.join(spriteEntries)+'};')
pal=raw('gPal_MapSprite');parts.append('static const u16 unitSpritePalettes[]={'+','.join(str(int.from_bytes(pal[i:i+2],'little')) for i in range(0,len(pal),2))+'};')
# Original LoadObjUIGfx copies 18 tiles per row; OAM rescue indicator uses tile 3.
ui=lz(symbols['gGfx_MiscUiGraphics'][0]);parts.append('static const u8 rescueIconTile[]={'+','.join(map(str,ui[3*32:4*32]))+'};')
(root/'web/generated/unit_assets.h').write_text('\n'.join(parts));(root/'web/generated/unit_assets.json').write_text(json.dumps(manifest,indent=2))
functions=[fn('src/bmunit.c',n) for n in ['ClearUnit','CopyUnit','GetFreeUnit','InitUnits','UnitAddItem','UnitClearInventory','UnitInitFromDefinition','UnitLoadStatsFromChracter','UnitCheckStatCaps','GetUnitFromCharId','SetUnitHp','UnitLoadSupports']]
functions.append(fn('src/muctrl.c','GenUnitDefinitionFinalPosition'))
functions.append(fn('src/bmsave.c','LoadSavedUnit'))
writer=fn('src/bmsave.c','WriteGameSavePackedUnit')
writer=writer.replace('struct GameSavePackedUnit unitp;','struct GameSavePackedUnit unitp = {0};').replace('struct Unit tmp_unit;','struct Unit tmp_unit = {0};')
writer=writer.replace('    unitp.pid = UNIT_CHAR_ID(unit);\n    unitp.jid = unit->pClassData->number;','    unitp.pid = 0;\n    unitp.jid = 0;')
writer=re.sub(r'#if BUGFIX.*?#endif','    else {unitp.pid = UNIT_CHAR_ID(unit);unitp.jid = unit->pClassData->number;}',writer,flags=re.S)
writer=writer.replace('        struct Unit tmp_unit = {0};','').replace('    if (NULL == unit->pCharacterData)', '    struct Unit tmp_unit = {0};\n    if (NULL == unit->pCharacterData)')
functions.append(writer)

functions += [fn('src/bmbattle.c',n) for n in ['GetStatIncrease','GetAutoleveledStatIncrease']]
functions.append(fn('src/bmdifficulty.c','GetCurrentPromotedLevelBonus'))
functions += [fn('src/bmunit.c',n) for n in ['UnitAutolevelCore','UnitAutolevel']]
(root/'web/generated/unit_routines.h').write_text('\n'.join(f[:f.index('{')].strip()+';' for f in functions)+'\n'+'\n'.join(functions))
print('Units: character/class/item tables, '+str(len(manifest))+' definition groups, '+str(len(seen))+' standing sprite sheets, '+str(len(functions))+' original routines')


