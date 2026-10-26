#include "ai.h"
#include "generated/battle_types.h"
#include "save.h"
#include "events.h"
#include "battle.h"
#include "tactical.h"
#include "map.h"
#include "units.h"
#include "rng.h"
#include "recovery.h"
#include "constants/terrains.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#define gPlaySt (*fe8_save_loaded_playst())
extern struct Unit *GetUnit(int index);
extern s8 CanUnitUseWeapon(struct Unit *u,int item);
extern s8 CanUnitUseStaff(struct Unit *u,int item);
extern int GetItemAttributes(int item);
extern int GetItemMaxRange(int item);
extern int GetItemMinRange(int item);
extern int GetItemMight(int item);
extern int GetUnitPower(struct Unit *unit);
#define AI_UNIT_FLAG_0 1
#define AI_UNIT_FLAG_3 8
#define AI_ACTION_ESCAPE 2
static struct {u8 units[116];} gAiState;
static u32 priorities[116],*sUnitPriorityArray=priorities;
static int prepared=-1,phaseIndex,phaseStep,phaseError,phaseCompleted;
static int phaseSimError,phaseSimTarget,phaseSimActor;
static void StartPhase(void);
static int InitHealingFlags(void);
static void ResetVm(void);
#include "generated/ai_order_routines.h"
static int dangerValid;
void fe8_ai_reset(void){phaseIndex=phaseStep=phaseError=phaseCompleted=phaseSimError=phaseSimTarget=phaseSimActor=0;dangerValid=0;ResetVm();prepared=-1;memset(&gAiState,0,sizeof(gAiState));memset(priorities,0,sizeof(priorities));}
int fe8_ai_prepare(void){if(fe8_event_busy()||fe8_battle_busy()||fe8_event_info(12)||(gPlaySt.faction!=FACTION_RED&&gPlaySt.faction!=FACTION_GREEN))return -1;if(InitHealingFlags()<0){prepared=0;phaseError=9;return -1;}prepared=BuildAiUnitList();SortAiUnitList(prepared);gAiState.units[prepared]=0;StartPhase();return prepared;}
int fe8_ai_info(int field){if(field==0)return prepared;if(field==1)return phaseIndex;if(field==2)return phaseStep;if(field==3)return phaseError;if(field==4)return phaseCompleted;if(field==5)return phaseSimError;if(field==6)return phaseSimTarget;if(field==7)return phaseSimActor;if(field>=10&&field<126)return prepared<0?-1:gAiState.units[field-10];if(field>=130&&field<246)return prepared<0?-1:priorities[field-130];return -1;}

