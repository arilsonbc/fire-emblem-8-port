#include "tactical.h"
#include "fog.h"
#include "proc.h"
#include "battle.h"
#include "support.h"
#include "ai.h"
#include "recovery.h"
#include "audio.h"
#include "generated/battle_types.h"
#include "units.h"
#include "map.h"
#include "events.h"
#include "save.h"
#include "rng.h"
#include "gameover.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#define gPlaySt (*fe8_save_loaded_playst())
#include "generated/tactical_assets.h"
extern struct Unit *GetUnit(int index);
extern int GetUnitMaxHp(struct Unit *unit);
struct Unit *gActiveUnit;
static u8 gActiveUnitId;
static struct Vec2 gActiveUnitMoveOrigin,gBmMapSize;
static struct {u8 subjectIndex,unitActionType,moveCount,itemSlotIndex,targetIndex;} gActionData;
static struct {int gameStateBits,taken_action,unk3F;} gBmSt;
struct Trap {int xPos,yPos,type;};
#define TRAP_BALLISTA 1
static struct Trap *GetTrapAt(int x,int y){(void)x;(void)y;return NULL;}
static int CanCharacterBePrepMoved(int pid){(void)pid;return 0;}
static void sub_802C334(void){}
static int squaresMoved[256],fogBump;
static void PidStatsAddSquaresMoved(int pid,int count){squaresMoved[pid&255]+=count;}
static void PidStatsSubFavval08(int pid){(void)pid;} // Usage statistics persistence remains a separate integration.

static int mode=-1,selected,menuChoice,lastError,floodError,cantoActive,eventAction=1;
static int stealPeer,stealRow,lastStolen;
static int supplyColumn,supplyRow,supplyChanged;
static int inventoryRow,inventoryAction,inventoryConfirm;
static int FinishAction(int action);
static int InventoryMenu(int *ids,const char **labels);
static void FindTargets(void);
static struct {int index,slot,x,y;} targets[255*5+64*64*5];
static int targetCount,targetChoice;

