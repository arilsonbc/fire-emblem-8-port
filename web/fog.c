#include "generated/unit_types.h"
#include "save.h"
#include "map.h"
#include "tactical.h"
#include "fog.h"
#include "constants/items.h"
#include <string.h>
#define gPlaySt (*fe8_save_loaded_playst())
extern struct Unit *GetUnit(int index);
extern int GetItemAfterUse(int item);
extern void UnitRemoveInvalidItems(struct Unit *unit);
static struct Vec2 gBmMapSize;
static u8 cells[64][64],*gWorkingBmMap[64];
#define MapAddInRange FogAddInRange
#include "generated/fog_routines.h"
int fe8_fog_enabled(void){return fe8_map_info(0)==7&&gPlaySt.chapterVisionRange>0&&fe8_tactical_info(0)>=0;}
int fe8_fog_visible(int x,int y){if(x<0||y<0||x>=fe8_map_info(1)||y>=fe8_map_info(2))return 0;return !fe8_fog_enabled()||cells[y][x]!=0;}
void fe8_fog_refresh(void){
 gBmMapSize.x=fe8_map_info(1);gBmMapSize.y=fe8_map_info(2);if(gBmMapSize.x<1||gBmMapSize.x>64||gBmMapSize.y<1||gBmMapSize.y>64)return;
 memset(cells,0,sizeof(cells));for(int y=0;y<64;y++)gWorkingBmMap[y]=cells[y];if(!fe8_fog_enabled())return;
 // Original RefreshUnitsOnBmMap vision union; selected native MU retains its vision.
 for(int i=1;i<128;i++){struct Unit *u=GetUnit(i);if(!UNIT_IS_VALID(u)||u->curHP<=0||u->state&(US_DEAD|US_NOT_DEPLOYED|US_RESCUED|US_BIT16)||((u->state&US_HIDDEN)&&!fe8_tactical_show_active(i))||u->xPos<0||u->yPos<0||u->xPos>=gBmMapSize.x||u->yPos>=gBmMapSize.y)continue;MapAddInRange(u->xPos,u->yPos,GetUnitFogViewRange(u),1);}
 for(int i=129;i<198;i++){struct Unit *u=GetUnit(i);if(!UNIT_IS_VALID(u)||u->state&(US_HIDDEN|US_DEAD|US_NOT_DEPLOYED|US_RESCUED)||u->xPos<0||u->yPos<0||u->xPos>=gBmMapSize.x||u->yPos>=gBmMapSize.y)continue;if(!cells[u->yPos][u->xPos])u->state|=US_BIT9;else if(u->state&US_BIT9){u->state&=~US_BIT9;if(gPlaySt.faction!=FACTION_RED)u->state|=US_BIT8;}}
}
void fe8_fog_phase(int faction){if(!gPlaySt.chapterVisionRange)return;for(int i=faction+1;i<faction+64&&i<256;i++){struct Unit *u=GetUnit(i);if(UNIT_IS_VALID(u)&&!(u->state&(US_UNAVAILABLE|US_RESCUED))&&u->torchDuration)u->torchDuration--;}}
int fe8_fog_can_torch(int index,int slot){if(index<1||index>255||slot<0||slot>4)return 0;struct Unit *u=GetUnit(index);return UNIT_IS_VALID(u)&&u->curHP>0&&!(u->state&(US_DEAD|US_NOT_DEPLOYED|US_RESCUED))&&(u->items[slot]&255)==ITEM_TORCH&&gPlaySt.chapterVisionRange&&u->torchDuration!=4;}
int fe8_fog_use_torch(int index,int slot){if(!fe8_fog_can_torch(index,slot))return -1;struct Unit *u=GetUnit(index);u->torchDuration=4;u->items[slot]=GetItemAfterUse(u->items[slot]);UnitRemoveInvalidItems(u);fe8_fog_refresh();return 0;}