// Script probing is read-only with respect to chapter units. Pending action handlers
// stop before rolling RNG, moving, attacking or dereferencing ROM callback keys.
#include "generated/ai_vm_types.h"
#include "generated/ai_vm_assets.h"
static struct AiScr **gpAi1Table[]={aiVmA},**gpAi2Table[]={aiVmB};
static struct Unit *aiScriptUnit;
static struct {int decideState,unk7C,unk7E,bestBlueMov,dangerMapFilled,flags,combatWeightTableId;u32 specialItemFlags;u8 cmd_result[8];} aiVmState;
static struct AiDecision gAiDecision;
static struct Vec2 gBmMapSize;
static u8 vmTerrain[64][64];
static u8 *gBmMapTerrain[64];
#define gMapMovementSigned ((s8**)gBmMapMovement)
static u8 vmRange[64][64],vmMove[64][64],vmOccupancy[64][64],vmDanger[64][64];
static u8 *gBmMapRange[64],*gBmMapMovement[64],*gBmMapUnit[64],*gBmMapOther[64],**gWorkingBmMap;
static void BmMapFill(u8 **map,int value){for(int y=0;y<gBmMapSize.y;y++)memset(map[y],value,gBmMapSize.x);}
void MapAddInBoundedRange(short x,short y,short min,short max);
#define gMapRangeSigned ((s8**)gBmMapRange)
#define RECT_DISTANCE(x1,y1,x2,y2) (abs((x1)-(x2))+abs((y1)-(y2)))
extern struct Unit *GetUnitFromCharId(int pid);
extern s8 AreUnitsAllied(int a,int b);
static u8 gActiveUnitId;
static struct AiScr *gpAiScriptCurrent;
static int gAiScriptEnded,gAiScriptKind,vmError,vmSteps,vmIndex,vmPc,vmAi1,vmAi2;
static s8 AiVmCompareKey(const void *key,u8 op,u32 right);
static void GenerateExtendedMovementMapOnRange(int x,int y,const s8 *costs){(void)costs;if(fe8_tactical_probe_movement(aiScriptUnit,x,y,120,0,(u8*)vmRange)<0)vmError=6;}
static const s8 *GetUnitMovementCost(struct Unit *u){return fe8_tactical_movement_cost(u);} // Tactical helper selects the original class/weather cost table.
static void GenerateUnitMovementMap(struct Unit *u){gWorkingBmMap=gBmMapRange;if(fe8_tactical_probe_movement(u,u->xPos,u->yPos,UNIT_MOV(u),(u8)u->index,(u8*)vmMove)<0)vmError=6;}
static void sub_80410C4(int x,int y,struct Unit *u){if(fe8_tactical_probe_movement(u,x,y,120,(u8)u->index,(u8*)vmRange)<0)vmError=6;} // Original extended range with the acting unit occupancy index.
static s8 AiCheckDangerAt(int x,int y,u8 limit){return vmDanger[y][x]<=limit;} // Only limit 255 is accepted; no danger value can exclude a tile.
#include "generated/ai_target_lists.h"
static const u16 *ResolveAiShortList(const void *key){for(int i=0;i<sizeof(aiShortLists)/sizeof(*aiShortLists);i++)if(aiShortLists[i].key==(u32)(uintptr_t)key)return aiShortLists[i].data;return NULL;}
static s8 AiIsUnitEnemyAndNotInScrList(struct Unit *u);
static void MarkWorkingMapEdges(void){for(int y=gBmMapSize.y-1;y>=0;y--)for(int x=gBmMapSize.x-1;x>=0;x--){if(vmRange[y][x]>=120)continue;if(x>0&&(s8)vmRange[y][x-1]<0)vmRange[y][x-1]=120;if(x+1<gBmMapSize.x&&(s8)vmRange[y][x+1]<0)vmRange[y][x+1]=120;if(y>0&&(s8)vmRange[y-1][x]<0)vmRange[y-1][x]=120;if(y+1<gBmMapSize.y&&(s8)vmRange[y+1][x]<0)vmRange[y+1][x]=120;}}
static int runtimeScript,runtimeHealing;
static void NativeAction(u8 *pc);
static void NativeLoot(u8 *pc);
static void AiNativeUnsupported(u8 *pc){if(runtimeScript&&(gpAiScriptCurrent->cmd==5||gpAiScriptCurrent->cmd==7)){NativeAction(pc);return;}if(runtimeScript&&(gpAiScriptCurrent->cmd==16||gpAiScriptCurrent->cmd==17)){NativeLoot(pc);return;}vmError=100+gpAiScriptCurrent->cmd;}
static void SetWorkingBmMap(u8 **map){gWorkingBmMap=map;}
static int Div(int a,int b){return a/b;}
extern int GetUnitEquippedWeapon(struct Unit *u);
extern int GetUnitEquippedWeaponSlot(struct Unit *u);
extern int GetItemIndex(int item);
static void GenerateUnitMovementMapExt(struct Unit *u,int limit){if(fe8_tactical_probe_movement(u,u->xPos,u->yPos,limit,(u8)u->index,(u8*)vmMove)<0)vmError=6;}
extern int GetUnitMaxHp(struct Unit *u);
static int GetUnitCurrentHp(struct Unit *u){return u->curHP;}
#define AI_FLAG_STAY 2
#define AI_UNIT_CONFIG_FLAG_STAY (1<<13)
#define AI_UNIT_CONFIG_HEALTHRESHOLD_MASK 7
#define AI_ACTION_COMBAT 1
#define AI_ACTION_USEITEM 6
#define AI_UNIT_FLAG_1 2
#define AI_UNIT_FLAG_2 4
#define MAP_MOVEMENT_EXTENDED 120
#include "generated/ai_combat_assets.h"
static const struct AiCombatScoreCoefficients *sCombatScoreCoefficients;
static struct BattleUnit combatActor,combatTarget;
static u32 combatLastScore;
#define gBattleActor combatActor
#define gBattleTarget combatTarget
#define gActiveUnit aiScriptUnit
#define gAiState aiVmState
static s8 AiTryMoveTowardsEscape(void);
#include "generated/ai_vm_routines.h"
#include "generated/ai_danger_routines.h"
extern int GetUnitItemCount(struct Unit *u);
static s8 AiAttemptStealActionWithinMovement(void);
static void MarkMovementMapEdges(void){for(int y=0;y<gBmMapSize.y;y++)for(int x=0;x<gBmMapSize.x;x++){if(vmMove[y][x]>=120)continue;if(x&&vmMove[y][x-1]==255)vmMove[y][x-1]=120;if(x+1<gBmMapSize.x&&vmMove[y][x+1]==255)vmMove[y][x+1]=120;if(y&&vmMove[y-1][x]==255)vmMove[y-1][x]=120;if(y+1<gBmMapSize.y&&vmMove[y+1][x]==255)vmMove[y+1][x]=120;}}
#define AI_ACTION_STEAL 3
#include "generated/ai_combat_routines.h"
#include "generated/ai_heal_assets.h"
#include "generated/ai_heal_routines.h"
#include "constants/items.h"
#define AI_ACTION_PILLAGE 4
#define AI_FLAG_0 1