static u8 terrain[64][64],occupancy[64][64],movement[64][64],hiddenMap[64][64];
static u8 *gBmMapHidden[64];
#define HIDDEN_BIT_UNIT 1
#define MAP_MOVEMENT_EXTENDED 124
#define RECT_DISTANCE(x1,y1,x2,y2) (abs((x1)-(x2))+abs((y1)-(y2)))
static u8 *gBmMapTerrain[64],*gBmMapUnit[64],*gBmMapMovement[64],**gWorkingBmMap;
static u8 gWorkingTerrainMoveCosts[65];
static struct MovMapFillStateExt gMovMapFillStPool1[8192],gMovMapFillStPool2[8192],*poolEnd;
static struct MovMapFillState gMovMapFillState;
static void SetWorkingBmMap(u8 **map){gWorkingBmMap=map;}
static void BmMapFill(u8 **map,int value){for(int y=0;y<gBmMapSize.y;y++)memset(map[y],value,gBmMapSize.x);}
static void CallARM_FillMovementMap(void);
#define TILEMAP_INDEX(x,y) ((y)*32+(x))
#include "generated/menu_font.h"
struct TradeMenuProc {struct Unit *units[2];int hoverColumn,hoverRow,selectedColumn,selectedRow,hasTraded;};
enum {UNIT_ACTION_WAIT=1,UNIT_ACTION_COMBAT=2,UNIT_ACTION_STAFF=3,UNIT_ACTION_TRADED=0x1b};
static void TradeMenu_RefreshItemText(struct TradeMenuProc *proc){(void)proc;} // Native renderer reads inventory every frame.
extern void UnitRemoveInvalidItems(struct Unit *u);
extern int GetUnitItemCount(struct Unit *u);
static struct Unit *gSubjectUnit;
static int tradeCollectCount,tradeCollectPopulate,tradePeer,tradeColumn,tradeRow,tradeChosenColumn=-1,tradeChosenRow,hasTraded;
static void AddTarget(int x,int y,int index,int extra){index=(u8)index;(void)x;(void)y;(void)extra;if(tradeCollectCount>=255)return;if(tradeCollectPopulate)targets[tradeCollectCount]=(typeof(*targets)){index,-1,x,y};tradeCollectCount++;}
#include "generated/tactical_routines.h"
static void CallARM_FillMovementMap(void){
 // Equivalent native loop in the original ARM core's order. No ARM instructions are executed.
 for(int wave=0;wave<4096;wave++){
  gMovMapFillState.src=(wave&1)?gMovMapFillStPool2:gMovMapFillStPool1;
  gMovMapFillState.dst=(wave&1)?gMovMapFillStPool1:gMovMapFillStPool2;
  poolEnd=gMovMapFillState.dst+8191;
  if(gMovMapFillState.src->connexion==4)return;
  for(int n=0;n<8192&&gMovMapFillState.src->connexion!=4;n++,gMovMapFillState.src++){
   int direction=gMovMapFillState.src->connexion;
   if(direction!=2)sub_801A570(3,0,-1);
   if(direction!=3)sub_801A570(2,0,1);
   if(direction!=1)sub_801A570(0,-1,0);
   if(direction!=0)sub_801A570(1,1,0);
   if(floodError)return;gMovMapFillState.dst->connexion=4;
  }
 }
 floodError=3;
}
static void SyncMaps(void){
 fe8_fog_refresh();memset(hiddenMap,0,sizeof(hiddenMap));
 gBmMapSize.x=fe8_map_info(1);gBmMapSize.y=fe8_map_info(2);
 memset(occupancy,0,sizeof(occupancy));
 for(int y=0;y<64;y++){gBmMapHidden[y]=hiddenMap[y];gBmMapTerrain[y]=terrain[y];gBmMapUnit[y]=occupancy[y];gBmMapMovement[y]=movement[y];for(int x=0;x<64;x++){int t=fe8_map_terrain(x,y);terrain[y][x]=t<0?0:t;}}
 for(int i=1;i<256;i++){struct Unit *u=GetUnit(i);if(UNIT_IS_VALID(u)&&!(u->state&(US_HIDDEN|US_DEAD|US_NOT_DEPLOYED|US_RESCUED))&&u->xPos>=0&&u->yPos>=0&&u->xPos<gBmMapSize.x&&u->yPos<gBmMapSize.y){if(UNIT_FACTION(u)==FACTION_RED&&!fe8_fog_visible(u->xPos,u->yPos)&&gPlaySt.faction!=FACTION_RED)hiddenMap[u->yPos][u->xPos]|=HIDDEN_BIT_UNIT;else occupancy[u->yPos][u->xPos]=i;}}
}
void fe8_tactical_reset(void){fogBump=0;inventoryRow=inventoryAction=inventoryConfirm=supplyColumn=supplyRow=supplyChanged=stealPeer=stealRow=lastStolen=0;cantoActive=0;eventAction=1;tradePeer=tradeColumn=tradeRow=tradeChosenRow=hasTraded=0;tradeChosenColumn=-1;fe8_recovery_reset();fe8_ai_reset();targetCount=targetChoice=0;gActiveUnit=NULL;gActiveUnitId=selected=menuChoice=lastError=floodError=0;mode=-1;memset(movement,255,sizeof(movement));memset(squaresMoved,0,sizeof(squaresMoved));}
int fe8_tactical_start(void){if(mode==0&&!fe8_event_busy())return 0;if(fe8_event_busy()||!fe8_map_active()||mode>0)return -1;SyncMaps();fe8_event_show_icons();gPlaySt.faction=FACTION_BLUE;if(!gPlaySt.chapterTurnNumber)gPlaySt.chapterTurnNumber=1;mode=0;selected=lastError=0;fe8_recovery_phase(FACTION_BLUE);fe8_audio_map_start();if(gPlaySt.chapterIndex>=1&&gPlaySt.chapterIndex<=7)fe8_event_phase();return 0;}
int fe8_tactical_select(int index){
 if(mode!=0||gPlaySt.faction!=FACTION_BLUE||fe8_event_busy()||index<1||index>255)return -1;
 struct Unit *u=GetUnit(index);if(!UNIT_IS_VALID(u)||u->xPos<0||u->yPos<0||u->xPos>=gBmMapSize.x||u->yPos>=gBmMapSize.y||u->state&(US_DEAD|US_HIDDEN|US_NOT_DEPLOYED|US_RESCUED|US_IN_BALLISTA|US_UNDER_A_ROOF|US_BIT16))return -2;
 if(GetPlayerSelectKind(u)!=PLAYER_SELECT_CONTROL)return -3;
 int mov=UNIT_MOV(u);if(mov<0||mov>=MOVE_CMD_MAX_COUNT-1)return -4;
 SyncMaps();UnitBeginAction(u);hasTraded=0;gBmSt.taken_action=0;gActionData.unitActionType=0;selected=index;floodError=0;GenerateUnitMovementMapExt(u,mov);
 if(floodError){u->state&=~US_HIDDEN;selected=0;lastError=floodError;return -5;}
 mode=1;fe8_map_focus(u->xPos,u->yPos);return 0;
}
int fe8_tactical_cost(int x,int y){return mode<0||x<0||y<0||x>=gBmMapSize.x||y>=gBmMapSize.y?-1:movement[y][x];}
#include "constants/characters.h"
struct MenuItemDef;
enum {MENU_ENABLED=1,MENU_DISABLED=2,MENU_NOTSHOWN=3,TILE_COMMAND_SEIZE=0x11,TILE_COMMAND_VISIT=0x10};
extern int IsItemStealable(int item);
static void MakeTargetListForSteal(struct Unit *u);
static int GetSelectTargetCount(void){return tradeCollectCount;}
#include "generated/tactical_steal.h"
static void MakeTargetListForSteal(struct Unit *u){
 // Original adjacent enumeration: bounded native occupancy replaces the map callback.
 static const s8 delta[4][2]={{0,1},{1,0},{-1,0},{0,-1}};
 tradeCollectCount=0;for(int k=0;k<4;k++){int i=fe8_unit_at(u->xPos+delta[k][0],u->yPos+delta[k][1]);if(i>0)AddAsTarget_IfCanStealFrom(GetUnit(i));}
}
int fe8_tactical_can_steal(void){if(mode!=3||!gActiveUnit||fe8_event_busy())return 0;tradeCollectPopulate=0;int availability=StealCommandUsability(NULL,0);return availability==MENU_NOTSHOWN?0:availability;}
int fe8_tactical_begin_steal(void){if(fe8_tactical_can_steal()!=MENU_ENABLED){fe8_audio_se(0x6b);return -1;}tradeCollectPopulate=1;MakeTargetListForSteal(gActiveUnit);targetCount=tradeCollectCount;targetChoice=0;mode=19;fe8_map_focus(targets[0].x,targets[0].y);return 0;}
int fe8_tactical_steal_target(void){if(mode!=19||targetChoice>=targetCount)return -1;stealPeer=targets[targetChoice].index;gActionData.targetIndex=stealPeer;stealRow=0;mode=20;return 0;}
int fe8_tactical_steal_info(int f){return f==0?stealPeer:f==1?stealRow:f==2?lastStolen:f==3?(mode==20?StealItemMenuCommand_Usability(NULL,stealRow):MENU_NOTSHOWN):-1;}
int fe8_tactical_steal_item(int slot){if(mode!=20||slot<0||slot>4||StealItemMenuCommand_Usability(NULL,slot)!=MENU_ENABLED){fe8_audio_se(0x6b);return -1;}int result=fe8_battle_steal(selected,stealPeer,slot);if(result<0)return result;lastStolen=result;fe8_audio_se(0x5a);fe8_map_focus(gActiveUnit->xPos,gActiveUnit->yPos);return FinishAction(6);}
#include "generated/tactical_supply.h"
extern int GetItemAttributes(int item);
extern s8 CanUnitUseWeapon(struct Unit *u,int item);
extern void EquipUnitItemSlot(struct Unit *u,int slot);
#include "generated/tactical_inventory.h"
extern int UnitAddItem(struct Unit *u,int item);
extern void UnitRemoveItem(struct Unit *u,int slot);
int fe8_tactical_can_supply(void){if(mode!=3||!gActiveUnit||fe8_event_busy()||(gPlaySt.chapterIndex!=2&&gPlaySt.chapterIndex!=3&&gPlaySt.chapterIndex!=4&&gPlaySt.chapterIndex!=6&&gPlaySt.chapterIndex!=7))return 0;SyncMaps();return SupplyUsability(NULL,0)==MENU_ENABLED;}
int fe8_tactical_begin_supply(void){if(!fe8_tactical_can_supply())return -1;supplyColumn=supplyRow=supplyChanged=0;gActionData.unitActionType=0x1d;mode=21;return 0;}
int fe8_tactical_supply_info(int f){return f==0?supplyColumn:f==1?supplyRow:f==2?supplyChanged:-1;}
int fe8_tactical_supply_transfer(int column,int row){
 if(mode!=21||!gActiveUnit||column<0||column>1||row<0||row>=(column?100:5))return -1;
 // Original PrepItemSupply Give/Take inventory effects, with preflight capacity
 // checks before mutation. Categorized prep lists/Proc animation are adapted.
 int item=column?fe8_convoy_item(row):gActiveUnit->items[row];if(!item)return -2;
 if(column){if(GetUnitItemCount(gActiveUnit)>=5)return -3;if(fe8_convoy_remove(row)!=item)return -4;UnitAddItem(gActiveUnit,item);}
 else {if(fe8_convoy_count()>=100)return -3;if(fe8_convoy_add(item)<0)return -4;UnitRemoveItem(gActiveUnit,row);}
 supplyChanged=1;gActionData.unitActionType=0x1c;gBmSt.taken_action|=2;supplyColumn=column;int count=column?fe8_convoy_count():GetUnitItemCount(gActiveUnit);supplyRow=row<count?row:count?count-1:0;fe8_audio_se(0x6a);return 0;
}
int fe8_tactical_can_inventory(void){return mode==3&&gActiveUnit&&!fe8_event_busy()&&ItemCommandUsability(NULL,0)==MENU_ENABLED;}
int fe8_tactical_begin_inventory(void){if(!fe8_tactical_can_inventory())return -1;inventoryRow=inventoryAction=0;mode=22;return 0;}
int fe8_tactical_inventory_select(int slot){if(mode!=22||slot<0||slot>=5||!gActiveUnit->items[slot])return -1;inventoryRow=gActionData.itemSlotIndex=slot;inventoryAction=0;mode=23;return 0;}
static int InventoryAvailable(int action){
 if(mode!=23&&mode!=24)return MENU_NOTSHOWN;int item=gActiveUnit->items[inventoryRow];if(!item)return MENU_NOTSHOWN;
 if(action==0){int id=item&255;if(id!=ITEM_VULNERARY&&id!=ITEM_ELIXIR)return MENU_NOTSHOWN;return gActiveUnit->curHP<GetUnitMaxHp(gActiveUnit)?MENU_ENABLED:MENU_DISABLED;}
 if(action==1)return ItemSubMenu_IsEquipAvailable(NULL,0);
 if(action==2){int previous=mode;mode=3;int available=fe8_tactical_can_trade();mode=previous;return available?MENU_ENABLED:MENU_NOTSHOWN;}
 if(action==3)return ItemSubMenu_IsDiscardAvailable(NULL,0);return MENU_NOTSHOWN;
}
int fe8_tactical_inventory_info(int f){return f==0?inventoryRow:f==1?inventoryAction:f==2?inventoryConfirm:f>=3&&f<=6?InventoryAvailable(f-3):f==7?(gActiveUnit?gActiveUnit->items[inventoryRow]:0):-1;}
int fe8_tactical_inventory_action(int action){
 if(mode!=23||InventoryAvailable(action)!=MENU_ENABLED){fe8_audio_se(0x6b);return -1;}
 if(action==0){int result=fe8_recovery_item(selected,inventoryRow);if(result<0)return result;fe8_audio_se(0x6a);mode=3;FinishAction(0x1a);return 0;}
 if(action==1){EquipUnitItemSlot(gActiveUnit,inventoryRow);inventoryRow=0;mode=22;fe8_audio_se(0x6a);return 0;}
 if(action==2){mode=3;return fe8_tactical_begin_trade();}
 inventoryConfirm=1;mode=24;return 0;
}
int fe8_tactical_inventory_discard(int yes){
 if(mode!=24||(yes!=0&&yes!=1))return -1;if(!yes){mode=23;return 0;}
 if(InventoryAvailable(3)!=MENU_ENABLED)return -2;UnitRemoveItem(gActiveUnit,inventoryRow);int count=GetUnitItemCount(gActiveUnit);inventoryRow=inventoryRow<count?inventoryRow:count?count-1:0;mode=count?22:3;if(!count)FindTargets();fe8_audio_se(0x6a);return 0;
}
static int GetAvailableTileEventCommand(s8 x,s8 y){return fe8_event_tile_command(x,y);}
#include "generated/progress_seize.h"
#include "shop.inc.h"
static int IsUnitMagicSealed(struct Unit *u){return u->statusIndex==3;} // Current roster has no magic-seal aura; silence checked here.
#include "generated/progress_visit.h"
#include "constants/terrains.h"
extern int GetItemIndex(int item);
extern int GetItemAfterUse(int item);
static int IsThereClosedChestAt(int x,int y){return fe8_event_tile_command(x,y)==0x14;}
static void StartAvailableDoorTileEvent(int x,int y){if(fe8_event_tile_command(x,y)==0x12)fe8_event_unlock(selected,x,y);}
static void StartAvailableChestTileEvent(int x,int y){if(IsThereClosedChestAt(x,y))fe8_event_unlock(selected,x,y);}
#define PlaySoundEffect fe8_audio_se
#define SONG_B1 0xb1
#include "generated/progress_keys.h"
int fe8_tactical_can_chest(void){if(mode!=3||!gActiveUnit||fe8_event_busy()||gActiveUnit->state&US_HAS_MOVED)return 0;SyncMaps();return GetUnitKeyItemSlotForTerrain(gActiveUnit,TERRAIN_CHEST_FULL)>=0&&CanUnitUseChestKeyItem(gActiveUnit);}
int fe8_tactical_can_door(void){if(mode!=3||!gActiveUnit||fe8_event_busy()||gActiveUnit->state&US_HAS_MOVED||GetUnitKeyItemSlotForTerrain(gActiveUnit,TERRAIN_DOOR)<0)return 0;int x=gActiveUnit->xPos,y=gActiveUnit->yPos;return fe8_event_tile_command(x-1,y)==0x12||fe8_event_tile_command(x+1,y)==0x12||fe8_event_tile_command(x,y-1)==0x12||fe8_event_tile_command(x,y+1)==0x12;}
static int KeyAction(int chest){if(!(chest?fe8_tactical_can_chest():fe8_tactical_can_door()))return -1;gActionData.subjectIndex=selected;gActionData.itemSlotIndex=GetUnitKeyItemSlotForTerrain(gActiveUnit,chest?TERRAIN_CHEST_FULL:TERRAIN_DOOR);ExecKeyItem();eventAction=chest?20:18;mode=8;return 0;}
int fe8_tactical_chest(void){return KeyAction(1);}
int fe8_tactical_door(void){return KeyAction(0);}
int fe8_tactical_npc_key(int index,int slot){if(mode!=7||index<1||index>255||slot<0||slot>=5)return -1;struct Unit *u=GetUnit(index);int id=u->items[slot]&255;if(id!=ITEM_LOCKPICK&&id!=ITEM_DOORKEY&&id!=ITEM_CHESTKEY&&id!=ITEM_CHESTKEY_BUNDLE)return -2;if(id==ITEM_LOCKPICK&&!(UNIT_CATTRIBUTES(u)&CA_THIEF))return -3;int old=selected;selected=index;gActionData.subjectIndex=index;gActionData.itemSlotIndex=slot;ExecKeyItem();selected=old;return 0;}

