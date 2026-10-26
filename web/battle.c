#include "battle.h"
#include "fog.h"
#include "support.h"
#include "death.h"
#include "events.h"
#include "tactical.h"
#include "proc.h"
#include "generated/battle_types.h"
#include "units.h"
#include "save.h"
#include "map.h"
#include "rng.h"
#include <string.h>
#include <stdlib.h>
#define gPlaySt (*fe8_save_loaded_playst())
#define UNIT_IS_GORGON_EGG(u) ((u)->pClassData->number==CLASS_GORGONEGG||(u)->pClassData->number==CLASS_GORGONEGG2)
#include "generated/battle_assets.h"
struct BattleStats gBattleStats;
struct BattleUnit gBattleActor,gBattleTarget;
struct BattleHit gBattleHitArray[BATTLE_HIT_MAX],*gBattleHitIterator;
static struct {struct BattleHit *scriptedBattleHits;u8 subjectIndex,targetIndex,itemSlotIndex;} gActionData;
static struct {u16 um_tmp_item;int gameStateBits;} gBmSt;
#define ABS(v) abs(v)
#define CHAPTER_E_21 21
#define CHAPTER_I_21 34
extern int GetStatIncrease(int growth);
extern const struct ClassData *GetClassData(int id);
extern struct Unit *GetUnit(int index);
extern void UnitCheckStatCaps(struct Unit *u);
static int gUnknown_03003060;
static int expGained[256];
static void PidStatsAddExpGained(int pid,int amount){expGained[pid&255]+=amount;}
// Triangle-attack check is now extracted from the source; adjacent map access is bounded.
static struct {u16 playerWeapon,opponentWeapon;} gArenaState;
static int IsUnitMagicSealed(struct Unit *u){return u->statusIndex==UNIT_STATUS_SILENCED;} // Magic-seal aura remains outside integrated chapters.
static int IsItemUnsealedForUnit(struct Unit *u,int item){(void)u;(void)item;return 0;} // Unusable locks are refused.
static int GetBallistaItemAt(int x,int y){(void)x;(void)y;return 0;} // Ballista/arena paths are refused.

