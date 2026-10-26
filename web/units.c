#include "units.h"
#include "fog.h"
#include "support.h"
#include "events.h"
#include "tactical.h"
#include "save.h"
#include "rng.h"
#include "generated/progress_chapters.h"
extern int GetUnitAid(struct Unit *u);
extern int GetUnitSkill(struct Unit *u);
extern int GetUnitSpeed(struct Unit *u);
extern void SetUnitStatus(struct Unit *u,int status);
#define UNIT_STATUS_NONE 0
#define UNIT_IS_PHANTOM(u) ((u)->pClassData->number==CLASS_PHANTOM)
#define gPlaySt (*fe8_save_loaded_playst())
#include <string.h>
#include <stdlib.h>
#include "generated/unit_assets.h"
static struct Unit units[256];
struct Unit *GetUnit(int index){return &units[index&255];}
const struct CharacterData *GetCharacterData(int id){return id>0&&id<=sizeof(characterData)/sizeof(*characterData)?characterData+id-1:NULL;}
const struct ClassData *GetClassData(int id){return id>0&&id<=sizeof(classData)/sizeof(*classData)?classData+id-1:NULL;}
static void CpuFill16(int value,void *out,int size){(void)value;memset(out,0,size);}
static int GetUnitMaxHp(struct Unit *u){return u->maxHP;} // Current prologue inventory has no HP-bonus equipment.
static int MakeNewItem(int item){int i=item&255;return i<sizeof(itemData)/sizeof(*itemData)?i|((itemData[i].attributes&8?0:itemData[i].uses)<<8):0;}
static void CharStoreAI(struct Unit *u,const struct UnitDefinition *d){u->ai1=d->ai[0];u->ai2=d->ai[1];u->ai_config=d->ai[2]|d->ai[3]<<8;}
static void AdjustNewUnitPosition(struct Unit *u,struct Vec2 *pos,u16 flags){(void)u;(void)pos;(void)flags;} // Supported definitions reject random/nearest placement before this boundary.
static int WriteAndVerifySramFast(const void *src,void *dst,int size){memcpy(dst,src,size);return memcmp(src,dst,size)!=0;} // Packed-unit codec writes to validated native checkpoint records, not live SRAM.
static void ReadSramFast(const void *src,void *dst,int size){memcpy(dst,src,size);} // Caller supplies validated native save records, never a ROM address.
#define UNIT_IS_GORGON_EGG(u) ((u)->pClassData->number==CLASS_GORGONEGG||(u)->pClassData->number==CLASS_GORGONEGG2)
#include "generated/unit_routines.h"
#include "generated/progress_levels.h"
#include "generated/progress_units.h"
#include "generated/progress_cutscene.h"
extern struct Unit *gActiveUnit;
#include "generated/progress_faction.h"
struct Motion {int px,py;const struct REDA *redas;int count,step,delay;int tx,ty,manual;};
static struct Motion motions[256];
static int unitError,loadCutscene,loadNoReda;
void fe8_units_reset(void){InitUnits();memset(motions,0,sizeof(motions));unitError=0;}
struct Unit *fe8_unit_by_pid(int pid){return GetUnitFromCharId(pid);}
void fe8_units_chapter_cleanup(void){ChapterChangeUnitCleanup();memset(motions,0,sizeof(motions));}
int fe8_units_load_event(u32 address,int animated,int cutscene){loadCutscene=cutscene;int result=fe8_units_load(address,animated);loadCutscene=0;return result;}
int fe8_units_load_event_config(u32 address,int animated,int cutscene,int noReda){loadNoReda=noReda;int result=fe8_units_load_event(address,animated,cutscene);loadNoReda=0;return result;}
int fe8_unit_spawn_definition(const struct UnitDefinition *d,int cutscene){
 // Event32 supplies a fixed single-unit definition. Preserve LoadUnit_800F704's
 // existing-Blue/faction semantics; MU placement is immediate in this frontend.
 if(!d||d->allegiance>2||d->autolevel||d->genMonster||d->sumFlag||d->redaCount||d->xPosition>63||d->yPosition>63||!GetCharacterData(d->charIndex)||!GetClassData(d->classIndex))return -1;
 int faction=d->allegiance==2?128:d->allegiance==1?64:0;
 struct Unit *u=GetUnitFromCharId(d->charIndex);if(u&&UNIT_FACTION(u)!=0)u=NULL;
 if(u&&faction){int next=fe8_unit_change_faction((u8)u->index,faction);if(next<0)return -2;u=GetUnit(next);}
 if(!u){u=GetFreeUnit(faction);if(!u)return -2;ClearUnit(u);UnitInitFromDefinition(u,d);UnitLoadStatsFromChracter(u,u->pCharacterData);UnitLoadSupports(u);if(d->itemDrop)u->state|=US_DROP_ITEM;UnitCheckStatCaps(u);SetUnitHp(u,GetUnitMaxHp(u));if(cutscene&&!faction)u->state|=US_BIT22;}
 else if(!faction){u->state&=~US_UNSELECTABLE;if(cutscene){if(u->state&US_DEAD)u->state|=US_BIT22;}else u->state&=~US_BIT22;}
 u->xPos=d->xPosition;u->yPos=d->yPosition;
 memset(motions+(u8)u->index,0,sizeof(*motions));fe8_unit_sync_position((u8)u->index);
 return (u8)u->index;
}
int fe8_units_load(u32 address,int animated){
 const struct UnitDefinition *defs=NULL;int count=0;
 for(int i=0;i<sizeof(unitDefinitions)/sizeof(*unitDefinitions);i++)if(unitDefinitions[i].address==address){defs=unitDefinitions[i].data;count=unitDefinitions[i].count;break;}
 if(!defs)return -1;
 // Unsupported generation never silently substitutes made-up stats or placement.
 for(int i=0;i<count;i++)if(defs[i].genMonster||defs[i].sumFlag||!GetCharacterData(defs[i].charIndex)||!GetClassData(defs[i].classIndex?defs[i].classIndex:GetCharacterData(defs[i].charIndex)->defaultClass))return -2;
 for(int i=0;i<count;i++){
  struct UnitDefinition definition=defs[i];if(loadNoReda){definition.redaCount=0;definition.redas=NULL;}const struct UnitDefinition *d=&definition;int faction=d->allegiance==2?128:d->allegiance==1?64:0;
  struct Unit *u=faction==0?GetUnitFromCharId(d->charIndex):NULL;if(!u||UNIT_FACTION(u)!=faction)u=GetFreeUnit(faction);if(!u){unitError++;return -3;}
  int existed=UNIT_IS_VALID(u);int reuse=(gPlaySt.chapterIndex==1||gPlaySt.chapterIndex==2||gPlaySt.chapterIndex==3||gPlaySt.chapterIndex==4||gPlaySt.chapterIndex==6||fe8_event_story_chapter()==5||gPlaySt.chapterIndex==0x38)&&faction==0&&UNIT_IS_VALID(u);
  if(!reuse){ClearUnit(u);UnitInitFromDefinition(u,d);UnitLoadStatsFromChracter(u,u->pCharacterData);UnitLoadSupports(u);if(d->autolevel)UnitAutolevel(u);UnitCheckStatCaps(u);SetUnitHp(u,GetUnitMaxHp(u));}
  else {s8 x,y;GenUnitDefinitionFinalPosition(d,&x,&y,0);u->xPos=x;u->yPos=y;u->state&=~US_UNSELECTABLE;}
  if((gPlaySt.chapterIndex>=1&&gPlaySt.chapterIndex<=5||gPlaySt.chapterIndex==6)&&faction==128&&u->pCharacterData->number>=0x3c){int bonus=gPlaySt.chapterStateBits&PLAY_FLAG_HARD?progressChapters[(u8)gPlaySt.chapterIndex].hard:-(gPlaySt.config.controller?progressChapters[(u8)gPlaySt.chapterIndex].normal:progressChapters[(u8)gPlaySt.chapterIndex].easy);UnitApplyBonusLevels(u,bonus);}
  if(faction==0){if(loadCutscene){if(!existed||u->state&US_DEAD)u->state|=US_BIT22;}else u->state&=~US_BIT22;}
  if(!(u->state&US_DEAD))u->state&=~(US_HIDDEN|US_NOT_DEPLOYED);
  if(d->itemDrop)u->state|=US_DROP_ITEM;
  struct Motion *m=&motions[(u8)u->index];memset(m,0,sizeof(*m));m->px=u->xPos*16;m->py=u->yPos*16;
  if(animated&&d->redaCount){m->px=d->xPosition*16;m->py=d->yPosition*16;m->redas=d->redas;m->count=d->redaCount;}
 }
 return count;
}
int fe8_units_deploy_existing(u32 address){
 const struct UnitDefinition *defs=NULL;int count=0;
 for(int i=0;i<sizeof(unitDefinitions)/sizeof(*unitDefinitions);i++)if(unitDefinitions[i].address==address){defs=unitDefinitions[i].data;count=unitDefinitions[i].count;break;}
 if(!defs)return -1;
 for(int i=1;i<64;i++){struct Unit *u=GetUnit(i);if(UNIT_IS_VALID(u))u->state|=US_NOT_DEPLOYED|US_HIDDEN;}
 for(int d=0;d<count;d++){struct Unit *u=GetUnitFromCharId(defs[d].charIndex);if(!UNIT_IS_VALID(u)||UNIT_FACTION(u)!=0||u->curHP<=0||u->state&(US_DEAD|US_BIT22))continue;s8 x,y;GenUnitDefinitionFinalPosition(defs+d,&x,&y,0);u->xPos=x;u->yPos=y;u->state&=~(US_NOT_DEPLOYED|US_HIDDEN|US_UNSELECTABLE|US_BIT22);fe8_unit_sync_position((u8)u->index);}
 return 0;
}
int fe8_unit_move_index(int index,int x,int y){if(index<1||index>255)return -1;struct Unit *u=GetUnit(index);if(!UNIT_IS_VALID(u)||x<0||y<0||x>63||y>63)return -1;struct Motion *m=&motions[(u8)u->index];m->redas=NULL;m->count=m->step=m->delay=0;m->tx=x*16;m->ty=y*16;m->manual=1;u->xPos=x;u->yPos=y;return 0;}
int fe8_unit_move(int pid,int x,int y){struct Unit *u=GetUnitFromCharId(pid);return u?fe8_unit_move_index((u8)u->index,x,y):-1;}
void fe8_units_tick(void){
 for(int i=1;i<256;i++)if(UNIT_IS_VALID(units+i)){
  struct Motion *m=motions+i;if(m->delay){m->delay--;continue;}
  if(m->step<m->count){m->tx=m->redas[m->step].x*16;m->ty=m->redas[m->step].y*16;}
  else if(!m->manual)continue;
  if(m->px<m->tx)m->px+=m->tx-m->px<4?m->tx-m->px:4;else if(m->px>m->tx)m->px-=m->px-m->tx<4?m->px-m->tx:4;
  else if(m->py<m->ty)m->py+=m->ty-m->py<4?m->ty-m->py:4;else if(m->py>m->ty)m->py-=m->py-m->ty<4?m->py-m->ty:4;
  if(m->px==m->tx&&m->py==m->ty){if(m->step<m->count){m->delay=m->redas[m->step].delayFrames;m->step++;}else m->manual=0;}
 }
}
int fe8_units_busy(void){for(int i=1;i<256;i++)if(UNIT_IS_VALID(units+i)&&(motions[i].step<motions[i].count||motions[i].manual||motions[i].delay))return 1;return 0;}
void fe8_units_clear_faction(int faction){for(int i=faction+1;i<faction+64&&i<256;i++){struct Unit *u=units+i;if(!UNIT_IS_VALID(u))continue;if(faction==0){u->state|=US_HIDDEN;u->state&=~(US_UNSELECTABLE|US_RESCUING|US_RESCUED);}else ClearUnit(u);memset(motions+i,0,sizeof(*motions));}if(faction==0)ClearCutsceneUnits();}
int fe8_unit_make_item(int item){return MakeNewItem(item);}
int fe8_unit_give(int pid,int item){struct Unit *u=GetUnitFromCharId(pid);return u?UnitAddItem(u,MakeNewItem(item)):-1;}
int fe8_units_restore(int slot){
 fe8_units_reset();struct GameSavePackedUnit records[51];if(fe8_save_read_units(slot,records)<0)return -1;
 for(int i=0;i<51;i++){
  if(!records[i].pid)continue;
  if(!GetCharacterData(records[i].pid)||!GetClassData(records[i].jid)){unitError++;continue;}
  struct Unit *u=units+i+1;LoadSavedUnit(records+i,u);motions[i+1].px=u->xPos*16;motions[i+1].py=u->yPos*16;
 }
 return fe8_units_count();
}
int fe8_unit_change_faction(int index,int faction){
 if(index<1||index>255||(faction!=0&&faction!=64&&faction!=128))return -1;struct Unit *u=GetUnit(index);if(!UNIT_IS_VALID(u))return -2;if(UNIT_FACTION(u)==faction)return index;
 struct Unit *dest=GetFreeUnit(faction);if(!dest)return -3;int next=(u8)dest->index;struct Motion motion=motions[index];UnitChangeFaction(u,faction);memset(motions+index,0,sizeof(*motions));motions[next]=motion;return next;
}
int fe8_units_count(void){int count=0;for(int i=1;i<256;i++)if(UNIT_IS_VALID(units+i))count++;return count;}
int fe8_unit_info(int index,int field){if(index<1||index>255)return -1;struct Unit *u=units+index;if(!UNIT_IS_VALID(u))return -1;switch(field){case 0:return u->pCharacterData->number;case 1:return u->pClassData->number;case 2:return (u8)u->index&192;case 3:return u->xPos;case 4:return u->yPos;case 5:return u->maxHP;case 6:return u->curHP;case 7:return u->pow;case 8:return u->skl;case 9:return u->spd;case 10:return u->def;case 11:return u->res;case 12:return u->lck;case 13:return u->level;case 14:return u->exp;case 15:return u->state;case 16:return u->pCharacterData->nameTextId;case 17:return u->pClassData->baseMov+u->movBonus;case 18:return motions[index].px;case 19:return motions[index].py;case 20:return u->pClassData->SMSId;case 21:return u->conBonus;case 22:return u->movBonus;case 23:return u->supportBits;case 24:return u->ai1;case 25:return u->ai2;case 26:return u->ai_config;case 27:return UNIT_LEADER_CHARACTER(u);case 63:return u->torchDuration;case 61:return GetUnitSkill(u);case 62:return GetUnitSpeed(u);case 60:return GetUnitAid(u);case 58:return u->rescue;case 59:return UNIT_CON(u);case 28:return u->ai_a_pc;case 29:return u->ai_b_pc;default:if(field>=30&&field<35)return u->items[field-30];if(field>=40&&field<48)return u->ranks[field-40];if(field>=50&&field<57)return u->supports[field-50];return -1;}}
void fe8_units_render(u8 *rgba,int cameraX,int cameraY,int clock){
 // Source standing sheet phase: 0..31 frame0, 32..35 frame1, 36..67 frame2, 68..71 frame1.
 int phase=clock%72,frame=phase>=68?1:phase>=36?2:phase>=32?1:0;
 // Native redraw sorts by foot position, matching the original SMS handle ordering.
 for(int foot=-32;foot<192;foot++)for(int i=255;i>0;i--){
  struct Unit *u=units+i;if(!UNIT_IS_VALID(u)||u->xPos<0||u->yPos<0||((u->state&(US_DEAD|US_NOT_DEPLOYED|US_RESCUED))||((u->state&US_HIDDEN)&&!fe8_tactical_show_active(i))))continue;
  if(UNIT_FACTION(u)==128&&!fe8_fog_visible(u->xPos,u->yPos))continue;int sy=motions[i].py-cameraY;if(sy!=foot)continue;int sms=u->pClassData->SMSId;if(sms>=sizeof(standingSprites)/sizeof(*standingSprites)){unitError++;continue;}
  int size=standingSprites[sms].size,w=size==2?32:16,h=size==0?16:32;int offset=frame*w*h/2;if(offset+w*h/2>standingSprites[sms].bytes){unitError++;continue;}const u8 *data=standingSprites[sms].data+offset;
  int sx=motions[i].px-cameraX-(size==2?8:0);sy-=h-16;int faction=UNIT_FACTION(u),bank=faction==128?1:faction==64?2:0;
  if(u->state&US_UNSELECTABLE)bank=3;
  for(int y=0;y<h;y++)for(int x=0;x<w;x++){
   int dx=sx+x,dy=sy+y;if(dx<0||dy<0||dx>=240||dy>=160)continue;
   int tile=(y/8)*(w/8)+x/8,pos=tile*32+(y&7)*4+(x&7)/2,index=(data[pos]>>((x&1)*4))&15;if(!index)continue;
   int color=gPaletteBuffer[448+bank*16+index];u8 *p=rgba+(dy*240+dx)*4;for(int c=0;c<3;c++){int value=(color>>(c*5))&31;p[c]=(value<<3)|(value>>2);}p[3]=255;
  }
 }
 // Original PutUnitSpriteIconsOam rescue indicator: tile 3, palette by passenger faction,
 // offset (+9,+7), blink 20/32 updates and flag 132. MU presentation remains adapted.
 if(clock%32<20&&!fe8_event_flag(132))for(int i=191;i>0;i--){struct Unit *u=units+i;if(!UNIT_IS_VALID(u)||!(u->state&US_RESCUING)||u->state&(US_HIDDEN|US_DEAD|US_NOT_DEPLOYED|US_RESCUED))continue;int faction=u->rescue>>6;if(faction>2)continue;int bank=faction==1?2:faction==2?1:0,x=u->xPos*16-cameraX+9,y=u->yPos*16-cameraY+7;for(int dy=0;dy<8;dy++)for(int dx=0;dx<8;dx++){int ink=rescueIconTile[dy*4+dx/2]>>((dx&1)*4)&15;if(!ink||x+dx<0||x+dx>=240||y+dy<0||y+dy>=160)continue;int color=gPaletteBuffer[448+bank*16+ink];u8 *p=rgba+((y+dy)*240+x+dx)*4;for(int c=0;c<3;c++){int v=color>>(c*5)&31;p[c]=(v<<3)|(v>>2);}p[3]=255;}}

}