static int ActionMenu(int *ids,const char **labels);
static int HealTargets(int populate){if(mode!=3||!gActiveUnit||fe8_event_busy()||gActiveUnit->state&US_HAS_MOVED)return 0;int n=0;for(int i=1;i<256;i++)for(int slot=0;slot<5;slot++)if(fe8_battle_can_heal(selected,i,slot)){if(populate)targets[n]=(typeof(*targets)){i,slot};n++;}return n;}
int fe8_tactical_can_heal(void){return HealTargets(0)>0;}
int fe8_tactical_begin_heal(void){int n=HealTargets(1);if(!n)return -1;targetCount=n;targetChoice=0;mode=11;fe8_map_focus(GetUnit(targets[0].index)->xPos,GetUnit(targets[0].index)->yPos);return 0;}
int fe8_tactical_heal(void){if(mode!=11||targetChoice>=targetCount)return -1;int result=fe8_battle_heal(selected,targets[targetChoice].index,targets[targetChoice].slot);if(result<0)return result;fe8_audio_se(0x6a);mode=6;return result;}
static int ItemTargets(int populate){if(mode!=3||!gActiveUnit||fe8_event_busy()||gActiveUnit->state&US_HAS_MOVED)return 0;int n=0;for(int slot=0;slot<5;slot++){int id=gActiveUnit->items[slot]&255;if(!fe8_fog_can_torch(selected,slot)&&((id!=ITEM_VULNERARY&&id!=ITEM_ELIXIR)||gActiveUnit->curHP>=GetUnitMaxHp(gActiveUnit)))continue;if(populate)targets[n]=(typeof(*targets)){selected,slot};n++;}return n;}
int fe8_tactical_can_item(void){return ItemTargets(0)>0;}
int fe8_tactical_begin_item(void){int n=ItemTargets(1);if(!n)return -1;targetCount=n;targetChoice=0;mode=10;return 0;}
int fe8_tactical_use_item(void){if(mode!=10||targetChoice>=targetCount)return -1;int slot=targets[targetChoice].slot;if(fe8_fog_can_torch(selected,slot)){int result=fe8_fog_use_torch(selected,slot);if(result<0)return result;fe8_audio_se(0x6a);mode=3;FinishAction(0x1a);return 1;}if(gActiveUnit->curHP>=GetUnitMaxHp(gActiveUnit))return -1;int result=fe8_recovery_item(selected,targets[targetChoice].slot);if(result<0)return result;fe8_audio_se(0x6a);mode=3;FinishAction(0x1a);return result;}
int fe8_tactical_can_visit(void){if(mode!=3||!gActiveUnit||fe8_event_busy())return 0;SyncMaps();return VisitCommandUsability(NULL,0)==MENU_ENABLED;}
int fe8_tactical_visit(void){if(!fe8_tactical_can_visit())return -1;int result=fe8_event_visit(selected);if(result<0)return result;fe8_audio_se(0x6a);eventAction=16;mode=8;return 0;}
static int TalkTargets(int populate){if(mode!=3||!gActiveUnit||fe8_event_busy()||gActiveUnit->state&US_HAS_MOVED||gActiveUnit->statusIndex==3)return 0;int n=0;for(int y=gBmMapSize.y-1;y>=0;y--)for(int x=gBmMapSize.x-1;x>=0;x--){if(abs(x-gActiveUnit->xPos)+abs(y-gActiveUnit->yPos)!=1)continue;int index=fe8_unit_at(x,y);if(index<0||GetUnit(index)->statusIndex==2||GetUnit(index)->statusIndex==4||!fe8_event_talk_available(selected,index))continue;if(populate)targets[n]=(typeof(*targets)){index,-1};n++;}return n;}
static int RescueTargets(int populate,int drop){if(mode!=3||!gActiveUnit||fe8_event_busy()||gActiveUnit->state&US_HAS_MOVED)return 0;if(drop){if(!(gActiveUnit->state&US_RESCUING)||!UNIT_IS_VALID(GetUnit(gActiveUnit->rescue)))return 0;}else if(gActiveUnit->state&(US_RESCUING|US_IN_BALLISTA))return 0;SyncMaps();gSubjectUnit=gActiveUnit;tradeCollectCount=0;tradeCollectPopulate=populate;for(int y=gBmMapSize.y-1;y>=0;y--)for(int x=gBmMapSize.x-1;x>=0;x--){if(abs(x-gActiveUnit->xPos)+abs(y-gActiveUnit->yPos)!=1)continue;if(drop)TryAddToDropTargetList(x,y);else{int index=fe8_unit_at(x,y);if(index>0)TryAddUnitToRescueTargetList(GetUnit(index));}}return tradeCollectCount;}
int fe8_tactical_can_rescue(void){return RescueTargets(0,0)>0;}
int fe8_tactical_can_drop(void){return RescueTargets(0,1)>0;}
int fe8_tactical_begin_rescue(void){int n=RescueTargets(1,0);if(!n)return -1;targetCount=n;targetChoice=0;mode=14;fe8_map_focus(targets[0].x,targets[0].y);return 0;}
int fe8_tactical_begin_drop(void){int n=RescueTargets(1,1);if(!n)return -1;targetCount=n;targetChoice=0;mode=15;fe8_map_focus(targets[0].x,targets[0].y);return 0;}
int fe8_tactical_rescue(void){if(mode!=14||targetChoice>=targetCount)return -1;int target=targets[targetChoice].index;struct Unit *u=GetUnit(target);if(!UNIT_IS_VALID(u)||!CanUnitRescue(gActiveUnit,u))return -2;UnitRescue(gActiveUnit,u);fe8_unit_sync_position(target);fe8_audio_se(0x6a);mode=3;return FinishAction(9);}
int fe8_tactical_drop(void){if(mode!=15||targetChoice>=targetCount)return -1;int x=targets[targetChoice].x,y=targets[targetChoice].y,index=gActiveUnit->rescue;SyncMaps();if(occupancy[y][x]||!CanUnitCrossTerrain(GetUnit(index),terrain[y][x]))return -2;UnitDrop(gActiveUnit,x,y);fe8_unit_sync_position(index);fe8_audio_se(0x6a);mode=3;return FinishAction(10);}
int fe8_tactical_rescue_death(int actor,int target){struct Unit *old=gActiveUnit;int ids[2]={actor,target};gActiveUnit=GetUnit(actor);for(int side=0;side<2;side++){struct Unit *u=GetUnit(ids[side]);if(!UNIT_IS_VALID(u)||u->curHP||!(u->state&US_RESCUING))continue;int passenger=u->rescue;if(!UNIT_IS_VALID(GetUnit(passenger))){gActiveUnit=old;return -1;}SyncMaps();int x=-1,y=-1;floodError=0;UnitGetDeathDropLocation(u,&x,&y);if(floodError||x<0||y<0){gActiveUnit=old;return -2;}UnitDrop(u,x,y);fe8_unit_sync_position(passenger);fe8_audio_se(0xac);}gActiveUnit=old;SyncMaps();return 0;}
static int TransferTargets(int populate,int give){if(mode!=3||!gActiveUnit||fe8_event_busy()||gActiveUnit->state&US_HAS_MOVED||gBmSt.taken_action&4)return 0;if(give){if(!(gActiveUnit->state&US_RESCUING))return 0;}else if(gActiveUnit->state&US_RESCUING)return 0;SyncMaps();gSubjectUnit=gActiveUnit;tradeCollectCount=0;tradeCollectPopulate=populate;for(int y=gBmMapSize.y-1;y>=0;y--)for(int x=gBmMapSize.x-1;x>=0;x--){if(abs(x-gActiveUnit->xPos)+abs(y-gActiveUnit->yPos)!=1)continue;int index=fe8_unit_at(x,y);if(index<0)continue;if(give)TryAddUnitToGiveTargetList(GetUnit(index));else TryAddRescuedUnitToTakeTargetList(GetUnit(index));}return tradeCollectCount;}
int fe8_tactical_can_take(void){return TransferTargets(0,0)>0;}
int fe8_tactical_can_give(void){return TransferTargets(0,1)>0;}
int fe8_tactical_begin_take(void){int n=TransferTargets(1,0);if(!n)return -1;targetCount=n;targetChoice=0;mode=17;fe8_map_focus(targets[0].x,targets[0].y);return 0;}
int fe8_tactical_begin_give(void){int n=TransferTargets(1,1);if(!n)return -1;targetCount=n;targetChoice=0;mode=18;fe8_map_focus(targets[0].x,targets[0].y);return 0;}
static int TransferPassenger(int give){if(mode!=(give?18:17)||targetChoice>=targetCount)return -1;struct Unit *peer=GetUnit(targets[targetChoice].index),*from=give?gActiveUnit:peer,*to=give?peer:gActiveUnit;if(!UNIT_IS_VALID(from)||!UNIT_IS_VALID(to)||!(from->state&US_RESCUING)||to->state&US_RESCUING||!IsSameAllegiance(from->index,to->index)||abs(from->xPos-to->xPos)+abs(from->yPos-to->yPos)!=1)return -2;int passenger=from->rescue;if(!CanUnitRescue(to,GetUnit(passenger)))return -3;UnitGive(from,to);fe8_unit_sync_position(passenger);gActionData.unitActionType=give?12:11;gBmSt.taken_action|=4;mode=3;menuChoice=0;FindTargets();fe8_audio_se(0x6a);return 0;}
int fe8_tactical_take(void){return TransferPassenger(0);}
int fe8_tactical_give(void){return TransferPassenger(1);}
static int TradeTargets(int populate){if(mode!=3||!gActiveUnit||fe8_event_busy()||gActiveUnit->state&US_HAS_MOVED)return 0;gSubjectUnit=gActiveUnit;tradeCollectCount=0;tradeCollectPopulate=populate;for(int y=gBmMapSize.y-1;y>=0;y--)for(int x=gBmMapSize.x-1;x>=0;x--){if(abs(x-gActiveUnit->xPos)+abs(y-gActiveUnit->yPos)!=1)continue;int index=fe8_unit_at(x,y);if(index<0)continue;TryAddUnitToTradeTargetList(GetUnit(index));}if(gActiveUnit->state&US_RESCUING)TryAddUnitToTradeTargetList(GetUnit(gActiveUnit->rescue));return tradeCollectCount;}
int fe8_tactical_can_trade(void){return TradeTargets(0)>0;}
int fe8_tactical_begin_trade(void){int n=TradeTargets(1);if(!n)return -1;targetCount=n;targetChoice=0;mode=12;return 0;}
int fe8_tactical_trade_target(void){if(mode!=12||targetChoice>=targetCount)return -1;tradePeer=targets[targetChoice].index;tradeColumn=tradeRow=tradeChosenRow=0;tradeChosenColumn=-1;mode=13;return 0;}
int fe8_tactical_trade_info(int f){return f==0?tradePeer:f==1?tradeColumn:f==2?tradeRow:f==3?tradeChosenColumn:f==4?tradeChosenRow:f==5?hasTraded:-1;}
int fe8_tactical_trade_swap(int a,int b){if(mode!=13||a<0||a>=5||b<0||b>=5)return -1;struct Unit *peer=GetUnit(tradePeer);if(!UNIT_IS_VALID(peer)||!gActiveUnit||!IsSameAllegiance(selected,tradePeer)||(abs(peer->xPos-gActiveUnit->xPos)+abs(peer->yPos-gActiveUnit->yPos)!=1&&(!(gActiveUnit->state&US_RESCUING)||gActiveUnit->rescue!=tradePeer)))return -2;if(!gActiveUnit->items[a]&&!peer->items[b])return -3;struct TradeMenuProc proc={.units={gActiveUnit,peer},.hoverColumn=1,.hoverRow=b,.selectedColumn=0,.selectedRow=a};TradeMenu_ApplyItemSwap(&proc);hasTraded=proc.hasTraded;gBmSt.taken_action|=1;tradeChosenColumn=-1;tradeColumn=tradeRow=0;fe8_audio_se(0x6a);return 0;}
int fe8_tactical_trade_choose(int column,int row){if(mode!=13||column<0||column>1||row<0||row>=5)return -1;struct Unit *u=column?GetUnit(tradePeer):gActiveUnit;int count=GetUnitItemCount(u);if(row>count||tradeChosenColumn<0&&!u->items[row])return -2;tradeColumn=column;tradeRow=row;if(tradeChosenColumn<0){tradeChosenColumn=column;tradeChosenRow=row;tradeColumn=column^1;tradeRow=0;return 0;}if(column==tradeChosenColumn){tradeChosenRow=row;return 0;}return fe8_tactical_trade_swap(column?tradeChosenRow:row,column?row:tradeChosenRow);}
int fe8_tactical_can_talk(void){return TalkTargets(0)>0;}
int fe8_tactical_begin_talk(void){int n=TalkTargets(1);if(!n)return -1;targetCount=n;targetChoice=0;mode=9;fe8_map_focus(GetUnit(targets[0].index)->xPos,GetUnit(targets[0].index)->yPos);return 0;}
int fe8_tactical_talk(void){if(mode!=9||targetChoice>=targetCount)return -1;int result=fe8_event_talk(selected,targets[targetChoice].index);if(result<0)return result;fe8_audio_se(0x6a);eventAction=14;mode=8;return 0;}
int fe8_tactical_can_seize(void){return mode==3&&gActiveUnit&&(gPlaySt.chapterIndex==1||gPlaySt.chapterIndex==3||gPlaySt.chapterIndex==5)&&gPlaySt.chapterModeIndex>=1&&gPlaySt.chapterModeIndex<=3&&!fe8_event_busy()&&UnitActionMenu_CanSeize(NULL,0)==MENU_ENABLED;}
int fe8_tactical_seize(void){if(!fe8_tactical_can_seize())return -1;struct Unit *u=gActiveUnit;if(fe8_event_seize(selected)<0)return -2;u->state|=US_HAS_MOVED|US_UNSELECTABLE;fe8_audio_se(0x6a);mode=0;selected=0;gActiveUnit=NULL;return 0;}
static void FindTargets(void);
int fe8_tactical_move(int x,int y){
 if((mode!=1&&mode!=16)||x<0||y<0||x>=gBmMapSize.x||y>=gBmMapSize.y||!CanMoveActiveUnitTo(x,y))return -1;
 u8 path[MOVE_CMD_MAX_COUNT];floodError=0;GenerateBestMovementScript(x,y,path);if(floodError)return -2;
 u32 queue[128];int px=gActiveUnitMoveOrigin.x,py=gActiveUnitMoveOrigin.y,n=0;
 for(;n<MOVE_CMD_MAX_COUNT-1&&path[n]!=MOVE_CMD_HALT;n++){switch(path[n]){case 0:px--;break;case 1:px++;break;case 2:py++;break;case 3:py--;break;default:return -3;}queue[n*2]=px|(py<<6);queue[n*2+1]=0;}
 if(px!=x||py!=y)return -6;
 fogBump=0;int bx=gActiveUnitMoveOrigin.x,by=gActiveUnitMoveOrigin.y;for(int k=0;k<n;k++){int nx=queue[k*2]&63,ny=queue[k*2]>>6&63;if(hiddenMap[ny][nx]&HIDDEN_BIT_UNIT){n=k;x=bx;y=by;fogBump=1;break;}bx=nx;by=ny;}
 gActionData.moveCount=movement[y][x];menuChoice=0;
 if(fogBump&&!n){FinishAction(UNIT_ACTION_WAIT);fogBump=0;}else if(n){if(fe8_unit_move_queue_index((u8)gActiveUnit->index,queue,n*2)<0)return -4;mode=2;}else if(cantoActive){FinishAction(UNIT_ACTION_WAIT);}else{mode=3;FindTargets();}
 fe8_map_focus(x,y);return 0;
}
static void FindTargets(void){
 targetCount=targetChoice=0;if(!gActiveUnit)return;
 for(int i=1;i<256;i++)for(int slot=0;slot<5;slot++)if(fe8_battle_forecast(selected,i,slot,4)>=0){targets[targetCount++]=(typeof(*targets)){i,slot,GetUnit(i)->xPos,GetUnit(i)->yPos};}
 for(int y=0;y<gBmMapSize.y;y++)for(int x=0;x<gBmMapSize.x;x++)if(fe8_map_obstacle_hp(x,y)>0)for(int slot=0;slot<5;slot++)if(fe8_battle_obstacle_forecast(selected,x,y,slot,4)>=0)targets[targetCount++]=(typeof(*targets)){0,slot,x,y};
}
int fe8_tactical_begin_attack(void){if(mode!=3)return -1;FindTargets();if(!targetCount)return -2;mode=5;menuChoice=0;fe8_map_focus(targets[0].x,targets[0].y);return 0;}
int fe8_tactical_next_target(void){if((mode!=5&&mode!=9&&mode!=10&&mode!=11&&mode!=12&&mode!=14&&mode!=15&&mode!=17&&mode!=18&&mode!=19)||!targetCount)return -1;targetChoice=(targetChoice+1)%targetCount;if(mode==15||mode==5)fe8_map_focus(targets[targetChoice].x,targets[targetChoice].y);else{struct Unit *u=GetUnit(targets[targetChoice].index);fe8_map_focus(u->xPos,u->yPos);}return 0;}
int fe8_tactical_attack(void){if(mode!=5||targetChoice>=targetCount)return -1;int result=targets[targetChoice].index?fe8_battle_attack(selected,targets[targetChoice].index,targets[targetChoice].slot):fe8_battle_obstacle_attack(selected,targets[targetChoice].x,targets[targetChoice].y,targets[targetChoice].slot);if(result<0){lastError=10-result;return result;}mode=6;return result;}
int fe8_tactical_forecast(int field){if(mode!=5||targetChoice>=targetCount)return -1;return targets[targetChoice].index?fe8_battle_forecast(selected,targets[targetChoice].index,targets[targetChoice].slot,field):fe8_battle_obstacle_forecast(selected,targets[targetChoice].x,targets[targetChoice].y,targets[targetChoice].slot,field);}
int fe8_tactical_cancel(void){
 if(mode==16){cantoActive=0;return FinishAction(UNIT_ACTION_WAIT);}
 if(mode==25)return CloseNativeShop();
 if(mode==26||mode==27){mode=25;shopError=0;return 0;}
 if(mode==24){mode=23;return 0;}
 if(mode==23){mode=22;return 0;}
 if(mode==22){mode=3;menuChoice=0;FindTargets();return 0;}
 if(mode==21){mode=3;menuChoice=0;FindTargets();return 0;}
 if(mode==20){mode=19;return 0;}
 if(mode==13){if(tradeChosenColumn>=0){tradeChosenColumn=-1;return 0;}mode=3;menuChoice=0;FindTargets();return 0;}
 if(mode==5||mode==9||mode==10||mode==11||mode==12||mode==14||mode==15||mode==17||mode==18||mode==19){mode=3;menuChoice=0;FindTargets();fe8_map_focus(gActiveUnit->xPos,gActiveUnit->yPos);return 0;}

 if(mode==3&&gBmSt.taken_action)return FinishAction(0x1f);
 if(mode==3){fe8_unit_move_index((u8)gActiveUnit->index,gActiveUnitMoveOrigin.x,gActiveUnitMoveOrigin.y);while(fe8_units_busy())fe8_units_tick();mode=1;fe8_map_focus(gActiveUnitMoveOrigin.x,gActiveUnitMoveOrigin.y);return 0;}
 if(mode==1){gActiveUnit->state&=~US_HIDDEN;mode=0;selected=0;return 0;}
 if(mode==4){mode=0;return 0;}return -1;
}
int fe8_tactical_end_turn(void){if(fe8_gameover_active()||mode!=0&&mode!=4)return -1;fe8_ai_reset();ClearActiveFactionGrayedStates();SwitchPhases();fe8_recovery_phase(gPlaySt.faction);selected=0;gActiveUnit=NULL;mode=7;fe8_event_phase();return 0;}
static int FinishAction(int action){if(!gActiveUnit)return -1;gActionData.unitActionType=action;MoveActiveUnit(gActiveUnit->xPos,gActiveUnit->yPos);fe8_event_actor(selected);fe8_event_post_action();SyncMaps();if(!cantoActive&&TryMakeCantoUnit(NULL)){cantoActive=1;floodError=0;GenerateUnitMovementMapExt(gActiveUnit,UNIT_MOV(gActiveUnit)-gActionData.moveCount);if(!floodError){mode=16;fe8_map_focus(gActiveUnit->xPos,gActiveUnit->yPos);return 0;}lastError=floodError;}cantoActive=0;gActiveUnit->state|=US_HAS_MOVED;mode=0;selected=0;gActiveUnit=NULL;if(!GetPhaseAbleUnitCount(FACTION_BLUE))fe8_tactical_end_turn();return 0;}
int fe8_tactical_wait(void){return mode==3?FinishAction(UNIT_ACTION_WAIT):-1;}
void fe8_tactical_tick(int keys){
 if(mode<0||fe8_event_busy())return;
 if(mode==8){mode=3;FinishAction(eventAction);return;}
 if(mode==2){if(!fe8_units_busy()){if(cantoActive||fogBump){fogBump=0;FinishAction(UNIT_ACTION_WAIT);}else{mode=3;FindTargets();}}return;}
 if(mode==6){if(!fe8_battle_busy()){mode=3;FinishAction(fe8_battle_info(18)==2?UNIT_ACTION_STAFF:UNIT_ACTION_COMBAT);}return;}
 if(mode>=25&&mode<=27){NativeShopTick(keys);return;}
 if(mode==24){if(keys&2){fe8_tactical_inventory_discard(0);return;}if(keys&1){fe8_tactical_inventory_discard(!inventoryConfirm);return;}if(keys&240)inventoryConfirm^=1;return;}
 if(mode==23){int ids[5];const char *labels[5];int count=InventoryMenu(ids,labels);if(keys&2){fe8_tactical_cancel();return;}if(keys&1){if(ids[inventoryAction]==4)fe8_tactical_cancel();else fe8_tactical_inventory_action(ids[inventoryAction]);return;}if(keys&192)inventoryAction=(inventoryAction+((keys&128)?1:count-1))%count;return;}
 if(mode==22){if(keys&2){fe8_tactical_cancel();return;}if(keys&1){fe8_tactical_inventory_select(inventoryRow);return;}int rows=GetUnitItemCount(gActiveUnit);if(rows&&(keys&192))inventoryRow=(inventoryRow+((keys&128)?1:rows-1))%rows;return;}
 if(mode==21){if(keys&2){fe8_tactical_cancel();return;}if(keys&1){if(fe8_tactical_supply_transfer(supplyColumn,supplyRow)<0)fe8_audio_se(0x6b);return;}if(keys&48){supplyColumn^=1;supplyRow=0;}int rows=supplyColumn?fe8_convoy_count():GetUnitItemCount(gActiveUnit);if(rows&&(keys&192))supplyRow=(supplyRow+((keys&128)?1:rows-1))%rows;return;}
 if(mode==20){if(keys&2){fe8_tactical_cancel();return;}if(keys&1){fe8_tactical_steal_item(stealRow);return;}int rows=GetUnitItemCount(GetUnit(stealPeer));if(rows&&(keys&192))stealRow=(stealRow+((keys&128)?1:rows-1))%rows;return;}
 if(mode==13){if(keys&2){fe8_tactical_cancel();return;}if(keys&1){fe8_tactical_trade_choose(tradeColumn,tradeRow);return;}if(keys&48){if(tradeChosenColumn<0){tradeColumn^=1;int count=GetUnitItemCount(tradeColumn?GetUnit(tradePeer):gActiveUnit);if(tradeRow>=count)tradeRow=count?count-1:0;}}if(keys&192){struct Unit *u=tradeColumn?GetUnit(tradePeer):gActiveUnit;int count=GetUnitItemCount(u),rows=tradeChosenColumn<0?count:count<5?count+1:5;if(rows>0)tradeRow=(tradeRow+((keys&128)?1:rows-1))%rows;}return;}
 if(mode==5||mode==9||mode==10||mode==11||mode==12||mode==14||mode==15||mode==17||mode==18||mode==19){if(keys&2){fe8_tactical_cancel();return;}if(keys&1){if(mode==9)fe8_tactical_talk();else if(mode==10)fe8_tactical_use_item();else if(mode==11)fe8_tactical_heal();else if(mode==12)fe8_tactical_trade_target();else if(mode==14)fe8_tactical_rescue();else if(mode==15)fe8_tactical_drop();else if(mode==17)fe8_tactical_take();else if(mode==18)fe8_tactical_give();else if(mode==19)fe8_tactical_steal_target();else fe8_tactical_attack();return;}if(keys&240){targetChoice=(targetChoice+((keys&80)?1:targetCount-1))%targetCount;if(mode==15||mode==5)fe8_map_focus(targets[targetChoice].x,targets[targetChoice].y);else{struct Unit *u=GetUnit(targets[targetChoice].index);fe8_map_focus(u->xPos,u->yPos);}}return;}
 if(mode==7){if(fe8_event_info(12))return;fe8_audio_map_start();if(gPlaySt.faction==FACTION_BLUE){mode=0;return;}if(fe8_ai_info(0)<0){fe8_ai_prepare();return;}if(fe8_ai_tick()==1){ClearActiveFactionGrayedStates();SwitchPhases();fe8_recovery_phase(gPlaySt.faction);fe8_ai_reset();fe8_event_phase();}return;}
 if(keys&2){fe8_tactical_cancel();return;}
 if(mode==3){int ids[20];const char *labels[20];int count=ActionMenu(ids,labels);if(menuChoice>=count)menuChoice=0;if(keys&1){switch(ids[menuChoice]){case 18:fe8_tactical_begin_shop();break;case 17:fe8_tactical_begin_supply();break;case 16:fe8_tactical_begin_steal();break;case 14:fe8_tactical_chest();break;case 15:fe8_tactical_door();break;case 1:fe8_tactical_begin_attack();break;case 2:fe8_tactical_begin_talk();break;case 3:fe8_tactical_visit();break;case 4:fe8_tactical_seize();break;case 7:fe8_tactical_begin_heal();break;case 8:fe8_tactical_begin_inventory();break;case 9:fe8_tactical_begin_trade();break;case 10:fe8_tactical_begin_rescue();break;case 11:fe8_tactical_begin_drop();break;case 12:fe8_tactical_begin_take();break;case 13:fe8_tactical_begin_give();break;case 5:fe8_tactical_wait();break;default:fe8_tactical_cancel();break;}}else if(keys&192)menuChoice=(menuChoice+((keys&128)?1:count-1))%count;return;}
 if(mode==4){if(keys&1){if(menuChoice)fe8_tactical_cancel();else fe8_tactical_end_turn();}else if(keys&192)menuChoice^=1;return;}
 if(keys&1){int x=fe8_map_info(3),y=fe8_map_info(4);if(mode==1||mode==16)fe8_tactical_move(x,y);else{int index=fe8_unit_at(x,y);if(index>0&&GetPlayerSelectKind(GetUnit(index))==PLAYER_SELECT_CONTROL)fe8_tactical_select(index);else{mode=4;menuChoice=0;}}}
}
int fe8_tactical_consumes_back(void){return mode>=1&&mode<=27;}
int fe8_tactical_locks_cursor(void){return mode==2||mode==3||mode==4||mode==5||mode==6||mode==7||mode==8||mode==9||mode==10||mode==11||mode==12||mode==13||mode==14||mode==15||mode==17||mode==18||mode==19||mode==20||mode==21||mode==22||mode==23||mode==24||mode==25||mode==26||mode==27;}
int fe8_tactical_show_active(int index){return selected==index&&mode>=1&&mode<=27;}
int fe8_tactical_info(int field){switch(field){case 0:return mode;case 1:return selected;case 2:return gPlaySt.faction;case 3:return gPlaySt.chapterTurnNumber;case 4:return menuChoice;case 5:return lastError;case 6:return GetPhaseAbleUnitCount(gPlaySt.faction);case 7:return gActionData.moveCount;case 8:return targetCount;case 9:return mode==5||mode==9||mode==10||mode==11||mode==12||mode==14||mode==17||mode==18||mode==19?targets[targetChoice].index:0;case 16:return cantoActive;case 14:return mode==15||mode==5?targets[targetChoice].x:-1;case 15:return mode==15||mode==5?targets[targetChoice].y:-1;case 10:return mode==5||mode==10||mode==11||mode==12||mode==14||mode==17||mode==18||mode==19?targets[targetChoice].slot:-1;default:return -1;}}
void fe8_tactical_render_range(u8 *rgba,int cameraX,int cameraY,int frame){
 if(mode!=1&&mode!=16)return;int phase=frame/2&31;
 for(int y=0;y<160;y++)for(int x=0;x<240;x++){
  int wx=x+cameraX,wy=y+cameraY,bx=wx>>4,by=wy>>4;if(bx>=gBmMapSize.x||by>=gBmMapSize.y||movement[by][bx]>=MAP_MOVEMENT_MAX)continue;
  int dx=wx&15,dy=wy&15,pos=((dy/8)*2+dx/8)*32+(dy&7)*4+(dx&7)/2,index=gUnknown_08A02EB4[pos]>>((dx&1)*4)&15;if(!index)continue;
  int color=gUnknown_08A02F34[phase+index-1];u8 *p=rgba+(y*240+x)*4;
  for(int c=0;c<3;c++){int v=(((color>>(c*5))&31)*10+(p[c]>>3)*6)>>4;if(v>31)v=31;p[c]=(v<<3)|(v>>2);}
 }
}