static void Terrain(struct BattleUnit *bu);
#define RECT_DISTANCE(x1,y1,x2,y2) (abs((x1)-(x2))+abs((y1)-(y2)))
struct CombatActionProc {int unitIdA,unitIdB;};
extern int GetUnitItemCount(struct Unit *u);
static void NewPopup_ItemGot(ProcPtr parent,struct Unit *u,int item){(void)parent;fe8_event_reward((u8)u->index,item,0);}
static void NewPopup_GoldGot(ProcPtr parent,struct Unit *u,int amount){(void)parent;fe8_event_reward((u8)u->index,amount,1);}
#include "generated/battle_routines.h"
#include "generated/battle_obstacle.h"
#define TERRAIN_PLAINS 1
extern u32 GetPartyGoldAmount(void);
extern void SetPartyGoldAmount(u32 amount);
extern int UnitAddItem(struct Unit *u,int item);
#include "generated/battle_steal.h"
#include "constants/characters.h"
static struct BattleHit script[BATTLE_HIT_MAX];
static int normalBattle,obstacleX,obstacleY;
static int simulationValid,simulationCount;
static int scriptReady,active,timer,roundIndex,roundCount,actorIndex,targetIndex,error,completed;
static int initialActorHP,initialTargetHP,displayActorHP,displayTargetHP;
extern struct Unit *GetUnit(int index);
static int Decode(const u32 *queue,int length,struct BattleHit *out){
 if(!queue||length<1||length>64)return -1;
 int n=0;memset(out,0,BATTLE_HIT_MAX*sizeof(*out));
 for(int i=0;i<length;i++){
  u32 value=queue[i];if((value&255)==255){out[n].info=BATTLE_HIT_INFO_END;return n;}
  if(n>=BATTLE_HIT_MAX-1||(value&255)>1)return -2;
  out[n].attributes=value>>16;out[n].info=(n?0:BATTLE_HIT_INFO_BEGIN)|((value&255)==1?BATTLE_HIT_INFO_RETALIATION:0);out[n].hpChange=(s8)(value>>8);n++;
 }
 return -3; // Require an explicit terminator before running source loops.
}
void fe8_battle_reset(void){simulationValid=simulationCount=0;memset(&gBattleStats,0,sizeof(gBattleStats));memset(&gBattleActor,0,sizeof(gBattleActor));memset(&gBattleTarget,0,sizeof(gBattleTarget));ClearBattleHits();memset(script,0,sizeof(script));gActionData.scriptedBattleHits=NULL;normalBattle=obstacleX=obstacleY=0;initialActorHP=initialTargetHP=displayActorHP=displayTargetHP=0;scriptReady=active=timer=roundIndex=roundCount=completed=0;error=0;actorIndex=targetIndex=0;}
int fe8_battle_set_script(const u32 *queue,int length){struct BattleHit temporary[BATTLE_HIT_MAX];int n=Decode(queue,length,temporary);if(n<0)return n;memcpy(script,temporary,sizeof(script));scriptReady=1;return n;}
static int Supported(struct Unit *u){
 if(!u||!UNIT_IS_VALID(u)||u->curHP<=0||u->xPos<0||u->yPos<0||u->state&(US_DEAD|US_IN_BALLISTA))return 0;

 for(int i=0;i<5;i++)if((u->items[i]&255)>=206||(GetItemAttributes(u->items[i])&IA_UNUSABLE))return 0;
 int terrain=fe8_map_terrain(u->xPos,u->yPos);return terrain>=0&&terrain<65;
}
static void Terrain(struct BattleUnit *bu){int t=fe8_map_terrain(bu->unit.xPos,bu->unit.yPos);bu->terrainId=t;bu->terrainAvoid=bu->unit.pClassData->pTerrainAvoidLookup[t];bu->terrainDefense=bu->unit.pClassData->pTerrainDefenseLookup[t];bu->terrainResistance=bu->unit.pClassData->pTerrainResistanceLookup[t];}
int fe8_battle_start(int actor,int target,int item,int ballista,const u32 *queue,int length){
 if(active)return -1;normalBattle=0;struct Unit *a=fe8_unit_by_pid(actor),*b=fe8_unit_by_pid(target);
 if(!Supported(a)||!Supported(b)||a==b||ballista||(item&&(a->items[0]&255)!=item))return -2;
 struct BattleHit hits[BATTLE_HIT_MAX];
 if(length){int n=Decode(queue,length,hits);if(n<0)return n;}else if(scriptReady)memcpy(hits,script,sizeof(hits));else return -4;
 memset(&gBattleActor,0,sizeof(gBattleActor));memset(&gBattleTarget,0,sizeof(gBattleTarget));memset(&gBattleStats,0,sizeof(gBattleStats));
 InitBattleUnit(&gBattleActor,a);InitBattleUnit(&gBattleTarget,b);gBattleStats.config=BATTLE_CONFIG_REAL|BATTLE_CONFIG_MAPANIMS;gBattleStats.range=abs(a->xPos-b->xPos)+abs(a->yPos-b->yPos);
 SetBattleUnitWeapon(&gBattleActor,-1);SetBattleUnitWeapon(&gBattleTarget,-1);BattleInitTargetCanCounter();BattleApplyWeaponTriangleEffect(&gBattleActor,&gBattleTarget);Terrain(&gBattleActor);Terrain(&gBattleTarget);
 ComputeBattleUnitStats(&gBattleActor,&gBattleTarget);ComputeBattleUnitStats(&gBattleTarget,&gBattleActor);ComputeBattleUnitEffectiveStats(&gBattleActor,&gBattleTarget);ComputeBattleUnitEffectiveStats(&gBattleTarget,&gBattleActor);
 ClearBattleHits();gActionData.scriptedBattleHits=hits;BattleUnwindScripted();scriptReady=0;
 // Original event combat disables EXP and reloads HP only after presentation.
 gBattleActor.expGain=gBattleTarget.expGain=0;actorIndex=(u8)a->index;targetIndex=(u8)b->index;initialActorHP=displayActorHP=a->curHP;initialTargetHP=displayTargetHP=b->curHP;
 roundCount=0;while(roundCount<BATTLE_HIT_MAX&&!(gBattleHitArray[roundCount].info&BATTLE_HIT_INFO_END))roundCount++;
 if(roundCount>=BATTLE_HIT_MAX){error=-5;return error;}roundIndex=timer=0;active=1;completed=0;return roundCount;
}
static int PairAt(int ai,int bi,int slot,int x,int y){
 if(active||ai<1||ai>255||bi<1||bi>255||ai==bi)return -1;
 struct Unit *a=GetUnit(ai),*b=GetUnit(bi);
 if(!Supported(a)||!Supported(b)||b->state&(US_HIDDEN|US_NOT_DEPLOYED|US_RESCUED)||a->state&(US_NOT_DEPLOYED|US_RESCUED))return -2;
 if(((UNIT_FACTION(a)^UNIT_FACTION(b))&128)==0)return -3;
 if(UNIT_FACTION(a)==FACTION_BLUE&&!fe8_fog_visible(b->xPos,b->yPos))return -3;
 // Triangle-attack eligibility is evaluated by the original routine during real/simulated hits.
 if(a->statusIndex==UNIT_STATUS_SLEEP||a->statusIndex==UNIT_STATUS_PETRIFY||a->statusIndex==UNIT_STATUS_13)return -4;
 for(int i=0;i<5;i++)if((GetItemAttributes(a->items[i])|GetItemAttributes(b->items[i]))&IA_MAGICDAMAGE)return -4;
 // Stat-bonus equipment requires equip-slot migration before taking the source snapshots.
 for(int i=0;i<5;i++)if(((GetItemAttributes(a->items[i])&IA_WEAPON)&&GetItemStatBonuses(a->items[i]))||((GetItemAttributes(b->items[i])&IA_WEAPON)&&GetItemStatBonuses(b->items[i])))return -4;
 if(a->pClassData->classRelativePower<=0||b->pClassData->classRelativePower<=0)return -4;
 if(x<0||y<0||x>=fe8_map_info(1)||y>=fe8_map_info(2)||fe8_map_terrain(x,y)<0)return -5;
 int range=abs(x-b->xPos)+abs(y-b->yPos);
 if(slot==-1){for(slot=0;slot<5;slot++)if(CanUnitUseWeaponNow(a,a->items[slot])&&IsItemCoveringRange(a->items[slot],range))break;}
 if(slot<0||slot>=5||!CanUnitUseWeaponNow(a,a->items[slot])||!IsItemCoveringRange(a->items[slot],range))return -5;
 return slot;
}
static int NormalPair(int ai,int bi,int slot){if(ai<1||ai>255)return -1;struct Unit *a=GetUnit(ai);return PairAt(ai,bi,slot,a->xPos,a->yPos);}
static struct BattleUnit simulationActor,simulationTarget;
static struct BattleHit simulationHits[BATTLE_HIT_MAX];
int fe8_battle_simulate(int ai,int bi,int x,int y,int slot){
 simulationValid=0;if(ai<1||ai>255)return -1;
 if(x==-1&&y==-1){x=GetUnit(ai)->xPos;y=GetUnit(ai)->yPos;}
 int chosen=PairAt(ai,bi,slot,x,y);if(chosen<0)return chosen;
 struct BattleStats stats=gBattleStats;struct BattleUnit a=gBattleActor,b=gBattleTarget;
 struct BattleHit hits[BATTLE_HIT_MAX],*iterator=gBattleHitIterator;memcpy(hits,gBattleHitArray,sizeof(hits));int oldUnknown=gUnknown_03003060;
 memset(&gBattleStats,0,sizeof(gBattleStats));memset(&gBattleActor,0,sizeof(gBattleActor));memset(&gBattleTarget,0,sizeof(gBattleTarget));ClearBattleHits();
 BattleGenerateSimulation(GetUnit(ai),GetUnit(bi),x,y,chosen);
 simulationActor=gBattleActor;simulationTarget=gBattleTarget;memcpy(simulationHits,gBattleHitArray,sizeof(simulationHits));
 simulationCount=0;while(simulationCount<BATTLE_HIT_MAX&&!(simulationHits[simulationCount].info&BATTLE_HIT_INFO_END))simulationCount++;
 gBattleStats=stats;gBattleActor=a;gBattleTarget=b;memcpy(gBattleHitArray,hits,sizeof(hits));gBattleHitIterator=iterator;gUnknown_03003060=oldUnknown;
 if(simulationCount>=BATTLE_HIT_MAX)return -6;simulationValid=1;return simulationCount;
}
int fe8_battle_simulation_info(int field){
 if(!simulationValid)return -1;struct BattleUnit *bu=field>=10&&field<20?&simulationTarget:&simulationActor;
 if(field<20)switch(field%10){case 0:return bu->unit.curHP;case 1:return bu->battleAttack;case 2:return bu->battleDefense;case 3:return bu->battleEffectiveHitRate;case 4:return bu->weapon;case 5:return bu->battleSpeed;case 6:return bu->unit.xPos;case 7:return bu->unit.yPos;case 8:return bu->terrainId;case 9:return bu->canCounter;}
 if(field==20)return simulationCount;if(field>=30&&field<30+BATTLE_HIT_MAX)return simulationHits[field-30].hpChange;if(field>=40&&field<40+BATTLE_HIT_MAX)return simulationHits[field-40].attributes;if(field>=50&&field<50+BATTLE_HIT_MAX)return simulationHits[field-50].info;return -1;
}
static void SetupNormal(int ai,int bi,int slot){
 struct Unit *a=GetUnit(ai),*b=GetUnit(bi);
 memset(&gBattleActor,0,sizeof(gBattleActor));memset(&gBattleTarget,0,sizeof(gBattleTarget));memset(&gBattleStats,0,sizeof(gBattleStats));
 InitBattleUnit(&gBattleActor,a);InitBattleUnit(&gBattleTarget,b);
 gBattleStats.config=BATTLE_CONFIG_REAL|BATTLE_CONFIG_MAPANIMS;gBattleStats.range=abs(a->xPos-b->xPos)+abs(a->yPos-b->yPos);
 SetBattleUnitWeapon(&gBattleActor,slot);SetBattleUnitWeapon(&gBattleTarget,-1);BattleInitTargetCanCounter();BattleApplyWeaponTriangleEffect(&gBattleActor,&gBattleTarget);Terrain(&gBattleActor);Terrain(&gBattleTarget);
 ComputeBattleUnitStats(&gBattleActor,&gBattleTarget);ComputeBattleUnitStats(&gBattleTarget,&gBattleActor);ComputeBattleUnitEffectiveStats(&gBattleActor,&gBattleTarget);ComputeBattleUnitEffectiveStats(&gBattleTarget,&gBattleActor);
}
int fe8_battle_forecast(int ai,int bi,int slot,int field){
 int chosen=NormalPair(ai,bi,slot);if(chosen<0)return chosen;SetupNormal(ai,bi,chosen);
 struct BattleUnit *bu=field>=10?&gBattleTarget:&gBattleActor,*other=field>=10?&gBattleActor:&gBattleTarget;
 switch(field%10){case 0:return bu->canCounter?((bu->battleAttack>other->battleDefense)?bu->battleAttack-other->battleDefense:0):-1;case 1:return bu->canCounter?bu->battleEffectiveHitRate:-1;case 2:return bu->canCounter?bu->battleEffectiveCritRate:-1;case 3:return bu->canCounter?(bu->battleSpeed-other->battleSpeed>=4?2:1):0;case 4:return bu->weaponSlotIndex;default:return -1;}
}
int fe8_battle_attack(int ai,int bi,int slot){
 int chosen=NormalPair(ai,bi,slot);if(chosen<0)return chosen;EquipUnitItemSlot(GetUnit(ai),chosen);SetupNormal(ai,bi,0);gActionData.scriptedBattleHits=NULL;BattleUnwind();BattleApplyExpGains();
 actorIndex=ai;targetIndex=bi;initialActorHP=displayActorHP=GetUnit(ai)->curHP;initialTargetHP=displayTargetHP=GetUnit(bi)->curHP;
 roundCount=0;while(roundCount<BATTLE_HIT_MAX&&!(gBattleHitArray[roundCount].info&BATTLE_HIT_INFO_END))roundCount++;
 if(roundCount>=BATTLE_HIT_MAX){error=-5;return error;}normalBattle=1;roundIndex=timer=completed=error=0;active=1;return roundCount;
}