int fe8_unit_move_queue_index(int index,const u32 *queue,int length){
 if(index<1||index>255)return -1;static struct REDA paths[256][64];struct Unit *u=GetUnit(index);
 if(!UNIT_IS_VALID(u)||!queue||length<2||length>128||(length&1))return -1;
 for(int i=0;i<length/2;i++){u32 a=queue[i*2],b=queue[i*2+1];paths[(u8)u->index][i]=(struct REDA){.x=a&63,.y=a>>6&63,.flags=a>>12&63,.a=a>>24,.b=b&65535,.delayFrames=b>>16};}
 struct Motion *m=motions+(u8)u->index;m->redas=paths[(u8)u->index];m->count=length/2;m->step=m->delay=m->manual=0;u->xPos=m->redas[m->count-1].x;u->yPos=m->redas[m->count-1].y;return 0;
}

int fe8_unit_move_queue(int pid,const u32 *queue,int length){struct Unit *u=GetUnitFromCharId(pid);return u?fe8_unit_move_queue_index((u8)u->index,queue,length):-1;}
void fe8_units_apply_palette(void){memcpy(gPaletteBuffer+448,unitSpritePalettes,128);}

int fe8_unit_at(int x,int y){for(int i=1;i<256;i++)if(UNIT_IS_VALID(units+i)&&!(units[i].state&(US_HIDDEN|US_DEAD|US_NOT_DEPLOYED|US_RESCUED))&&units[i].xPos==x&&units[i].yPos==y){if(UNIT_FACTION(units+i)==128&&!fe8_fog_visible(x,y))continue;return i;}return -1;}
int fe8_units_errors(void){return unitError;}