static void MenuPixel(u8 *rgba,int x,int y,int color){if(x<0||y<0||x>=240||y>=160)return;u8 *p=rgba+(y*240+x)*4;for(int c=0;c<3;c++){int v=color>>(c*5)&31;p[c]=(v<<3)|(v>>2);}p[3]=255;}
static void MenuText(u8 *rgba,const char *s,int x,int y,int highlight){for(;*s;s++){int code=(u8)*s;for(int dy=0;dy<16;dy++)for(int dx=0;dx<16;dx++){int ink=menuGlyphs[code].rows[dy]>>(dx*2)&3;if(ink)MenuPixel(rgba,x+dx,y+dy,menuTextPalette[ink+(highlight?9:0)]);}x+=menuGlyphs[code].width;}}
static int InventoryMenu(int *ids,const char **labels){const char *names[]={tacticalUse,tacticalEquip,tacticalTrade,tacticalDiscard};int n=0;for(int action=0;action<4;action++)if(InventoryAvailable(action)!=MENU_NOTSHOWN){ids[n]=action;labels[n++]=names[action];}ids[n]=4;labels[n++]=tacticalCancel;return n;}
static void MenuFrame(u8 *rgba,int tx,int ty,int width,int height){u16 map[1024]={0};DrawUiFrame(map,tx,ty,width,height,0,0);for(int y=ty*8;y<(ty+height)*8;y++)for(int x=tx*8;x<(tx+width)*8;x++){int ref=map[(y/8)*32+x/8],dx=ref&1024?7-(x&7):x&7,dy=ref&2048?7-(y&7):y&7,pos=(ref&1023)*32+dy*4+dx/2;if(pos>=sizeof(gUiFrameImage))continue;int ink=gUiFrameImage[pos]>>((dx&1)*4)&15;if(ink)MenuPixel(rgba,x,y,gUiFramePaletteA[ink]);}}
static int ActionMenu(int *ids,const char **labels){int n=0;
 #define ADD_ACTION(id,text) do{ids[n]=id;labels[n++]=text;}while(0)
 if(fe8_tactical_can_shop())ADD_ACTION(18,fe8_event_tile_command(gActiveUnit->xPos,gActiveUnit->yPos)==0x17?shopVendorText:shopArmoryText);if(fe8_tactical_can_supply())ADD_ACTION(17,tacticalSupply);if(fe8_tactical_can_steal())ADD_ACTION(16,tacticalSteal);if(fe8_tactical_can_chest())ADD_ACTION(14,tacticalChest);if(fe8_tactical_can_door())ADD_ACTION(15,tacticalDoor);if(targetCount)ADD_ACTION(1,tacticalAttack);if(fe8_tactical_can_heal())ADD_ACTION(7,tacticalStaff);if(fe8_tactical_can_inventory())ADD_ACTION(8,tacticalItem);if(fe8_tactical_can_trade())ADD_ACTION(9,tacticalTrade);if(fe8_tactical_can_rescue())ADD_ACTION(10,tacticalRescue);if(fe8_tactical_can_drop())ADD_ACTION(11,tacticalDrop);if(fe8_tactical_can_take())ADD_ACTION(12,tacticalTake);if(fe8_tactical_can_give())ADD_ACTION(13,tacticalGive);if(fe8_tactical_can_talk())ADD_ACTION(2,tacticalTalk);if(fe8_tactical_can_visit())ADD_ACTION(3,tacticalVisit);if(fe8_tactical_can_seize())ADD_ACTION(4,tacticalSeize);ADD_ACTION(5,tacticalWait);ADD_ACTION(6,tacticalCancel);
 #undef ADD_ACTION
 return n;
}
void fe8_tactical_render_menu(u8 *rgba){
 if(mode>=25&&mode<=27){MenuFrame(rgba,1,1,28,18);MenuText(rgba,shopType?shopVendorText:shopArmoryText,20,12,0);char gold[24];snprintf(gold,sizeof(gold),"%u G",GetPartyGoldAmount());MenuText(rgba,gold,154,12,0);MenuText(rgba,shopBuyText,24,30,!shopColumn);MenuText(rgba,shopSellText,130,30,shopColumn);int count=shopColumn?GetUnitItemCount(gActiveUnit):nativeShop.shopItemCount,first=shopRow>=6?shopRow-5:0;for(int row=0;row<6&&row+first<count;row++){int actual=row+first,item=fe8_tactical_shop_item(shopColumn,actual);MenuText(rgba,actual==shopRow?">":"",12,50+row*15,actual==shopRow);MenuText(rgba,tacticalItemNames[item&255],24,50+row*15,actual==shopRow);char price[12];snprintf(price,sizeof(price),"%d",fe8_tactical_shop_price(shopColumn,actual));MenuText(rgba,price,177,50+row*15,actual==shopRow);}if(mode!=25){MenuFrame(rgba,9,5,20,9);MenuText(rgba,mode==27?tacticalSupply:shopColumn?shopSellText:shopBuyText,83,44,0);MenuText(rgba,tacticalYes,90,68,!shopConfirm);MenuText(rgba,tacticalNo,90,85,shopConfirm);}return;}

 if(mode>=22&&mode<=24){MenuFrame(rgba,1,2,19,14);MenuText(rgba,tacticalItem,20,22,0);for(int row=0;row<GetUnitItemCount(gActiveUnit);row++){int item=gActiveUnit->items[row];if(row==inventoryRow)MenuText(rgba,">",10,44+row*16,1);MenuText(rgba,tacticalItemNames[item&255],20,44+row*16,row==inventoryRow);char uses[8];snprintf(uses,sizeof(uses),"%d",item>>8);MenuText(rgba,uses,138,44+row*16,row==inventoryRow);}if(mode==23){int ids[5];const char *labels[5];int n=InventoryMenu(ids,labels);MenuFrame(rgba,20,2,9,n*2+2);for(int row=0;row<n;row++)MenuText(rgba,labels[row],174,24+row*16,row==inventoryAction);}else if(mode==24){MenuFrame(rgba,20,2,9,8);MenuText(rgba,tacticalDiscard,174,24,0);MenuText(rgba,tacticalYes,174,44,!inventoryConfirm);MenuText(rgba,tacticalNo,174,60,inventoryConfirm);}return;}
 if(mode==21){u16 map[1024]={0};DrawUiFrame(map,1,1,14,18,0,0);DrawUiFrame(map,15,1,14,18,0,0);for(int y=8;y<152;y++)for(int x=8;x<232;x++){int ref=map[(y/8)*32+x/8],dx=ref&1024?7-(x&7):x&7,dy=ref&2048?7-(y&7):y&7,pos=(ref&1023)*32+dy*4+dx/2;if(pos>=sizeof(gUiFrameImage))continue;int ink=gUiFrameImage[pos]>>((dx&1)*4)&15;if(ink)MenuPixel(rgba,x,y,gUiFramePaletteA[ink]);}MenuText(rgba,tacticalItem,20,14,0);char heading[24];snprintf(heading,sizeof(heading),"%s %d/100",tacticalSupply,fe8_convoy_count());MenuText(rgba,heading,132,14,0);for(int col=0;col<2;col++){int count=col?fe8_convoy_count():GetUnitItemCount(gActiveUnit),first=col&&supplyColumn&&supplyRow>=7?supplyRow-6:0;for(int row=0;row<7&&row+first<count;row++){int actual=row+first,item=col?fe8_convoy_item(actual):gActiveUnit->items[actual];if(supplyColumn==col&&supplyRow==actual)MenuText(rgba,">",10+col*112,34+row*16,1);MenuText(rgba,tacticalItemNames[item&255],20+col*112,34+row*16,supplyColumn==col&&supplyRow==actual);}}return;}
 if(mode==20){u16 map[1024]={0};DrawUiFrame(map,3,2,24,14,0,0);for(int y=16;y<128;y++)for(int x=24;x<216;x++){int ref=map[(y/8)*32+x/8],dx=ref&1024?7-(x&7):x&7,dy=ref&2048?7-(y&7):y&7,pos=(ref&1023)*32+dy*4+dx/2;if(pos>=sizeof(gUiFrameImage))continue;int ink=gUiFrameImage[pos]>>((dx&1)*4)&15;if(ink)MenuPixel(rgba,x,y,gUiFramePaletteA[ink]);}MenuText(rgba,tacticalSteal,40,22,0);struct Unit *peer=GetUnit(stealPeer);for(int row=0;row<5;row++){int item=peer->items[row];if(!item)break;MenuText(rgba,row==stealRow?">":IsItemStealable(item)?"":"x",30,42+row*16,row==stealRow);MenuText(rgba,tacticalItemNames[item&255],42,42+row*16,row==stealRow);char uses[8];snprintf(uses,sizeof(uses),"%d",item>>8);MenuText(rgba,uses,186,42+row*16,row==stealRow);}return;}
 if(mode!=3&&mode!=4)return;int ids[20];const char *labels[20];int rows=mode==3?ActionMenu(ids,labels):2,visible=rows>8?8:rows,first=menuChoice>=visible?menuChoice-visible+1:0,height=visible*2+2;u16 map[1024]={0};DrawUiFrame(map,20,1,9,height,0,0);
 for(int y=8;y<8+height*8;y++)for(int x=160;x<232;x++){int ref=map[(y/8)*32+x/8],dx=ref&1024?7-(x&7):x&7,dy=ref&2048?7-(y&7):y&7,pos=(ref&1023)*32+dy*4+dx/2;if(pos>=sizeof(gUiFrameImage))continue;int index=gUiFrameImage[pos]>>((dx&1)*4)&15;if(index)MenuPixel(rgba,x,y,gUiFramePaletteA[index]);}
 if(mode==3){for(int row=0;row<visible;row++)MenuText(rgba,labels[first+row],174,16+row*16,menuChoice==first+row);}else{MenuText(rgba,tacticalEnd,174,16,!menuChoice);MenuText(rgba,tacticalCancel,174,32,menuChoice);}
}