static s8 gWorkingTerrainMoveCosts[65];
static void SetWorkingMoveCosts(const s8 *costs){memcpy(gWorkingTerrainMoveCosts,costs,65);} // Native tactical probe selects class/weather costs.
static void GenerateMovementMap(int x,int y,int limit,int id){if(fe8_tactical_probe_costs(aiScriptUnit,x,y,limit,id,(u8*)gWorkingBmMap[0],gWorkingTerrainMoveCosts)<0)vmError=6;}
#define AI_FLAG_3 8
#define AI_FLAG_BERSERKED 4
#define UNIT_ACTION_STEAL 6
#define CHARACTER_RENNAC 0x1c
extern int GetItemType(int item);
static s8 sub_8040C5C(void){if(aiScriptUnit->pClassData->number==CLASS_ROGUE)vmError=7;return 0;} // Rogue-specific logic remains a guarded boundary.
static s8 AiTryDoSpecialItems(void);

#include "generated/ai_loot.h"
#include "generated/ai_keys.h"
#include "generated/ai_escape.h"
#include "generated/ai_steal.h"
#undef gBattleActor
#undef gBattleTarget
#undef gAiState
#undef gActiveUnit
static void ResetVm(void){memset(&gAiDecision,0,sizeof(gAiDecision));aiScriptUnit=NULL;gpAiScriptCurrent=NULL;memset(&aiVmState,0,sizeof(aiVmState));gAiScriptEnded=gAiScriptKind=vmError=vmSteps=vmIndex=vmPc=vmAi1=vmAi2=0;}
static s8 AiVmCompareKey(const void *key,u8 op,u32 right){uintptr_t p=(uintptr_t)key;if(p>=AI_VM_STATE_KEY+0x86&&p<AI_VM_STATE_KEY+0x8e)return AiCompare(aiVmState.cmd_result+p-(AI_VM_STATE_KEY+0x86),op,right);vmError=5;return 0;}
static int ProbeScript(int index,int kind,int startPc,int override){
 if(fe8_tactical_info(0)==7||index<1||index>255||kind<0||kind>1||fe8_event_busy()||fe8_battle_busy()||!fe8_map_active()||!UNIT_IS_VALID(GetUnit(index)))return -1;
 if(startPc>255||startPc<-1||override>255)return -2;
 if(GetUnit(index)->xPos<0||GetUnit(index)->yPos<0||GetUnit(index)->xPos>=fe8_map_info(1)||GetUnit(index)->yPos>=fe8_map_info(2))return -2;
 memset(&aiVmState,0,sizeof(aiVmState));AiClearDecision();gBmMapSize.x=fe8_map_info(1);gBmMapSize.y=fe8_map_info(2);memset(vmOccupancy,0,sizeof(vmOccupancy));memset(vmDanger,0,sizeof(vmDanger));
 for(int y=0;y<64;y++){gBmMapRange[y]=vmRange[y];gBmMapMovement[y]=vmMove[y];gBmMapUnit[y]=vmOccupancy[y];gBmMapOther[y]=vmDanger[y];}
 for(int i=1;i<256;i++){struct Unit *u=GetUnit(i);if(UNIT_IS_VALID(u)&&!(u->state&(US_HIDDEN|US_DEAD|US_NOT_DEPLOYED|US_RESCUED))&&u->xPos>=0&&u->yPos>=0&&u->xPos<gBmMapSize.x&&u->yPos<gBmMapSize.y)vmOccupancy[u->yPos][u->xPos]=i;}
 gActiveUnitId=index;
 struct Unit copy=*GetUnit(index);if(override>=0){if(kind)copy.ai2=override;else copy.ai1=override;}aiScriptUnit=&copy;vmError=vmSteps=0;vmIndex=index;gAiScriptKind=kind;gpAiScriptCurrent=NULL;
 u8 *pc=kind?&copy.ai_b_pc:&copy.ai_a_pc;if(startPc>=0){*pc=startPc;}
 for(int i=0;i<256;i++){
  int id=kind?copy.ai2:copy.ai1;int count=kind?sizeof(aiVmB)/sizeof(*aiVmB):sizeof(aiVmA)/sizeof(*aiVmA);
  if(id>=count||*pc>=(kind?aiVmBLengths[id]:aiVmALengths[id])){vmError=2;break;}
  struct AiScr *command=(kind?aiVmB[id]:aiVmA[id])+*pc;if(command->cmd==AI_CMD_LABEL){vmError=3;break;}
  if(kind)AiTryExecScriptB();else AiTryExecScriptA();vmSteps++;
  if(vmError||gAiScriptEnded)break;
  if(i==255)vmError=4;
 }
 vmPc=*pc;vmAi1=copy.ai1;vmAi2=copy.ai2;aiScriptUnit=NULL;
 return vmError?-vmError:0;
}
int fe8_ai_script_probe(int index,int kind,int startPc){return ProbeScript(index,kind,startPc,-1);}
int fe8_ai_script_table_probe(int index,int kind,int id,int pc){if(id<0)return -2;return ProbeScript(index,kind,pc,id);}
int fe8_ai_script_info(int field){switch(field){case 0:return vmError;case 1:return vmSteps;case 2:return vmIndex;case 3:return gAiScriptKind;case 4:return vmPc;case 5:return vmAi1;case 6:return vmAi2;case 7:return gpAiScriptCurrent?gpAiScriptCurrent->cmd:-1;case 8:return gAiDecision.actionPerformed;case 9:return gAiDecision.xMove;case 10:return gAiDecision.yMove;case 11:return gAiDecision.actionId;case 12:return gAiDecision.targetId;default:return -1;}}