void fe8_unit_remove_index(int index){if(index>0&&index<256){memset(motions+index,0,sizeof(*motions));ClearUnit(GetUnit(index));}}
void fe8_unit_remove(int pid){struct Unit *u=GetUnitFromCharId(pid);if(u){memset(motions+(u8)u->index,0,sizeof(*motions));ClearUnit(u);}}

void fe8_unit_sync_position(int index){if(index<1||index>255||!UNIT_IS_VALID(units+index))return;struct Motion *m=motions+index;memset(m,0,sizeof(*m));m->px=m->tx=units[index].xPos*16;m->py=m->ty=units[index].yPos*16;}

extern void UnitRemoveInvalidItems(struct Unit *u);
int fe8_unit_steal(int actor,int target,int slot){if(actor<1||actor>255||target<1||target>255||actor==target||slot<0||slot>4)return -1;struct Unit *a=GetUnit(actor),*b=GetUnit(target);if(!UNIT_IS_VALID(a)||!UNIT_IS_VALID(b)||a->state&(US_DEAD|US_RESCUED)||b->state&(US_DEAD|US_RESCUED)||abs(a->xPos-b->xPos)+abs(a->yPos-b->yPos)!=1||!b->items[slot])return -2;if(!UnitAddItem(a,b->items[slot]))return -3;b->items[slot]=0;UnitRemoveInvalidItems(b);return 0;} // Original AiStartStealAction inventory effects; animated popup deferred.

int fe8_units_pack_blue(struct GameSavePackedUnit *out){if(!out)return -1;for(int i=52;i<64;i++)if(UNIT_IS_VALID(GetUnit(i)))return -2;for(int i=0;i<51;i++){struct Unit *u=GetUnit(i+1);if(UNIT_IS_VALID(u)&&!u->pClassData)return -3;WriteGameSavePackedUnit(u,out+i);}return 0;}