extern const struct CharacterData *GetCharacterData(int id);
static int ObstaclePair(int ai,int x,int y,int slot){if(ai<1||ai>255||slot<0||slot>4||active||fe8_map_obstacle_hp(x,y)<=0)return -1;struct Unit *a=GetUnit(ai);if(!Supported(a)||a->state&(US_NOT_DEPLOYED|US_RESCUED)||!CanUnitUseWeapon(a,a->items[slot])||!IsItemCoveringRange(a->items[slot],abs(a->xPos-x)+abs(a->yPos-y)))return -2;return slot;}
static void SetupObstacle(int ai,int x,int y,int slot){struct Unit *a=GetUnit(ai);memset(&gBattleActor,0,sizeof(gBattleActor));memset(&gBattleTarget,0,sizeof(gBattleTarget));memset(&gBattleStats,0,sizeof(gBattleStats));InitBattleUnit(&gBattleActor,a);gBattleTarget.unit.pClassData=GetClassData(CLASS_OBSTACLE);gBattleTarget.unit.pCharacterData=GetCharacterData(fe8_map_terrain(x,y)==27?CHARACTER_WALL:CHARACTER_SNAG);gBattleTarget.unit.maxHP=fe8_map_obstacle_max(x,y);gBattleTarget.unit.curHP=fe8_map_obstacle_hp(x,y);gBattleTarget.unit.xPos=x;gBattleTarget.unit.yPos=y;gBattleTarget.statusOut=-1;gBattleStats.config=BATTLE_CONFIG_REAL|BATTLE_CONFIG_MAPANIMS;gBattleStats.range=abs(a->xPos-x)+abs(a->yPos-y);SetBattleUnitWeapon(&gBattleActor,slot);SetBattleUnitWeapon(&gBattleTarget,-1);BattleInitTargetCanCounter();BattleApplyWeaponTriangleEffect(&gBattleActor,&gBattleTarget);Terrain(&gBattleActor);Terrain(&gBattleTarget);ComputeBattleUnitStats(&gBattleActor,&gBattleTarget);ComputeBattleUnitStats(&gBattleTarget,&gBattleActor);ComputeBattleUnitEffectiveStats(&gBattleActor,&gBattleTarget);ComputeBattleUnitEffectiveStats(&gBattleTarget,&gBattleActor);ComputeBattleObstacleStats();}
int fe8_battle_obstacle_forecast(int ai,int x,int y,int slot,int field){if(ObstaclePair(ai,x,y,slot)<0)return -1;SetupObstacle(ai,x,y,slot);switch(field){case 0:return gBattleActor.battleAttack>gBattleTarget.battleDefense?gBattleActor.battleAttack-gBattleTarget.battleDefense:0;case 1:return gBattleActor.battleEffectiveHitRate;case 2:return gBattleActor.battleEffectiveCritRate;case 3:return 1;case 4:return slot;default:return -1;}}
int fe8_battle_obstacle_attack(int ai,int x,int y,int slot){if(ObstaclePair(ai,x,y,slot)<0)return -1;EquipUnitItemSlot(GetUnit(ai),slot);SetupObstacle(ai,x,y,0);gActionData.scriptedBattleHits=NULL;BattleUnwind();actorIndex=ai;targetIndex=0;obstacleX=x;obstacleY=y;initialActorHP=displayActorHP=GetUnit(ai)->curHP;initialTargetHP=displayTargetHP=fe8_map_obstacle_hp(x,y);roundCount=0;while(roundCount<BATTLE_HIT_MAX&&!(gBattleHitArray[roundCount].info&BATTLE_HIT_INFO_END))roundCount++;if(roundCount>=BATTLE_HIT_MAX){error=-5;return error;}normalBattle=3;roundIndex=timer=completed=error=0;active=1;return roundCount;}
static void CommitNormal(struct Unit *u,struct BattleUnit *bu){
 if(bu->canCounter&&bu->weaponSlotIndex<5)bu->unit.items[bu->weaponSlotIndex]=bu->weapon;
 UpdateUnitFromBattle(u,bu);
}
void fe8_battle_tick(void){
 if(!active)return;
 timer++;
 if(timer==24&&roundIndex<roundCount){struct BattleHit *h=gBattleHitArray+roundIndex;if(!(h->attributes&BATTLE_HIT_ATTR_MISS)){int *hp=(h->attributes&BATTLE_HIT_ATTR_DEVIL)?((h->info&BATTLE_HIT_INFO_RETALIATION)?&displayTargetHP:&displayActorHP):((h->info&BATTLE_HIT_INFO_RETALIATION)?&displayActorHP:&displayTargetHP);*hp-=h->hpChange;if(*hp<0)*hp=0;}}
 if(timer>=60){timer=0;roundIndex++;}
 if(roundIndex>=roundCount){struct Unit *a=GetUnit(actorIndex),*b=normalBattle==3?&gBattleTarget.unit:GetUnit(targetIndex);if(normalBattle==3){CommitNormal(a,&gBattleActor);if(fe8_map_obstacle_update(obstacleX,obstacleY,gBattleTarget.unit.curHP)<0)error=-13;active=0;completed++;if(!a->curHP){a->state|=US_HIDDEN|US_DEAD;fe8_death_after_battle(actorIndex,0);}return;}if(normalBattle){CommitNormal(a,&gBattleActor);CommitNormal(b,&gBattleTarget);}else{a->curHP=gBattleActor.unit.curHP;b->curHP=gBattleTarget.unit.curHP;a->state&=~US_UNSELECTABLE;b->state&=~US_UNSELECTABLE;}if(normalBattle&&fe8_tactical_rescue_death(actorIndex,targetIndex)<0)error=-12;if(!a->curHP)a->state|=US_HIDDEN|US_DEAD;if(!b->curHP)b->state|=US_HIDDEN|US_DEAD;active=0;completed++;if(normalBattle)fe8_death_after_battle(actorIndex,targetIndex);if(normalBattle==1){struct CombatActionProc proc={0};BATTLE_HandleItemDrop(&proc);}}
}
int fe8_battle_busy(void){return active;}
extern int GetUnitItemHealAmount(struct Unit *u,int item);
extern s8 AreUnitsAllied(int a,int b);
int fe8_item_stealable(int item){return IsItemStealable(item);}
int fe8_battle_steal(int actor,int target,int slot){
 if(active||actor<1||actor>63||target<129||target>191||slot<0||slot>=5)return -1;
 struct Unit *a=GetUnit(actor),*b=GetUnit(target);
 if(!UNIT_IS_VALID(a)||!UNIT_IS_VALID(b)||a->curHP<=0||b->curHP<=0||fe8_map_terrain(a->xPos,a->yPos)<0||fe8_map_terrain(a->xPos,a->yPos)>=65||!(UNIT_CATTRIBUTES(a)&CA_STEAL)||(a->state&(US_DEAD|US_NOT_DEPLOYED|US_RESCUED|US_HAS_MOVED))||(b->state&(US_DEAD|US_HIDDEN|US_NOT_DEPLOYED|US_RESCUED))||a->spd<b->spd||abs(a->xPos-b->xPos)+abs(a->yPos-b->yPos)!=1||GetUnitItemCount(a)>=5||!IsItemStealable(b->items[slot]))return -2;
 memset(&gBattleStats,0,sizeof(gBattleStats));memset(&gBattleActor,0,sizeof(gBattleActor));memset(&gBattleTarget,0,sizeof(gBattleTarget));
 int item=b->items[slot];gActionData.subjectIndex=actor;gActionData.targetIndex=target;gActionData.itemSlotIndex=slot;ActionSteal(NULL);
 UpdateUnitFromBattle(a,&gBattleActor);actorIndex=actor;targetIndex=target;normalBattle=4;completed=1;error=0;return item;
}
int fe8_battle_can_heal(int actor,int target,int slot){
 if(active||actor<1||actor>255||target<1||target>255||actor==target||slot<0||slot>4)return 0;struct Unit *a=GetUnit(actor),*b=GetUnit(target);if(!UNIT_IS_VALID(a)||!UNIT_IS_VALID(b)||a->curHP<=0||b->curHP<=0||(a->state|b->state)&(US_DEAD|US_NOT_DEPLOYED|US_RESCUED)||b->state&US_HIDDEN)return 0;
 if(!AreUnitsAllied(actor,target)||b->curHP>=GetUnitMaxHp(b)||a->xPos<0||a->yPos<0||b->xPos<0||b->yPos<0||a->xPos>=fe8_map_info(1)||a->yPos>=fe8_map_info(2)||b->xPos>=fe8_map_info(1)||b->yPos>=fe8_map_info(2))return 0;int item=a->items[slot],id=GetItemIndex(item);if(id!=ITEM_STAFF_HEAL&&id!=ITEM_STAFF_MEND&&id!=ITEM_STAFF_RECOVER)return 0;
 return CanUnitUseStaff(a,item)&&IsItemCoveringRange(item,abs(a->xPos-b->xPos)+abs(a->yPos-b->yPos));
}
int fe8_battle_heal(int actor,int target,int slot){
 if(!fe8_battle_can_heal(actor,target,slot))return -1;struct Unit *a=GetUnit(actor),*b=GetUnit(target);int item=a->items[slot],hp=b->curHP,amount=GetUnitItemHealAmount(a,item),next=hp+amount;if(next>GetUnitMaxHp(b))next=GetUnitMaxHp(b);
 memset(&gBattleStats,0,sizeof(gBattleStats));memset(&gBattleActor,0,sizeof(gBattleActor));memset(&gBattleTarget,0,sizeof(gBattleTarget));BattleInitItemEffect(a,slot);BattleInitItemEffectTarget(b);
 gBattleHitArray[0].info=BATTLE_HIT_INFO_BEGIN;gBattleHitArray[0].hpChange=hp-next;gBattleHitArray[1].info=BATTLE_HIT_INFO_END;gBattleTarget.unit.curHP=next;BattleApplyItemExpGains();
 // Original BattleApplyItemEffect durability path; Proc/animation replaced by existing map presentation.
 gBattleActor.weapon=GetItemAfterUse(item);gBattleActor.unit.items[slot]=gBattleActor.weapon;gBattleActor.weaponBroke=!gBattleActor.weapon;
 actorIndex=actor;targetIndex=target;initialActorHP=displayActorHP=a->curHP;initialTargetHP=displayTargetHP=hp;roundCount=1;roundIndex=timer=completed=error=0;normalBattle=2;active=1;return next-hp;
}
int fe8_battle_info(int field){switch(field){case 0:return active;case 1:return roundIndex;case 2:return roundCount;case 3:return actorIndex;case 4:return targetIndex;case 5:return initialActorHP;case 6:return initialTargetHP;case 7:return gBattleActor.unit.curHP;case 8:return gBattleTarget.unit.curHP;case 9:return completed;case 10:return gBattleActor.battleAttack;case 11:return gBattleTarget.battleAttack;case 12:return gBattleActor.battleDefense;case 13:return gBattleTarget.battleDefense;case 14:return error;case 15:return displayActorHP;case 16:return displayTargetHP;case 17:return timer;case 18:return normalBattle;case 60:return gBattleTarget.unit.maxHP;case 19:return gBattleActor.expGain;case 27:return gBattleTarget.expGain;case 28:return gBattleActor.unit.level;case 29:return gBattleActor.unit.exp;default:if(field>=20&&field<20+BATTLE_HIT_MAX)return gBattleHitArray[field-20].hpChange;if(field>=30&&field<30+BATTLE_HIT_MAX)return gBattleHitArray[field-30].attributes;if(field>=40&&field<40+BATTLE_HIT_MAX)return gBattleHitArray[field-40].info;return -1;}}
static void Pixel(u8 *rgba,int x,int y,int color){if(x<0||y<0||x>=240||y>=160)return;u8 *p=rgba+(y*240+x)*4;for(int c=0;c<3;c++){int v=color>>(c*5)&31;p[c]=(v<<3)|(v>>2);}p[3]=255;}
void fe8_battle_render(u8 *rgba,int cameraX,int cameraY){
 if(!active)return;
 struct Unit *a=GetUnit(actorIndex),*b=normalBattle==3?&gBattleTarget.unit:GetUnit(targetIndex);struct Unit *units[2]={a,b};int hp[2]={displayActorHP,displayTargetHP};
 for(int i=0;i<2;i++){int x=units[i]->xPos*16-cameraX,y=units[i]->yPos*16-cameraY+16,max=units[i]->maxHP;for(int dx=0;dx<16;dx++)for(int dy=0;dy<3;dy++)Pixel(rgba,x+dx,y+dy,dy==1&&dx*max<hp[i]*16?0x3e0:0);}
 if(timer>=20&&timer<32&&roundIndex<roundCount){struct BattleHit *h=gBattleHitArray+roundIndex;struct Unit *u=(h->info&BATTLE_HIT_INFO_RETALIATION)?a:b;int x=u->xPos*16-cameraX,y=u->yPos*16-cameraY;if(!(h->attributes&BATTLE_HIT_ATTR_MISS))for(int dx=-2;dx<18;dx++)for(int dy=-2;dy<18;dy++)if(dx==-2||dy==-2||dx==17||dy==17)Pixel(rgba,x+dx,y+dy,0x7fff);}
}

int fe8_battle_simulation_copy(struct BattleUnit *a,struct BattleUnit *b){if(!simulationValid||!a||!b)return -1;*a=simulationActor;*b=simulationTarget;return 0;}