// Isolated danger computation keeps script decision diagnostics and player maps intact.
static u8 dangerResult[64][64];
int fe8_ai_danger_probe(int index){
 if(fe8_tactical_info(0)==7)return -1;dangerValid=0;if(index<1||index>255||fe8_event_busy()||fe8_battle_busy()||!fe8_map_active())return -1;
 struct Unit *u=GetUnit(index);if(!UNIT_IS_VALID(u)||u->xPos<0||u->yPos<0||u->xPos>=fe8_map_info(1)||u->yPos>=fe8_map_info(2))return -2;
 struct Unit *oldUnit=aiScriptUnit;u8 oldId=gActiveUnitId;int oldError=vmError,oldFilled=aiVmState.dangerMapFilled;struct Vec2 oldSize=gBmMapSize;u8 **oldWorking=gWorkingBmMap;
 u8 oldRange[4096],oldMove[4096],oldOccupancy[4096],oldDanger[4096];memcpy(oldRange,vmRange,4096);memcpy(oldMove,vmMove,4096);memcpy(oldOccupancy,vmOccupancy,4096);memcpy(oldDanger,vmDanger,4096);
 gBmMapSize.x=fe8_map_info(1);gBmMapSize.y=fe8_map_info(2);aiScriptUnit=u;gActiveUnitId=index;vmError=aiVmState.dangerMapFilled=0;
 memset(vmOccupancy,0,sizeof(vmOccupancy));for(int y=0;y<64;y++){gBmMapRange[y]=vmRange[y];gBmMapMovement[y]=vmMove[y];gBmMapUnit[y]=vmOccupancy[y];gBmMapOther[y]=vmDanger[y];}
 for(int i=1;i<256;i++){struct Unit *v=GetUnit(i);if(UNIT_IS_VALID(v)&&!(v->state&(US_HIDDEN|US_DEAD|US_NOT_DEPLOYED|US_RESCUED))&&v->xPos>=0&&v->yPos>=0&&v->xPos<gBmMapSize.x&&v->yPos<gBmMapSize.y)vmOccupancy[v->yPos][v->xPos]=i;}
 AiInitDangerMap();int result=vmError?-vmError:0;if(!result){memcpy(dangerResult,vmDanger,4096);dangerValid=1;}
 memcpy(vmRange,oldRange,4096);memcpy(vmMove,oldMove,4096);memcpy(vmOccupancy,oldOccupancy,4096);memcpy(vmDanger,oldDanger,4096);aiScriptUnit=oldUnit;gActiveUnitId=oldId;vmError=oldError;aiVmState.dangerMapFilled=oldFilled;gBmMapSize=oldSize;gWorkingBmMap=oldWorking;return result;
}
int fe8_ai_danger_at(int x,int y){return !dangerValid||x<0||y<0||x>=fe8_map_info(1)||y>=fe8_map_info(2)?-1:dangerResult[y][x];}