const s8 *fe8_tactical_movement_cost(struct Unit *u){return GetUnitMovementCost(u);}
int fe8_tactical_probe_movement(struct Unit *u,int x,int y,int limit,int id,u8 *out){return fe8_tactical_probe_costs(u,x,y,limit,id,out,NULL);}
int fe8_tactical_probe_costs(struct Unit *u,int x,int y,int limit,int id,u8 *out,const s8 *override){
 if(!u||!UNIT_IS_VALID(u)||!out||!fe8_map_active()||limit<0||limit>124||x<0||y<0||x>=fe8_map_info(1)||y>=fe8_map_info(2))return -1;
 u8 backup[4096],costs[65];memcpy(backup,movement,4096);memcpy(costs,gWorkingTerrainMoveCosts,65);u8 **working=gWorkingBmMap;struct MovMapFillState fill=gMovMapFillState;int oldError=floodError;
 SyncMaps();SetWorkingMoveCosts(override?override:GetUnitMovementCost(u));SetWorkingBmMap(gBmMapMovement);floodError=0;GenerateMovementMap(x,y,limit,id);int result=floodError?-2:0;if(!result)memcpy(out,movement,4096);
 memcpy(movement,backup,4096);memcpy(gWorkingTerrainMoveCosts,costs,65);gWorkingBmMap=working;gMovMapFillState=fill;floodError=oldError;return result;
}

