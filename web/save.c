#include "save.h"
#include "units.h"
#include "events.h"
#include "tactical.h"
#include <string.h>
#include "generated/save_chapter_ids.h"
static struct SaveBlocks storage;
static u8 staging[32768];
static struct SaveBlocks *gSram=&storage;
#define CART_SRAM ((u8 *)&storage)
static struct PlaySt gPlaySt;
static struct UnitUsageStats gPidStatsData[BWL_ARRAY_NUM];
static struct ChapterStats gChapterStats[WIN_ARRAY_NUM];
static struct UnitUsageStats *gPidStatsSaveLoc;
static u32 gBonusContentClaimFlags;
static _Alignas(4) u8 gGenericBuffer[32768];
static u16 supply[100];
#define gConvoyItemArray supply
static struct {u16 itemUnk2E;} gBmSt;
#include "generated/progress_convoy.h"
int fe8_convoy_count(void){return GetConvoyItemCount();}
int fe8_convoy_item(int index){return index>=0&&index<100?supply[index]:-1;}
int fe8_convoy_add(int item){return item>0?AddItemToConvoy(item):-1;}
static u8 permanentFlags[25];
static const char sSaveMarker[]="AGB-FE9";
static int initialized,revision,errors,importWarnings;
static struct GameSaveBlock chapterStart,checkpointOrigin;
static int checkpointCaptured,checkpointSerial,checkpointSavedSerial,checkpointError;
static u32 checkpointOriginChecksum,playtimeFrames;
static bool IsSramWorking(void){return true;}
static bool InStorage(const void *p,u32 size){uintptr_t a=(uintptr_t)p,b=(uintptr_t)&storage;return a>=b&&a-b<=32768&&size<=32768-(a-b);}
static void ReadSramFast(const void *src,void *dst,u32 size){
 if(!InStorage(src,size)){errors++;memset(dst,0,size);return;}memcpy(dst,src,size);
}
static int WriteAndVerifySramFast(const void *src,void *dst,u32 size){
 if(!InStorage(dst,size)){errors++;return 1;}memcpy(dst,src,size);revision++;return memcmp(src,dst,size)!=0;
}
static void CpuFill16(u16 value,void *dst,u32 size){u8 *p=dst;for(u32 i=0;i<size;i++)p[i]=(value>>((i&1)*8))&255;}
static bool StringCompare(const char *a,const char *b){return memcmp(a,b,8)==0;}
static void CopyString(char *dst,const char *src){memcpy(dst,src,8);}
// Assembly Checksum32 loads HALFWORDS, despite the misleading source comment.
static u32 ComputeChecksum32(const void *data,int size){
 const u8 *p=data;u32 sum=0,xor=0;for(int i=0;i+1<size;i+=2){u32 v=p[i]|p[i+1]<<8;sum+=v;xor^=v;}return (sum&65535)|(xor<<16);
}
static bool ValidBlockRange(const struct SaveBlockInfo *c,int index){
 static const u16 offsets[]={0x3fc4,0x4d8c,0x5b54,0xd4,0x204c,0x691c,0x7400};
 if(index<0||index>6||c->offset!=offsets[index])return false;
 if(!c->size||(c->offset&1)||(c->size&1)||c->offset<0xd4||c->offset>=32768||c->size>32768-c->offset)return false;
 if(index<3)return c->kind==SAVEBLOCK_KIND_GAME&&c->size==0xdc8;
 if(index==3||index==4)return c->kind==SAVEBLOCK_KIND_SUSPEND&&c->size==0x1f78;
 if(index==5)return c->kind==SAVEBLOCK_KIND_ARENA&&c->size==0x874;
 return c->kind==SAVEBLOCK_KIND_XMAP&&c->size<=0xc00;
}
static void SetGameTime(int time){(void)time;}
static void InitUnits(void){} // No starting units are written here; original routine writes zero-packed units.
static void ClearSupplyItems(void){memset(supply,0,sizeof(supply));}
static u16 *GetConvoyItemArray(void){return supply;}
static void CpuCopy16(const void *src,void *dst,int size){memcpy(dst,src,size);}
#include "generated/progress_convoy_remove.h"
int fe8_convoy_remove(int index){if(index<0||index>=100||!supply[index])return -1;int item=supply[index];RemoveItemFromConvoy(index);return item;}
static void ResetPermanentFlags(void){memset(permanentFlags,0,sizeof(permanentFlags));}
static const u8 *GetPermanentFlagBits(void){return permanentFlags;}
static int GetPermanentFlagBitsSize(void){return 25;}
static bool IsValidSuspendSave(int slot);
static void ReadSuspendSavePlaySt(int slot,struct PlaySt *p);
#include "generated/save_routines.h"
#include "generated/save_checkpoint_codec.h"
static bool IsValidSuspendSave(int slot){return slot>=3&&slot<=4&&ReadSaveBlockInfo(NULL,slot);}
static void ReadSuspendSavePlaySt(int slot,struct PlaySt *p){struct SuspendSaveBlock *s=GetSaveReadAddr(slot);if(s)ReadSramFast(s,p,sizeof(*p));else memset(p,0,sizeof(*p));}
void fe8_save_ensure(void){if(!initialized){initialized=1;InitGlobalSaveInfodata();}}
void fe8_save_reset_empty(void){fe8_save_checkpoint_reset();playtimeFrames=0;initialized=1;memset(&gPlaySt,0,sizeof(gPlaySt));errors=importWarnings=0;InitGlobalSaveInfodata();}
int fe8_save_valid(int slot){fe8_save_ensure();return slot>=0&&slot<3&&IsSaveValid(slot);}
int fe8_save_chapter(int slot){struct PlaySt p;if(!fe8_save_valid(slot))return -1;ReadGameSavePlaySt(slot,&p);return (u8)p.chapterIndex;}
int fe8_save_difficulty(int slot){struct PlaySt p;if(!fe8_save_valid(slot))return -1;ReadGameSavePlaySt(slot,&p);return p.chapterStateBits&PLAY_FLAG_HARD?2:p.config.controller?1:0;}
int fe8_save_time(int slot){struct PlaySt p;if(!fe8_save_valid(slot))return 0;ReadGameSavePlaySt(slot,&p);return p.time_saved>215999940u?215999940:(int)p.time_saved;}
int fe8_save_last_slot(void){fe8_save_ensure();return ReadLastGameSaveId();}
int fe8_save_create(int slot,int difficulty){
 fe8_save_ensure();if(slot<0||slot>2||difficulty<0||difficulty>2)return -1;
 if(fe8_save_valid(slot))return -2;
 if(!GetNewPlaythroughId())return -3;
 WriteNewGameSave(slot,difficulty==2,1,difficulty!=0);
 return fe8_save_valid(slot)?0:-4;
}
int fe8_save_copy(int source,int destination){
 fe8_save_ensure();if(source<0||source>2||destination<0||destination>2||source==destination)return -1;
 if(!fe8_save_valid(source))return -2;if(fe8_save_valid(destination))return -3;
 CopyGameSave(source,destination);return fe8_save_valid(destination)?0:-4;
}
int fe8_save_erase(int slot){if(!fe8_save_valid(slot))return -1;InvalidateGameSave(slot);return fe8_save_valid(slot)?-2:0;}
int fe8_save_load(int slot){if(!fe8_save_valid(slot))return -1;ReadGameSavePlaySt(slot,&gPlaySt);struct GameSaveBlock *block=GetSaveReadAddr(slot);ReadSupplyItems(block->supplyItems);memcpy(permanentFlags,block->permanentFlags,25);playtimeFrames=gPlaySt.time_saved>215999940u?215999940u:gPlaySt.time_saved;gPlaySt.gameSaveSlot=slot;WriteLastGameSaveId(slot);return 0;}
u8 *fe8_save_buffer(void){fe8_save_ensure();return (u8 *)&storage;}
u8 *fe8_save_import_buffer(void){return staging;}
int fe8_save_global_valid(void){fe8_save_ensure();return ReadGlobalSaveInfo(NULL);}
int fe8_save_revision(void){return revision;}
int fe8_save_errors(void){return errors;}
int fe8_save_import_warnings(void){return importWarnings;}
struct PlaySt *fe8_save_loaded_playst(void){return &gPlaySt;}
int fe8_save_import(int length){
 fe8_save_ensure();if(length!=32768)return -1;
 struct GlobalSaveInfo info;memcpy(&info,staging,sizeof(info));
 if(!StringCompare(info.name,sSaveMarker)||info.magic32!=SAVEMAGIC32||info.magic16!=SAVEMAGIC16||info.checksum!=Checksum16(&info,80))return -2;
 // Validate before changing live SRAM. A single corrupted occupied slot rejects the import.
 int warning=0;struct SaveBlockInfo accepted[7];int used=0;
 for(int slot=0;slot<7;slot++){
  struct SaveBlockInfo c;memcpy(&c,staging+0x64+slot*16,16);
  if(c.magic16!=SAVEMAGIC16){
   const u8 *raw=(const u8 *)&c;int allZero=1,allFF=1;
   for(int i=0;i<16;i++){allZero&=raw[i]==0;allFF&=raw[i]==255;}
   if(allZero||allFF||(c.kind==255&&c.magic16==0&&c.size==0&&c.offset==0))continue;
   return -3;
  }
  u32 magic=slot<5?SAVEMAGIC32:slot==5?SAVEMAGIC32_ARENA:SAVEMAGIC32_XMAP;
  if(c.magic32!=magic||!ValidBlockRange(&c,slot)||c.checksum32!=ComputeChecksum32(staging+c.offset,c.size))return -3;
  for(int i=0;i<used;i++)if(c.offset<accepted[i].offset+accepted[i].size&&accepted[i].offset<c.offset+c.size)return -3;
  accepted[used++]=c;
  if(slot>=3)warning++;
 }
 memcpy(&storage,staging,32768);fe8_save_checkpoint_reset();revision++;importWarnings=warning;return 0;
}