static void SyncVmMaps(void){
 gBmMapSize.x=fe8_map_info(1);gBmMapSize.y=fe8_map_info(2);memset(vmOccupancy,0,sizeof(vmOccupancy));
 for(int y=0;y<64;y++){gBmMapRange[y]=vmRange[y];gBmMapMovement[y]=vmMove[y];gBmMapUnit[y]=vmOccupancy[y];gBmMapOther[y]=vmDanger[y];gBmMapTerrain[y]=vmTerrain[y];for(int x=0;x<64;x++){int t=fe8_map_terrain(x,y);vmTerrain[y][x]=t<0?0:t;}}
 for(int i=1;i<256;i++){struct Unit *u=GetUnit(i);if(UNIT_IS_VALID(u)&&!(u->state&(US_HIDDEN|US_DEAD|US_NOT_DEPLOYED|US_RESCUED))&&u->xPos>=0&&u->yPos>=0&&u->xPos<gBmMapSize.x&&u->yPos<gBmMapSize.y)vmOccupancy[u->yPos][u->xPos]=i;}
}
static void NativeLoot(u8 *pc){if(gpAiScriptCurrent->cmd==16)AiScriptCmd_10_DoLooting(pc);else AiScriptCmd_11_MoveTowardsSafety(pc);}
s8 AiTryDoStaff(s8 (*isEnemy)(struct Unit *));
static void NativeAction(u8 *pc){
 if(gpAiScriptCurrent->unk_08&&!ResolveAiShortList(gpAiScriptCurrent->unk_08)){vmError=6;return;}
 for(int i=0;i<5;i++){int item=aiScriptUnit->items[i],id=GetItemIndex(item);if(GetItemAttributes(item)&IA_MAGICDAMAGE||((GetItemAttributes(item)&IA_STAFF)&&(id!=ITEM_STAFF_HEAL&&id!=ITEM_STAFF_MEND&&id!=ITEM_STAFF_RECOVER))){vmError=7;return;}}
 u8 chance=NextRN_N(100);if(chance<=gpAiScriptCurrent->unk_01){if(gpAiScriptCurrent->cmd==7)aiVmState.flags|=AI_FLAG_STAY;if(!AiTryDoStaff(AiIsUnitEnemy)&&!vmError)AiAttemptOffensiveAction(gpAiScriptCurrent->cmd==5&&gpAiScriptCurrent->unk_08?AiIsUnitEnemyAndNotInScrList:AiIsUnitEnemy);}else aiVmState.decideState=4;(*pc)++;
}
#define AI_ACTION_STAFF 5
extern int GetItemRequiredExp(int item);
static void GenerateMagicSealMap(int value){(void)value;for(int i=129;i<192;i++){struct Unit *u=GetUnit(i);if(UNIT_IS_VALID(u)&&UNIT_CATTRIBUTES(u)&CA_MAGICSEAL){vmError=7;return;}}} // Magic-seal maps remain guarded.
#define gActiveUnit aiScriptUnit
#define gAiState aiVmState
#include "generated/ai_staff_routines.h"
#undef gAiState
#undef gActiveUnit
static int RunRuntimeScript(int kind){
 gAiScriptKind=kind;u8 *pc=kind?&aiScriptUnit->ai_b_pc:&aiScriptUnit->ai_a_pc;runtimeScript=1;
 for(int n=0;n<256;n++){int id=kind?aiScriptUnit->ai2:aiScriptUnit->ai1,count=kind?sizeof(aiVmB)/sizeof(*aiVmB):sizeof(aiVmA)/sizeof(*aiVmA);if(id>=count||*pc>=(kind?aiVmBLengths[id]:aiVmALengths[id])){vmError=2;break;}struct AiScr *c=(kind?aiVmB[id]:aiVmA[id])+*pc;if(c->cmd==AI_CMD_LABEL){vmError=3;break;}if(kind)AiTryExecScriptB();else AiTryExecScriptA();if(vmError||gAiScriptEnded)break;if(n==255)vmError=4;}
 runtimeScript=0;return vmError?-vmError:0;
}
static void StartPhase(void){phaseIndex=phaseStep=phaseError=phaseCompleted=0;memset(&aiVmState,0,sizeof(aiVmState));aiVmState.flags=AI_FLAG_0;aiVmState.unk7E=255;aiVmState.specialItemFlags=nativeAiItemConfig[(u8)gPlaySt.chapterIndex];for(int i=1;i<64;i++){struct Unit *u=GetUnit(i);if(UNIT_IS_VALID(u)&&!(u->state&(US_HIDDEN|US_DEAD|US_BIT16))&&UNIT_MOV(u)>aiVmState.bestBlueMov)aiVmState.bestBlueMov=UNIT_MOV(u);}}
int fe8_ai_tick(void){
 if(prepared<0||phaseError)return -1;if(fe8_event_busy()||fe8_event_info(12))return 0;
 if(phaseStep==1){if(fe8_units_busy())return 0;if(gAiDecision.actionId==AI_ACTION_STAFF){int r=fe8_battle_heal(gAiDecision.unitId,gAiDecision.targetId,gAiDecision.itemSlot);if(r<0){phaseError=20-r;return -1;}phaseStep=2;return 0;}if(gAiDecision.actionId==AI_ACTION_COMBAT){int r=fe8_battle_attack(gAiDecision.unitId,gAiDecision.targetId,gAiDecision.itemSlot);if(r<0){phaseError=20-r;return -1;}phaseStep=2;return 0;}if(gAiDecision.actionId==AI_ACTION_STEAL&&fe8_unit_steal(gAiDecision.unitId,gAiDecision.targetId,gAiDecision.itemSlot)<0){phaseError=14;return -1;}if(gAiDecision.actionId==AI_ACTION_USEITEM&&((GetItemIndex(GetUnit(gAiDecision.unitId)->items[gAiDecision.itemSlot])==ITEM_DOORKEY||GetItemIndex(GetUnit(gAiDecision.unitId)->items[gAiDecision.itemSlot])==ITEM_LOCKPICK)?fe8_tactical_npc_key(gAiDecision.unitId,gAiDecision.itemSlot):fe8_recovery_item(gAiDecision.unitId,gAiDecision.itemSlot))<0){phaseError=12;return -1;}if(gAiDecision.actionId==AI_ACTION_PILLAGE&&(fe8_map_terrain(gAiDecision.xMove,gAiDecision.yMove)==TERRAIN_CHEST_FULL?fe8_tactical_npc_key(gAiDecision.unitId,gAiDecision.itemSlot):fe8_event_pillage(gAiDecision.unitId))<0){phaseError=13;return -1;}phaseStep=2;}
 if(phaseStep==2){if(fe8_battle_busy())return 0;struct Unit *u=GetUnit(gAiDecision.unitId);u->state|=US_HAS_MOVED_AI|US_UNSELECTABLE;phaseStep=0;phaseIndex++;phaseCompleted++;fe8_event_actor((u8)u->index);fe8_event_post_action();return 0;}
 if(phaseIndex>=prepared)return 1;
 int index=gAiState.units[phaseIndex];struct Unit *real=GetUnit(index);if(!UNIT_IS_VALID(real)||real->state&(US_DEAD|US_UNSELECTABLE)){phaseIndex++;return 0;}
 struct Unit copy=*real;aiScriptUnit=&copy;gActiveUnitId=index;vmError=0;SyncVmMaps();AiUpdateNoMoveFlag(&copy);aiVmState.combatWeightTableId=(copy.ai_config>>3)&31;
 if(aiVmState.combatWeightTableId>=AI_COEFFICIENT_COUNT){phaseError=9;aiScriptUnit=NULL;return -1;}
 // Healing and escape must be ported before these branches can execute faithfully.
 if((copy.ai_config&7)>=sizeof(gAI3HealingThresholdTable)/sizeof(*gAI3HealingThresholdTable)){phaseError=9;aiScriptUnit=NULL;return -1;}

 aiVmState.dangerMapFilled=0;AiInitDangerMap();AiClearDecision();aiVmState.decideState=0;
 if(!vmError&&AiUpdateGetUnitIsHealing(&copy)){
  if(!AiTryHealSelf()){
   struct Vec2 heal;if(AiTryGetNearestHealPoint(&heal)){
    runtimeHealing=1;AiTryMoveTowards(heal.x,heal.y,0,0,1);runtimeHealing=0;
    if(gAiDecision.actionPerformed){if(UNIT_CATTRIBUTES(&copy)&(CA_DANCE|CA_PLAY|CA_STEAL))vmError=7;else sub_803EEB0(gAiDecision.xMove,gAiDecision.yMove);}
   }
  }
 }
 if(!vmError&&!gAiDecision.actionPerformed&&(copy.aiFlags&8))AiTryMoveTowardsEscape();
 if(!vmError&&!gAiDecision.actionPerformed)RunRuntimeScript(0);if(!vmError&&!gAiDecision.actionPerformed&&aiVmState.decideState!=4){aiVmState.decideState=4;RunRuntimeScript(1);}
 if(vmError){phaseError=vmError;aiScriptUnit=NULL;return -1;}
 real->ai1=copy.ai1;real->ai2=copy.ai2;real->ai_a_pc=copy.ai_a_pc;real->ai_b_pc=copy.ai_b_pc;real->aiFlags=copy.aiFlags;real->ai_counter=copy.ai_counter;real->state|=US_HAS_MOVED_AI;aiScriptUnit=NULL;
 if(!gAiDecision.actionPerformed||(gAiDecision.xMove==real->xPos&&gAiDecision.yMove==real->yPos&&gAiDecision.actionId==AI_ACTION_NONE)){phaseIndex++;return 0;}
 if(gAiDecision.actionId!=AI_ACTION_STAFF&&gAiDecision.actionId!=AI_ACTION_NONE&&gAiDecision.actionId!=AI_ACTION_COMBAT&&gAiDecision.actionId!=AI_ACTION_USEITEM&&gAiDecision.actionId!=AI_ACTION_PILLAGE&&gAiDecision.actionId!=AI_ACTION_STEAL){phaseError=10;return -1;}
 if(fe8_tactical_npc_move(index,gAiDecision.xMove,gAiDecision.yMove)<0){phaseError=11;return -1;}fe8_map_focus(gAiDecision.xMove,gAiDecision.yMove);phaseStep=1;return 0;
}