int fe8_tactical_npc_move(int index,int x,int y){
 if(mode!=7||index<1||index>255)return -1;struct Unit *u=GetUnit(index);SyncMaps();if(!UNIT_IS_VALID(u)||x<0||y<0||x>=gBmMapSize.x||y>=gBmMapSize.y)return -2;
 struct Unit *old=gActiveUnit;u8 oldId=gActiveUnitId;struct Vec2 origin=gActiveUnitMoveOrigin;gActiveUnit=u;gActiveUnitId=index;gActiveUnitMoveOrigin.x=u->xPos;gActiveUnitMoveOrigin.y=u->yPos;floodError=0;GenerateUnitMovementMapExt(u,UNIT_MOV(u));
 if(movement[y][x]>120||(occupancy[y][x]&&occupancy[y][x]!=index)){gActiveUnit=old;gActiveUnitId=oldId;gActiveUnitMoveOrigin=origin;return -3;}
 u8 path[MOVE_CMD_MAX_COUNT];GenerateBestMovementScript(x,y,path);int px=u->xPos,py=u->yPos,n=0;u32 queue[128];
 for(;n<MOVE_CMD_MAX_COUNT-1&&path[n]!=MOVE_CMD_HALT;n++){switch(path[n]){case 0:px--;break;case 1:px++;break;case 2:py++;break;case 3:py--;break;default:floodError=4;break;}queue[n*2]=px|(py<<6);queue[n*2+1]=0;}
 gActiveUnit=old;gActiveUnitId=oldId;gActiveUnitMoveOrigin=origin;if(floodError||px!=x||py!=y)return -4;return n?fe8_unit_move_queue_index(index,queue,n*2):0;
}

