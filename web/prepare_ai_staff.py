from pathlib import Path
import re
root=Path(__file__).resolve().parent.parent
def function(file,name):
 s=(root/file).read_text();m=re.search(r'(?:s8|int|void) '+name+r'\([^\n]*\)\s*\{',s);assert m,name;p=m.end();d=1
 while d:
  if s[p]=='{':d+=1
  if s[p]=='}':d-=1
  p+=1
 return s[m.start():p]
parts=['struct AiStaffLutEntry {u16 itemId;void(*func)(int,s8(*)(struct Unit*));};','struct AdjPosLutEnt {int x,y;};','static const struct AdjPosLutEnt sAiAdjacentPositionLut[]={{1,0},{-1,0},{0,1},{0,-1}};','void AiStaffHealMendRecover(int itemIdx,s8(*isEnemy)(struct Unit*));','static const struct AiStaffLutEntry sAiStaffFuncLut[]={{ITEM_STAFF_HEAL,AiStaffHealMendRecover},{ITEM_STAFF_MEND,AiStaffHealMendRecover},{ITEM_STAFF_RECOVER,AiStaffHealMendRecover},{0,0}};']
functions=[function('src/cp_utility.c','sub_803C490')]
for name in ['GetAiStaffFuncIndex','AiTryDoStaff','GetAiSafestAccessibleAdjacentPosition','AiStaffHealMendRecover']:
 f=function('src/cp_staff.c',name)
 if name=='GetAiSafestAccessibleAdjacentPosition':f=f.replace('        if (gBmMapMovement[iy][ix]', '        if(ix<0||iy<0||ix>=gBmMapSize.x||iy>=gBmMapSize.y)continue;\n        if (gBmMapMovement[iy][ix]')
 functions.append(f)
parts.extend(f[:f.index('{')].strip()+';' for f in functions);parts.extend(functions)
(root/'web/generated/ai_staff_routines.h').write_text('\n'.join(parts));print('AI staff: original Heal/Mend/Recover dispatcher, target percentage and safe adjacent-position routines; other staves guarded')