int fe8_save_title(int slot){
 struct PlaySt p;if(!fe8_save_valid(slot))return 0x54;ReadGameSavePlaySt(slot,&p);
 if(p.chapterStateBits&PLAY_FLAG_POSTGAME)return 0x57;
 if(p.chapterStateBits&PLAY_FLAG_COMPLETE)return 0x55;
 return (u8)p.chapterIndex<sizeof(saveChapterTitleIds)?saveChapterTitleIds[(u8)p.chapterIndex]:0x54;
}

void fe8_save_read_playst(int slot,struct PlaySt *p){memset(p,0,sizeof(*p));if(fe8_save_valid(slot))ReadGameSavePlaySt(slot,p);}
int fe8_save_suspend_slot(void){
 fe8_save_ensure();struct GlobalSaveInfo info;ReadGlobalSaveInfo(&info);
 int first=info.last_suspend_slot==1?4:3;
 for(int n=0;n<2;n++){int slot=n?7-first:first;if(IsValidSuspendSave(slot)){struct PlaySt p;ReadSuspendSavePlaySt(slot,&p);return p.gameSaveSlot<3?p.gameSaveSlot:0;}}
 return -1;
}

int fe8_save_read_units(int slot,struct GameSavePackedUnit *out){if(!fe8_save_valid(slot))return -1;struct GameSaveBlock *block=GetSaveReadAddr(slot);ReadSramFast(block->units,out,51*sizeof(*out));return 0;}