// Native choice presentation; original frame/font/names and English message IDs retained.
void fe8_tactical_render_item_menu(u8 *rgba){
 int incoming=fe8_event_item_info(0);if(!incoming)return;int index=fe8_event_item_info(1),choice=fe8_event_item_info(2),supply=fe8_event_item_info(3);struct Unit *u=GetUnit(index);if(!UNIT_IS_VALID(u))return;u16 map[1024]={0};DrawUiFrame(map,3,1,24,18,0,0);
 for(int y=8;y<152;y++)for(int x=24;x<216;x++){int ref=map[(y/8)*32+x/8],dx=ref&1024?7-(x&7):x&7,dy=ref&2048?7-(y&7):y&7,pos=(ref&1023)*32+dy*4+dx/2;if(pos>=sizeof(gUiFrameImage))continue;int ink=gUiFrameImage[pos]>>((dx&1)*4)&15;if(ink)MenuPixel(rgba,x,y,gUiFramePaletteA[ink]);}
 const char *message=supply?tacticalInventorySupply:tacticalInventoryFull;char title[96];snprintf(title,sizeof(title),"%s",message);char *next=strchr(title,'.');if(next){*next=0;next++;while(*next==' ')next++;}MenuText(rgba,title,36,14,0);if(next)MenuText(rgba,next,36,30,0);
 for(int row=0;row<6;row++){int item=row==5?incoming:u->items[row];const char *name=tacticalItemNames[item&255];char uses[8];snprintf(uses,sizeof(uses),"%d",item>>8);if(row==choice)MenuText(rgba,">",30,50+row*16,1);MenuText(rgba,name?name:"",42,50+row*16,row==choice);MenuText(rgba,uses,186,50+row*16,row==choice);}
}

