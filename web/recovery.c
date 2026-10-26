#include "recovery.h"
#include "fog.h"
#include "generated/battle_types.h"
#include "units.h"
#include "map.h"
#include <string.h>
extern struct Unit *GetUnit(int index);
extern int GetUnitCurrentHp(struct Unit *u),GetUnitMaxHp(struct Unit *u),GetUnitPower(struct Unit *u),GetItemIndex(int item),GetItemAttributes(int item),GetItemAfterUse(int item);
extern void SetUnitStatus(struct Unit *u,int status),UnitRemoveInvalidItems(struct Unit *u);
#include "generated/recovery_assets.h"
static u8 terrain[64][64],*gBmMapTerrain[64];
static struct {int index,amount;} targets[128];
static int count,lastHealed,lastCured;
static void InitTargets(int x,int y){(void)x;(void)y;count=0;}
static void AddTarget(int x,int y,int index,int amount){(void)x;(void)y;if(count<128)targets[count++]=(typeof(*targets)){index,amount};}
#include "generated/recovery_routines.h"
int fe8_recovery_phase(int faction){
 if((faction!=0&&faction!=64&&faction!=128)||!fe8_map_active())return -1;
 for(int y=0;y<64;y++){gBmMapTerrain[y]=terrain[y];for(int x=0;x<64;x++){int t=fe8_map_terrain(x,y);terrain[y][x]=t<0?0:t;}}
 fe8_fog_phase(faction);lastHealed=lastCured=0;MakeTerrainHealTargetList(faction);
 for(int i=0;i<count;i++){struct Unit *u=GetUnit(targets[i].index);if(targets[i].amount<0){SetUnitStatus(u,0);lastCured++;}else{int hp=u->curHP;AddUnitHp(u,targets[i].amount);lastHealed+=u->curHP-hp;}}
 return count;
}
int fe8_recovery_item(int index,int slot){
 if(index<1||index>255||slot<0||slot>=5)return -1;struct Unit *u=GetUnit(index);if(!UNIT_IS_VALID(u)||u->curHP<=0||u->state&(US_DEAD|US_NOT_DEPLOYED|US_RESCUED))return -2;int item=u->items[slot],id=GetItemIndex(item);if(id!=ITEM_VULNERARY&&id!=ITEM_ELIXIR)return -3;
 int hp=u->curHP;AddUnitHp(u,GetUnitItemHealAmount(u,item));u->items[slot]=GetItemAfterUse(item);UnitRemoveInvalidItems(u);return u->curHP-hp;
}
int fe8_recovery_info(int field){return field==0?lastHealed:field==1?lastCured:field==2?count:-1;}

void fe8_recovery_reset(void){count=lastHealed=lastCured=0;}