const u8 *fe8_save_loaded_flags(void){return permanentFlags;}
void fe8_save_checkpoint_reset(void){checkpointCaptured=checkpointSavedSerial=checkpointError=0;}
void fe8_save_playtime_tick(void){if(playtimeFrames<215999940u)playtimeFrames++;}
int fe8_save_checkpoint_capture(void){
 checkpointCaptured=0;int slot=gPlaySt.gameSaveSlot,chapter=(u8)gPlaySt.chapterIndex;
 if((chapter>6)||slot<0||slot>2||!fe8_save_valid(slot))return -1;
 struct SaveBlockInfo info;if(!ReadSaveBlockInfo(&info,slot))return -2;
 struct GameSaveBlock *source=GetSaveReadAddr(slot);if(source->playSt.playthroughIdentifier!=gPlaySt.playthroughIdentifier)return -3;
 memcpy(&checkpointOrigin,source,sizeof(checkpointOrigin));memcpy(&chapterStart,source,sizeof(chapterStart));chapterStart.playSt=gPlaySt;chapterStart.playSt.chapterTurnNumber=0;chapterStart.playSt.faction=64;chapterStart.playSt.time_saved=playtimeFrames;
 if(fe8_units_pack_blue(chapterStart.units)<0)return -4;
 PackCheckpointSupply(chapterStart.supplyItems);fe8_event_pack_permanent(chapterStart.permanentFlags);checkpointOriginChecksum=info.checksum32;checkpointSerial++;checkpointSavedSerial=0;checkpointCaptured=1;checkpointError=0;return 0;
}
static int CheckpointAvailable(void){int boundary=checkpointCaptured&&chapterStart.playSt.chapterIndex==6&&fe8_event_info(1)==42&&fe8_event_info(9)==5;return checkpointCaptured&&(!fe8_event_busy()||boundary)&&!fe8_event_info(12)&&!fe8_units_busy()&&fe8_tactical_info(0)==0&&gPlaySt.faction==0&&gPlaySt.gameSaveSlot==chapterStart.playSt.gameSaveSlot&&(gPlaySt.chapterIndex==chapterStart.playSt.chapterIndex||boundary)&&gPlaySt.playthroughIdentifier==chapterStart.playSt.playthroughIdentifier;}
int fe8_save_checkpoint_info(int field){return field==0?CheckpointAvailable():field==1?(checkpointCaptured?(u8)chapterStart.playSt.chapterIndex:-1):field==2?(checkpointCaptured?chapterStart.playSt.gameSaveSlot:-1):field==3?(checkpointCaptured&&checkpointSavedSerial==checkpointSerial):field==4?checkpointSerial:field==5?checkpointError:-1;}
int fe8_save_checkpoint_commit(void){
 checkpointError=0;if(!CheckpointAvailable())return checkpointError=-1;int slot=chapterStart.playSt.gameSaveSlot;struct SaveBlockInfo current;if(!ReadSaveBlockInfo(&current,slot)||(current.checksum32!=checkpointOriginChecksum||memcmp(GetSaveReadAddr(slot),&checkpointOrigin,sizeof(checkpointOrigin))))return checkpointError=-2;
 if(checkpointSavedSerial==checkpointSerial)return 0;
 struct SaveBlocks backup=storage;int oldRevision=revision,oldErrors=errors;chapterStart.playSt.time_saved=playtimeFrames;
 struct GameSaveBlock *dest=GetSaveWriteAddr(slot);WriteAndVerifySramFast(&chapterStart,dest,sizeof(chapterStart));struct SaveBlockInfo chunk={.magic32=SAVEMAGIC32,.kind=SAVEBLOCK_KIND_GAME};WriteSaveBlockInfo(&chunk,slot);WriteLastGameSaveId(slot);
 if(errors!=oldErrors||!ReadSaveBlockInfo(&current,slot)){storage=backup;revision=oldRevision;return checkpointError=-3;}
 memcpy(&checkpointOrigin,&chapterStart,sizeof(checkpointOrigin));checkpointOriginChecksum=current.checksum32;checkpointSavedSerial=checkpointSerial;return 0;
} // Explicit native chapter-start save: original packed formats/checksums; unrelated slots/suspensions and unported metadata stay opaque.