int fe8_ai_offensive_probe(int index,int weight,int stay){
 if(fe8_tactical_info(0)==7||index<1||index>255||weight<0||weight>=AI_COEFFICIENT_COUNT||fe8_event_busy()||fe8_battle_busy()||phaseStep||!fe8_map_active())return -1;
 struct Unit *u=GetUnit(index);if(!UNIT_IS_VALID(u)||u->xPos<0||u->yPos<0||u->xPos>=fe8_map_info(1)||u->yPos>=fe8_map_info(2))return -2;
 struct Unit copy=*u;SyncVmMaps();aiScriptUnit=&copy;gActiveUnitId=index;vmError=0;aiVmState.flags=stay?AI_FLAG_STAY:0;aiVmState.combatWeightTableId=weight;aiVmState.dangerMapFilled=0;combatLastScore=0;AiClearDecision();AiInitDangerMap();if(!vmError)AiAttemptOffensiveAction(AiIsUnitEnemy);aiScriptUnit=NULL;return vmError?-vmError:gAiDecision.actionPerformed;
}
int fe8_ai_offensive_info(int field){switch(field){case 0:return vmError;case 1:return gAiDecision.xMove;case 2:return gAiDecision.yMove;case 3:return gAiDecision.targetId;case 4:return gAiDecision.itemSlot;case 5:return combatLastScore;default:return -1;}}

static int InitHealingFlags(void){
 int limit=gPlaySt.faction==FACTION_RED?50:20;
 for(int i=gPlaySt.faction+1;i<=gPlaySt.faction+limit;i++){struct Unit *u=GetUnit(i);if(UNIT_IS_VALID(u)&&((u->ai_config&7)>=sizeof(gAI3HealingThresholdTable)/sizeof(*gAI3HealingThresholdTable)||GetUnitMaxHp(u)<=0))return -1;}
 UpdateAllPhaseHealingAIStatus();return 0;
}