void fe8_tactical_render_trade(u8 *rgba){if(mode!=13||!gActiveUnit)return;struct Unit *units[2]={gActiveUnit,GetUnit(tradePeer)};u16 map[1024]={0};DrawUiFrame(map,1,3,14,15,0,0);DrawUiFrame(map,15,3,14,15,0,0);for(int y=24;y<144;y++)for(int x=8;x<232;x++){int ref=map[(y/8)*32+x/8],dx=ref&1024?7-(x&7):x&7,dy=ref&2048?7-(y&7):y&7,pos=(ref&1023)*32+dy*4+dx/2;if(pos>=sizeof(gUiFrameImage))continue;int ink=gUiFrameImage[pos]>>((dx&1)*4)&15;if(ink)MenuPixel(rgba,x,y,gUiFramePaletteA[ink]);}for(int col=0;col<2;col++){MenuText(rgba,tacticalTrade,20+col*112,30,0);for(int row=0;row<5;row++){int item=units[col]->items[row];if(col==tradeColumn&&row==tradeRow)MenuText(rgba,">",10+col*112,50+row*16,1);if(col==tradeChosenColumn&&row==tradeChosenRow)MenuText(rgba,">",10+col*112,50+row*16,0);const char *name=tacticalItemNames[item&255];MenuText(rgba,name?name:"",20+col*112,50+row*16,col==tradeColumn&&row==tradeRow);}}}
