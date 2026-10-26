from pathlib import Path
import re,hashlib,subprocess,json
root=Path(__file__).resolve().parent.parent;rom=(root/'baserom.gba').read_bytes();assert hashlib.sha1(rom).hexdigest()=='c25b145e37456171ada4b0d440bf88a19f4d509f'
symbols={}
for line in subprocess.check_output(['arm-none-eabi-nm','-S','fireemblem8.elf'],cwd=root,text=True).splitlines():
 p=line.split()
 if len(p)==4:symbols[p[3]]=(int(p[0],16),int(p[1],16))
def raw(name,extra=0):a,n=symbols[name];return rom[a-0x8000000:a-0x8000000+n+extra]
def struct(file,name):return re.search(r'struct '+name+r'\s*\{.*?\};',(root/file).read_text(),re.S).group()
def fn(file,name):
 s=(root/file).read_text();m=re.search(r'(?:void|s8|u8|u32|int) '+name+r'\([^\n]*\)\s*\{',s);assert m,name;p=m.end();d=1
 while d:
  if s[p]=='{':d+=1
  if s[p]=='}':d-=1
  p+=1
 return s[m.start():p]
assets=[struct('include/cp_common.h','AiCombatSimulationSt'),struct('include/cp_data.h','AiCombatScoreCoefficients').replace('};','u8 gbaPadding[3];};'),struct('include/cp_data.h','AiHealThreshold'),struct('src/cp_battle.c','RangeScore')]
coeff=raw('gAiCombatScoreCoefficientTable',1);assert (len(coeff)-1)%20==0
assets+=['static const u8 combatCoefficientBytes[]={'+','.join(map(str,coeff))+'};','#define gAiCombatScoreCoefficientTable ((const struct AiCombatScoreCoefficients*)combatCoefficientBytes)','#define AI_COEFFICIENT_COUNT '+str((len(coeff)-1)//20)]
heal=raw('gAI3HealingThresholdTable');assets+=['static const struct AiHealThreshold gAI3HealingThresholdTable[]={'+','.join('{'+str(heal[i])+','+str(heal[i+1])+'}' for i in range(0,len(heal),4))+'};']
ptrs=raw('gAiClassRankLists');entries=[]
for i in range(0,len(ptrs),4):
 p=int.from_bytes(ptrs[i:i+4],'little')
 if not p:entries.append('NULL');break
 data=[]
 while True:
  v=rom[p-0x8000000];p+=1;data.append(v)
  if not v:break
 name='combatClassList'+str(i//4);assets+=['static const u8 '+name+'[]={'+','.join(map(str,data))+'};'];entries.append(name)
assets+=['static const u8 *gAiClassRankLists[]={'+','.join(entries)+'};']
r=raw('sFriendZoneRangeScoreLut');assets+=['static struct RangeScore sFriendZoneRangeScoreLut[]={'+','.join('{'+','.join(str(int.from_bytes(bytes([v]),'little',signed=True)) for v in r[i:i+3])+'}' for i in range(0,len(r),4))+'};']
r=raw('sRange3OffsetLut_');assets+=['static struct Vec2 sRange3OffsetLut_[]={'+','.join('{'+str(int.from_bytes(r[i:i+2],'little',signed=True))+','+str(int.from_bytes(r[i+2:i+4],'little',signed=True))+'}' for i in range(0,len(r),4))+'};']
names=['AiAttemptOffensiveAction','AiFillReversedAttackRangeMap','AiSimulateBestBattleAgainstTarget','AiGetCombatPositionScore','AiIsBadFight','AiSimulateBattleAgainstTargetAtPosition','AiGetDamageDealtCombatScoreComponent','AiGetOpponentLowHpScoreComponent','AiGetFriendZoneCombatScoreComponent','AiGetTargetClassCombatScoreComponent','AiGetTurnCombatScoreComponent','AiGetDamageTakenScoreComponent','AiGetDangerScoreComponent','AiGetLowHpScoreComponent','AiComputeCombatScore','AiGetInRangeCombatPositionScoreComponent','AiGetTerrainCombatPositionScoreComponent','AiGetFriendZoneCombatPositionScoreComponent']
functions=[fn('src/cp_battle.c',n) for n in names]+[fn('src/cp_utility.c',n) for n in ['AiGetClassRank','AiReachesByBirdsEyeDistance']]+[fn('src/cpextra_80407F0.c','AiUpdateNoMoveFlag'),fn('src/cp_0803E2F4.c','AiUpdateGetUnitIsHealing')]
for i,f in enumerate(functions):
 if f.startswith('s8 AiAttemptOffensiveAction'):
  start=f.index('    if (gActiveUnit->state & US_IN_BALLISTA)');end=f.index('    SetWorkingBmMap',start)
  f=f[:start]+'''    if(gActiveUnit->state&US_IN_BALLISTA || (UNIT_CATTRIBUTES(gActiveUnit)&CA_BALLISTAE && gPlaySt.chapterIndex>3 && gPlaySt.chapterIndex!=5 && gPlaySt.chapterIndex!=6 && gPlaySt.chapterIndex!=7)){vmError=7;return 0;}
    if((UNIT_CATTRIBUTES(gActiveUnit)&CA_STEAL)&&GetUnitItemCount(gActiveUnit)<5){GenerateUnitMovementMap(gActiveUnit);MarkMovementMapEdges();if(AiAttemptStealActionWithinMovement()==1)return 0;}
    if(gAiState.flags&AI_FLAG_STAY){BmMapFill(gBmMapMovement,255);gBmMapMovement[gActiveUnit->yPos][gActiveUnit->xPos]=0;}else GenerateUnitMovementMap(gActiveUnit);
''' +f[end:]
  start=f.index('try_ballist_combat:');end=f.index('    if ((finalResult.score',start);f=f[:start]+f[end:]
  f=f.replace('        AiSetDecision(finalResult.xMove','        combatLastScore=finalResult.score;\n        AiSetDecision(finalResult.xMove')
  f=f.replace('        if ((s8)finalResult.itemSlot != -1)\n            TryRemoveUnitFromBallista(gActiveUnit);','')
  f=f.replace('#if BUGFIX\n    return ret;\n#endif','    return gAiDecision.actionPerformed; // Original missing return made defined; caller uses decision.')
  f=f.replace('            if (!isEnemy(unit))','            if(unit->xPos<0||unit->yPos<0||unit->xPos>=gBmMapSize.x||unit->yPos>=gBmMapSize.y)continue;\n            if (!isEnemy(unit))')
 if f.startswith('s8 AiSimulateBattleAgainstTargetAtPosition'):
  start=f.index('    if (st->itemSlot');end=f.index('    if (!AiIsBadFight',start)
  f=f[:start]+'''    if(st->itemSlot==0xffff){vmError=7;return 0;}
    int result=fe8_battle_simulate(gActiveUnitId,st->targetId,st->xMove,st->yMove,st->itemSlot);if(result<0){phaseSimError=result;phaseSimTarget=st->targetId;phaseSimActor=gActiveUnitId;vmError=7;return 0;}
    fe8_battle_simulation_copy(&combatActor,&combatTarget);
''' +f[end:]
 if f.startswith('int AiGetFriendZoneCombatScoreComponent'):
  f=f.replace('        u8 uid = gBmMapUnit[y][x];','        u8 uid;').replace('        if (uid == 0)', '        uid=gBmMapUnit[y][x];\n        if (uid == 0)')
 if f.startswith('int AiGetFriendZoneCombatPositionScoreComponent'):
  f=f.replace('        if (gBmMapUnit[y + it->y]', '        if(x+it->x<0||y+it->y<0||x+it->x>=gBmMapSize.x||y+it->y>=gBmMapSize.y)continue;\n        if (gBmMapUnit[y + it->y]')
 if f.startswith('int AiGetTargetClassCombatScoreComponent'):f=f.replace('sCombatScoreCoefficients->classRankBonuses[rank]','((const u8*)sCombatScoreCoefficients)[8+rank]')
 functions[i]=f
(root/'web/generated/ai_combat_assets.h').write_text('\n'.join(assets))
(root/'web/generated/ai_combat_routines.h').write_text('\n'.join(f[:f.index('{')].strip()+';' for f in functions)+'\n'+'\n'.join(functions))
(root/'web/generated/ai_combat_assets.json').write_text(json.dumps({'routines':names+['AiGetClassRank','AiReachesByBirdsEyeDistance','AiUpdateNoMoveFlag','AiUpdateGetUnitIsHealing'],'coefficientRows':(len(coeff)-1)//20,'adaptations':['map edges guarded before access','bugged friend-zone offset retained','class rank 9 reads original GBA row padding retained as explicit native bytes','ballista/magic unsupported branches guarded; steal uses original item-priority routines','missing return defined from decision flag']},indent=2))
print('AI combat: 22 original routines, ROM coefficient/rank/offset/healing data')

