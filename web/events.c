extern void SetKeyStatus_IgnoreMask(int mask);
#include "events.h"
#include "audio.h"
#include "background.h"
#include <stdint.h>
#include "rng.h"
#include "generated/progress_chapters.h"
#include "dialogue.h"
#include "battle.h"
#include "death.h"
#include "gameover.h"
#include "generated/gameover_address.h"
#include "tactical.h"
#include "generated/event_types.h"
#include "units.h"
#include "map.h"
#include "save.h"
#include <string.h>
static struct EventEngineProc engine;
#include "generated/event_assets.h"
#define gPlaySt (*fe8_save_loaded_playst())
static struct KeyStatusBuffer *gKeyStatusPtr;
extern struct KeyStatusBuffer *fe8_input_status(void);
static struct {struct {u8 win0_enableBlend,win1_enableBlend,wobj_enableBlend,wout_enableBlend;} wincnt;int blendY,blendMode;} gLCDControlBuffer;
static struct Proc *fadeAnchor;
static const struct ProcCmd eventFadeAnchorScript[]={PROC_BLOCK,PROC_END};
static ProcPtr EventFadeParent(void){if(!fadeAnchor)fadeAnchor=Proc_Start(eventFadeAnchorScript,PROC_TREE_4);return fadeAnchor;}
static void SetBlendConfig(int mode,int a,int b,int y){(void)a;(void)b;gLCDControlBuffer.blendMode=mode;gLCDControlBuffer.blendY=y;}
static void SetBlendTargetB(int a,int b,int c,int d,int e){(void)a;(void)b;(void)c;(void)d;(void)e;}
#include "generated/event_fades.h"
static int EventFadePending(void){if(!fadeAnchor)return 0;if(fadeAnchor->proc_lockCnt)return 1;Proc_End(fadeAnchor);fadeAnchor=NULL;return 0;}

static struct EventQueue gEventActiveQueue[8];
static u32 gEventSlots[14],queue[64];static u8 eventFlags[512];
static int eventTriggerId,eventTriggerAddress,eventCallSerial;
static struct Unit *eventActor;
#define gActiveUnit eventActor
static int requestedChapter=-1,progressAction,progressCount,progressFrom=-1,progressTo=-1,progressError,lastWin,lastBoss;
static u16 monsterRN[3];
void fe8_progress_reset(void){fe8_save_checkpoint_reset();requestedChapter=progressFrom=progressTo=-1;progressAction=progressCount=progressError=lastWin=lastBoss=0;}
int fe8_progress_info(int f){return f==0?progressCount:f==1?progressFrom:f==2?progressTo:f==3?requestedChapter:f==4?progressError:f==5?lastWin:f==6?lastBoss:f==7?gPlaySt.partyGoldAmount:-1;}
static struct {int kind,cursor,actor;} chapterJobs[4];
static int chapterJobCount;
static int noRedaNext;
static int audioWait,pausedText=-1,pausedPage;
static int pendingItem,pendingUnit,pendingChoice,itemResolved,pendingOrigin;
static int rewardUnit,rewardValue,rewardGold,lastRewardItem,lastRewardUnit,rewardSerial;
static int running,blocked=-1,textId=-1,textPage,textCount,executed,requests=-1,preview;
static const u16 *ResolveScript(u32 address){for(int i=0;i<sizeof(eventScripts)/sizeof(*eventScripts);i++)if(eventScripts[i].address==address)return eventScripts[i].data;return NULL;}
static int ScriptRemaining(const u16 *pointer){uintptr_t p=(uintptr_t)pointer;for(int i=0;i<sizeof(eventScripts)/sizeof(*eventScripts);i++){uintptr_t base=(uintptr_t)eventScripts[i].data;if(p>=base&&p<base+eventScripts[i].length*2&&!(p&1))return (base+eventScripts[i].length*2-p)/2;}return 0;}
static const u16 *FindLabel(const u16 *pointer,int label){for(int n=0;n<2048&&ScriptRemaining(pointer)>=2;n++){if((pointer[0]>>8)==8&&pointer[1]==label)return pointer;int length=EVT_CMD_LEN(pointer);if(!length||length>ScriptRemaining(pointer))break;pointer+=length;}return NULL;}
static void SetFlag(int flag){if(flag>0&&flag<512)eventFlags[flag]=1;}
static void ClearFlag(int flag){if(flag>=0&&flag<512)eventFlags[flag]=0;}
static bool CheckFlag(int flag){return flag>0&&flag<512&&eventFlags[flag];}
static void SlotQueuePush(u32 value){if(gEventSlots[13]>=64){blocked=7;return;}queue[gEventSlots[13]++]=value;}
static u32 SlotQueuePop(void){if(!gEventSlots[13]){blocked=7;return 0;}u32 value=queue[0];for(int i=1;i<gEventSlots[13];i++)queue[i-1]=queue[i];gEventSlots[13]--;return value;}
#include "generated/event_routines.h"
extern const struct CharacterData *GetCharacterData(int id);
extern const struct ClassData *GetClassData(int id);
enum {UDEF_AIIDX_AI_A=0,UDEF_AIIDX_AI_B=1,UDEF_AIIDX_AI_CONF_L=2,UDEF_AIIDX_AI_CONF_H=3};
#include "generated/event_spawn.h"
static u8 menuOverrides[256][2];
enum {MENU_OVERRIDE_ISAVAILABLE=0,MENU_OVERRIDE_ONSELECT=1};
#define ARRAY_COUNT(x) (sizeof(x)/sizeof(*(x)))
static int MenuAlwaysNotShown(void){return 0;}
static int MenuAlwaysDisabled(void){return 0;}
static int Get8(void){return 8;}
static void ResetMenuOverrides(void){memset(menuOverrides,0,sizeof(menuOverrides));}
static void AddMenuOverride(int id,int kind,int (*callback)(void)){if(id>=0&&id<256&&kind>=0&&kind<2)menuOverrides[id][kind]=callback==MenuAlwaysNotShown?1:callback==MenuAlwaysDisabled?2:3;}
#include "generated/event_guide.h"
extern struct Unit *GetUnit(int index);
extern void ClearUnit(struct Unit *unit);
#include "generated/event_party.h"
int fe8_event_story_chapter(void){return engine.chapterIndex;}

#include "generated/progress_icons.h"
void fe8_event_show_icons(void){UnsetEventId_0x84(NULL);}
void fe8_event_reset(void){noRedaNext=0;fe8_gameover_reset();SetKeyStatus_IgnoreMask(0);pendingItem=pendingUnit=pendingChoice=itemResolved=pendingOrigin=rewardUnit=rewardValue=rewardGold=lastRewardItem=lastRewardUnit=rewardSerial=0;fe8_background_set(-1);pausedText=-1;pausedPage=0;audioWait=0;eventActor=NULL;fe8_death_reset();chapterJobCount=eventTriggerId=eventTriggerAddress=eventCallSerial=0;fe8_tactical_reset();ResetMenuOverrides();fe8_battle_reset();fe8_dialogue_reset();if(fadeAnchor){Proc_End(fadeAnchor);fadeAnchor=NULL;}memset(&gLCDControlBuffer,0,sizeof(gLCDControlBuffer));memset(&engine,0,sizeof(engine));memset(gEventActiveQueue,0,sizeof(gEventActiveQueue));memset(gEventSlots,0,sizeof(gEventSlots));memset(eventFlags,0,sizeof(eventFlags));running=preview=executed=0;blocked=textId=requests=-1;textPage=textCount=0;gKeyStatusPtr=fe8_input_status();}
int fe8_event_start(u32 address){fe8_event_reset();const u16 *p=ResolveScript(address);if(!p)return -1;engine.pEventStart=engine.pEventIdk=engine.pEventCurrent=p;engine.execType=EV_EXEC_GAMEPLAY;engine.chapterIndex=gPlaySt.chapterIndex;running=1;return 0;}
int fe8_event_call(u32 address,int trigger){
 const u16 *p=ResolveScript(address);if(!p||running||blocked>=0||textId>=0||fe8_battle_busy())return -1;
 memset(&engine,0,sizeof(engine));memset(gEventActiveQueue,0,sizeof(gEventActiveQueue));engine.pEventStart=engine.pEventIdk=engine.pEventCurrent=p;engine.execType=EV_EXEC_GAMEPLAY;engine.chapterIndex=gPlaySt.chapterIndex;eventTriggerId=trigger;eventTriggerAddress=address;eventCallSerial++;SetFlag(trigger);if(trigger==3)SetFlag(132);running=1;return 0;
}
int fe8_event_flag(int id){return CheckFlag(id);}
void fe8_event_pack_permanent(u8 *out){memset(out,0,25);for(int i=0;i<200;i++)if(CheckFlag(101+i))out[i>>3]|=1u<<(i&7);}
void fe8_event_load_permanent(const u8 *data){for(int i=0;i<200;i++)eventFlags[101+i]=!!(data[i>>3]&(1u<<(i&7)));}

static void AddChapterJob(int kind){if((gPlaySt.chapterIndex!=0&&gPlaySt.chapterIndex!=1&&gPlaySt.chapterIndex!=2&&gPlaySt.chapterIndex!=3&&gPlaySt.chapterIndex!=4&&gPlaySt.chapterIndex!=5&&gPlaySt.chapterIndex!=6&&gPlaySt.chapterIndex!=7)||preview)return;if(chapterJobCount>=4){blocked=258;return;}chapterJobs[chapterJobCount].kind=kind;chapterJobs[chapterJobCount].actor=kind==2&&eventActor?(u8)eventActor->index:0;chapterJobs[chapterJobCount++].cursor=0;}
void fe8_event_phase(void){AddChapterJob(1);}
extern struct Unit *GetUnit(int index);
extern u16 CountRedUnits(void);
int AreAnyEnemyUnitDead(void);
extern u16 CountGreenUnits(void);
void fe8_event_post_action(void){
 if(fe8_battle_info(18)&&fe8_battle_info(9)){for(int side=3;side<=4;side++){struct Unit *u=GetUnit(fe8_battle_info(side));if(!UNIT_IS_VALID(u)||u->curHP)continue;if(UNIT_FACTION(u)==FACTION_RED&&(UNIT_CATTRIBUTES(u)&CA_BOSS))SetFlag(2);if(UNIT_FACTION(u)==FACTION_BLUE&&(UNIT_CATTRIBUTES(u)&CA_LORD))SetFlag(101);}}
 if(gPlaySt.chapterIndex==2||gPlaySt.chapterIndex==4){if(AreAnyEnemyUnitDead()==0)SetFlag(6);else ClearFlag(6);}
 AddChapterJob(2);
}
int fe8_event_busy(void){return running||blocked>=0||textId>=0||pendingItem||rewardValue;}
static int ShowText(int id){textCount=0;for(int i=0;i<sizeof(eventTextCounts)/sizeof(*eventTextCounts);i++)if(eventTextCounts[i].id==id)textCount=eventTextCounts[i].count;if(!textCount)return EVC_ERROR;textId=id;textPage=0;return EVC_ADVANCE_YIELD;}
void fe8_event_advance(void){if(textId>=0){textPage++;for(int i=0;i<ARRAY_COUNT(eventTextPauses);i++)if(eventTextPauses[i].id==textId&&eventTextPauses[i].page==textPage){pausedText=textId;pausedPage=textPage;textId=-1;return;}if(textPage>=textCount){textId=-1;textPage=textCount=0;pausedText=-1;}}}
void fe8_event_confirm(void){if(fe8_dialogue_confirm())fe8_event_advance();}
#include "generated/chapter_trigger_routines.h"
struct EvCheck03 {u32 unk0,script;u8 pidA,pidB;u16 fillerA,unkC,unkE;};
struct EvCheck05 {u32 unk0,script;u8 x,y;u16 cmdId;};
#define TILE_COMMAND_CHEST 0x14
#include "generated/progress_loca.h"
#include "generated/progress_keyevents.h"
#include "generated/shop_assets.h"
#include "constants/items.h"
extern int GetUnitItemSlot(struct Unit *u,int item);
#define TILE_COMMAND_SECRET 0x18
#include "generated/shop_events.h"
static int FindTileEvent(int x,int y,struct EventInfo *info){
 if(preview||x<0||y<0||x>=fe8_map_info(1)||y>=fe8_map_info(2))return 0;const u32 *list=gPlaySt.chapterIndex==7?EventListScr_Ch6_Location:gPlaySt.chapterIndex==5?EventListScr_Ch5x_Location:gPlaySt.chapterIndex==6?EventListScr_Ch5_Location:gPlaySt.chapterIndex==4?EventListScr_Ch4_Location:gPlaySt.chapterIndex==3?EventListScr_Ch3_Location:gPlaySt.chapterIndex==1?EventListScr_Ch1_Location:gPlaySt.chapterIndex==2?EventListScr_Ch2_Location:NULL;int words=gPlaySt.chapterIndex==7?ARRAY_COUNT(EventListScr_Ch6_Location):gPlaySt.chapterIndex==5?ARRAY_COUNT(EventListScr_Ch5x_Location):gPlaySt.chapterIndex==6?ARRAY_COUNT(EventListScr_Ch5_Location):gPlaySt.chapterIndex==4?ARRAY_COUNT(EventListScr_Ch4_Location):gPlaySt.chapterIndex==3?ARRAY_COUNT(EventListScr_Ch3_Location):gPlaySt.chapterIndex==1?ARRAY_COUNT(EventListScr_Ch1_Location):ARRAY_COUNT(EventListScr_Ch2_Location);if(!list)return 0;
 for(int i=0;i+2<words&&list[i];i+=3){const u32 *r=list+i;int code=r[0]&65535;if(CheckFlag(r[0]>>16)||(code!=5&&code!=6&&code!=7&&code!=8&&code!=10))continue;*info=(struct EventInfo){.listScript=r,.xPos=x,.yPos=y};if((code==7?EvCheck07_CHES(info):code==8?EvCheck08_DOOR(info):code==10?EvCheck0A_SHOP(info):EvCheck05_LOCA(info))&&info->script){if((info->commandId==0x14&&fe8_map_terrain(x,y)!=33)||(info->commandId==0x12&&fe8_map_terrain(x,y)!=30))continue;if(code==6)info->givenMoney=3;return 1;}}return 0; // Original EvCheck06_VILL lacks a C return: defined equivalent of its LOCA match + givenMoney=3.
}
int fe8_event_shop_item(int x,int y,int row){struct EventInfo info;if(row<0||!FindTileEvent(x,y,&info)||(info.commandId!=0x16&&info.commandId!=0x17))return 0;for(int i=0;i<ARRAY_COUNT(shopLists);i++)if(shopLists[i].address==info.script&&row<shopLists[i].count)return fe8_unit_make_item(shopLists[i].items[row]);return 0;}
int fe8_event_tile_command(int x,int y){struct EventInfo info;return FindTileEvent(x,y,&info)?info.commandId:0;}
int fe8_event_unlock(int index,int x,int y){struct EventInfo info;struct Unit *u=GetUnit(index);if(!UNIT_IS_VALID(u)||fe8_event_busy()||!FindTileEvent(x,y,&info)||(info.commandId!=0x14&&info.commandId!=0x12))return -1;int change=fe8_map_change_at(x,y);if(change<0)return -2;if(info.commandId==0x14&&!info.givenItem)return -3;if(fe8_map_change(change,0)<0)return -4;SetFlag(info.flag);if(info.commandId==0x14)fe8_event_reward(index,info.givenItem==0x77?info.givenMoney:info.givenItem,info.givenItem==0x77);return 0;} // Native equivalent of known fixed-item chest / no-script door branch; random/scripted chests remain refused.
int fe8_event_visit(int index){struct Unit *u=GetUnit(index);struct EventInfo info;if(!UNIT_IS_VALID(u)||fe8_event_busy()||!FindTileEvent(u->xPos,u->yPos,&info)||info.commandId!=0x10)return -1;int id=info.givenMoney==3?fe8_map_change_at(u->xPos,u->yPos):-1;if(info.givenMoney==3&&id<0)return -2;fe8_event_actor(index);if(fe8_event_call(info.script,info.flag)<0)return -3;if(id>=0)fe8_map_change(id,0);return 0;}
static int FindCharacterEvent(int actor,int target,struct EventInfo *info){
 struct Unit *a=GetUnit(actor),*b=GetUnit(target);if(preview||actor<1||actor>255||target<1||target>255||!UNIT_IS_VALID(a)||!UNIT_IS_VALID(b))return 0;
 const u32 *list=gPlaySt.chapterIndex==6?EventListScr_Ch5_Character:gPlaySt.chapterIndex==4?EventListScr_Ch4_Character:gPlaySt.chapterIndex==3?EventListScr_Ch3_Character:gPlaySt.chapterIndex==1?EventListScr_Ch1_Character:gPlaySt.chapterIndex==2?EventListScr_Ch2_Character:NULL;int words=gPlaySt.chapterIndex==6?ARRAY_COUNT(EventListScr_Ch5_Character):gPlaySt.chapterIndex==4?ARRAY_COUNT(EventListScr_Ch4_Character):gPlaySt.chapterIndex==3?ARRAY_COUNT(EventListScr_Ch3_Character):gPlaySt.chapterIndex==1?ARRAY_COUNT(EventListScr_Ch1_Character):ARRAY_COUNT(EventListScr_Ch2_Character);if(!list)return 0;
 for(int i=0;i+3<words&&list[i];i+=4){const u32 *r=list+i;if((r[0]&65535)!=3||CheckFlag(r[0]>>16))continue;*info=(struct EventInfo){.listScript=r,.pidA=a->pCharacterData->number,.pidB=b->pCharacterData->number};if(EvCheck03_CHAR(info))return 1;}return 0;
}
int fe8_event_talk_available(int actor,int target){struct EventInfo info;return FindCharacterEvent(actor,target,&info);}
int fe8_event_talk(int actor,int target){struct EventInfo info;if(fe8_event_busy()||!FindCharacterEvent(actor,target,&info))return -1;fe8_event_actor(actor);return fe8_event_call(info.script,info.flag);}
int fe8_event_seize(int index){struct Unit *u=GetUnit(index);if(!UNIT_IS_VALID(u)||fe8_event_tile_command(u->xPos,u->yPos)!=0x11||fe8_event_busy())return -1;fe8_event_actor(index);if(fe8_event_call(gPlaySt.chapterIndex==5?EventScr_Ch5x_EndingScene_ADDRESS:gPlaySt.chapterIndex==3?EventScr_Ch3_EndingScene_ADDRESS:EventScr_Ch1_EndingScene_ADDRESS,3)<0)return -1;SetFlag(132);return 0;}

int fe8_event_pillage(int index){
 struct Unit *u=GetUnit(index);struct EventInfo info;if((gPlaySt.chapterIndex!=2&&gPlaySt.chapterIndex!=4&&gPlaySt.chapterIndex!=6)||!UNIT_IS_VALID(u)||UNIT_FACTION(u)!=FACTION_RED||fe8_event_busy()||!FindTileEvent(u->xPos,u->yPos-1,&info)||info.commandId!=0x20)return -1;
 int id=fe8_map_change_at(u->xPos,u->yPos-1);if(id<0||fe8_map_change(id,0)<0)return -1;fe8_event_actor(index);fe8_audio_se(0xab);return ShowText(0x12)==EVC_ERROR?-1:0;
} // Original AiPillageAction/StartAvailableTileEvent TILE_COMMAND_20 effects; popup/tile fade presentation adapted.
int fe8_event_actor_portrait(void){return eventActor&&UNIT_IS_VALID(eventActor)?eventActor->pCharacterData->portraitId:0;}
void fe8_event_actor(int index){eventActor=index>0&&index<256?GetUnit(index):NULL;}
static struct Unit *EventUnit(int pid){
 pid=(s16)pid;if(pid<-3)return NULL;if(pid==-3)pid=(u16)gEventSlots[2];
 else if(pid==-2){int x=gEventSlots[11]&65535,y=gEventSlots[11]>>16;if(x>63||y>63)return NULL;for(int i=1;i<256;i++){struct Unit *u=GetUnit(i);if(UNIT_IS_VALID(u)&&!(u->state&US_NOT_DEPLOYED)&&u->xPos==x&&u->yPos==y)return u;}return NULL;}
 else if(pid==-1)return eventActor;else if(pid==0)pid=1;
 return fe8_unit_by_pid(pid);
}
static struct Unit *GetUnitStructFromEventParameter(s16 pid){return EventUnit(pid);}
extern int GetUnitLuck(struct Unit *u);
#define FACTION_ID_BLUE 0
#define FACTION_ID_GREEN 1
#define FACTION_ID_RED 2
#include "generated/progress_gold.h"
static void SetNextChapterId(int id){requestedChapter=id;}
static void SetNextGameActionId(int action){progressAction=action;}
static void GotoChapterWithoutSave(int id){(void)id;progressError=1;} // MNC3 is not integrated.
enum {GAME_ACTION_EVENT_RETURN=0,GAME_ACTION_CLASS_REEL=1,GAME_ACTION_USR_SKIPPED=2,GAME_ACTION_PLAYED_THROUGH=3};
static void StartBgmExt(int song,int speed,void *p){(void)p;fe8_audio_bgm(song,speed);}
static void StartBgmFadeIn(int song,int speed,int p){(void)p;fe8_audio_fadein(song,speed);}
static void Sound_FadeOutBGM(int speed){fe8_audio_fadeout(speed);}
static void OverrideBgm(int song){fe8_audio_override(song);}
static void _RestoreBgm(int speed){fe8_audio_restore(speed);}
static void DeleteAll6CWaitMusicRelated(void){} // Native audio boundary cancels pending transitions.
static void StartBgmVolumeChange(int from,int to,int frames,void *p){if(p)audioWait=frames;fe8_audio_volume(from,to,frames);}
static void Sound_SetSEVolume(int volume){fe8_audio_volume(volume,volume,1);}
static void PlaySoundEffect(int song){fe8_audio_se(song);}
#include "generated/audio_events.h"
#include "generated/progress_events.h"
#include "generated/progress_area.h"
static int Command(struct EventEngineProc *p){
 const u16 *s=p->pEventCurrent;int code=s[0]>>8,sub=s[0]&15,a=(s16)s[1];
 switch(code){
 case 0:return Event00_NULL(p);case 1:return Event01_End(p);case 2:return Event02_EvBitAndIdMod(p);case 3:return Event03_CheckEvBitOrId(p);case 4:return Event04_CheckRandom(p);
 case 5:if(s[1]>=14)return EVC_ERROR;return Event05_SetSlot(p);
 case 6:{int src=s[1]>>8&15;if((s[1]&15)>=14||(s[1]>>4&15)>=14||src>=14)return EVC_ERROR;if((sub==3||sub==4)&&!gEventSlots[src])return EVC_ERROR;if((sub==8||sub==9)&&gEventSlots[src]>=32)return EVC_ERROR;return Event06_SlotOperation(p);}
 case 7:if((sub==0||sub==2)&&s[1]>=14)return EVC_ERROR;return Event07_SlotQueueOperations(p);
 case 8:return Event08_Label(p);case 9:return Event09_Goto(p);case 10:return Event0A_Call(p);
 case 12:if(s[2]>=14||s[3]>=14)return EVC_ERROR;return Event0C_Branch(p);
 case 13:if(!preview&&p->chapterIndex==5&&(EVT_CMD_ARG32_BE(s)&~1u)==HandleCh5xUnits_Start_ADDRESS){HandleCh5xUnits_Start();return EVC_ADVANCE_CONTINUE;}if(!preview&&p->chapterIndex==5&&(EVT_CMD_ARG32_BE(s)&~1u)==HandleCh5xUnits_End_ADDRESS){HandleCh5xUnits_End();return EVC_ADVANCE_CONTINUE;}if((EVT_CMD_ARG32_BE(s)&~1u)==BmGuideTextSetAllGreen_ADDRESS){BmGuideTextSetAllGreen();return EVC_ADVANCE_CONTINUE;}if(!preview&&(EVT_CMD_ARG32_BE(s)&~1u)==EventCallGameOverExt_ADDRESS)return fe8_gameover_start()<0?EVC_ERROR:EVC_ADVANCE_YIELD;return EVC_ERROR;
 case 14:return Event0E_STAL(p);
 case 16:return Event10_ModifyEvBit(p);case 17:return Event11_SetIgnoredKeys(p);
 case 18:requests=code;return Event12_StartBGM(p);case 19:requests=code;return Event13_BgmFadeIn(p);case 20:requests=code;return Event14_BgmOverideRestore(p);case 21:requests=code;return Event15_BgmVolume(p);case 22:requests=code;return Event16_(p);
 case 23:return Event17_Fade(p);
 case 25: // Original CHECK family needed by prologue and tutorial guards.
  switch(sub){case EVSUBCMD_CHECK_MODE:gEventSlots[12]=gPlaySt.chapterModeIndex;break;case EVSUBCMD_CHECK_CHAPTER_NUMBER:gEventSlots[12]=p->chapterIndex;break;case EVSUBCMD_CHECK_HARD:gEventSlots[12]=!!(gPlaySt.chapterStateBits&PLAY_FLAG_HARD);break;case EVSUBCMD_CHECK_TURNS:gEventSlots[12]=gPlaySt.chapterTurnNumber;break;case EVSUBCMD_CHECK_ENEMIES:gEventSlots[12]=CountRedUnits();break;case EVSUBCMD_CHECK_OTHERS:gEventSlots[12]=CountGreenUnits();break;case EVSUBCMD_19CHECK_EVENTID:gEventSlots[12]=eventTriggerId;break;case EVSUBCMD_CHECK_TUTORIAL:gEventSlots[12]=!gPlaySt.config.controller&&!(gPlaySt.chapterStateBits&PLAY_FLAG_HARD);break;default:return EVC_ERROR;}return EVC_ADVANCE_CONTINUE;
 case 26:return EVC_ADVANCE_CONTINUE; // Text presentation setup is supplied by the temporary DOM frontend.
 case 27:if(sub==2){pausedText=-1;return EVC_ADVANCE_CONTINUE;}if(a==0)return EVC_ADVANCE_CONTINUE;return ShowText(a<0?gEventSlots[2]:s[1]);
 case 28:if(pausedText<0)return EVC_ERROR;textId=pausedText;textPage=pausedPage;pausedText=-1;return EVC_ADVANCE_YIELD;
 case 33:if(sub==0){int bg=a<0?gEventSlots[2]:a;return fe8_background_set(bg)<0?EVC_ERROR:EVC_ADVANCE_YIELD;}return EVC_ADVANCE_CONTINUE;
 case 34:fe8_background_set(-1);return EVC_ADVANCE_CONTINUE;
 case 29:case 32:case 35:case 36:return EVC_ADVANCE_CONTINUE; // Dialogue/background/face display boundaries, no data or gameplay effects.
 case 37:{int chapter=a<0?gEventSlots[2]:a;if(fe8_map_start(chapter)<0)return EVC_ERROR;gPlaySt.chapterIndex=chapter;fe8_map_focus(gEventSlots[11]&65535,gEventSlots[11]>>16);if(p->evStateBits&EV_STATE_FADEDIN)SetDispEnable(0,0,0,0,0);return EVC_ADVANCE_YIELD;}
 case 38:{struct Unit *u=(sub&7)==1?EventUnit(a):NULL;if((sub&7)==1&&!u)return EVC_ERROR;fe8_map_focus(u?u->xPos:s[1]&255,u?u->yPos:s[1]>>8);return EVC_ADVANCE_YIELD;}
 case 39:{int id=a;if(id==-1)id=fe8_map_change_at(gEventSlots[11]&65535,gEventSlots[11]>>16);else if(id==-2){if(!eventActor)return EVC_ERROR;id=fe8_map_change_at(eventActor->xPos,eventActor->yPos);}if(id<0)return EVC_ERROR;return fe8_map_change(id,sub&1)<0?EVC_ERROR:EVC_ADVANCE_CONTINUE;}
 case 42:{int to=a<0?(int)gEventSlots[2]:a;if(preview||!(((gPlaySt.chapterIndex==0&&to==1||gPlaySt.chapterIndex==6&&to==5)&&sub==EVSUBCMD_MNC2)||((gPlaySt.chapterIndex==1&&to==0x38||gPlaySt.chapterIndex==0x38&&to==2||gPlaySt.chapterIndex==2&&to==3||(gPlaySt.chapterIndex==3||gPlaySt.chapterIndex==27&&engine.chapterIndex==3)&&to==4||gPlaySt.chapterIndex==4&&to==6||engine.chapterIndex==5&&gPlaySt.chapterIndex==8&&to==7)&&sub==EVSUBCMD_MNCH)))return EVC_ERROR;return Event2A_MoveToChapter(p);}
 case 43:if(sub!=2)return EVC_ERROR;noRedaNext=1;return EVC_ADVANCE_CONTINUE;
 case 62:if(gPlaySt.chapterIndex!=4&&gPlaySt.chapterIndex!=6&&gPlaySt.chapterIndex!=7)return EVC_ERROR;return fe8_units_deploy_existing(gPlaySt.chapterIndex==7?UnitDef_Event_Ch6Ally_ADDRESS:gPlaySt.chapterIndex==6?UnitDef_Event_Ch5Ally_ADDRESS:UnitDef_Event_Ch4Ally_ADDRESS)<0?EVC_ERROR:EVC_ADVANCE_YIELD; // Native fixed original deployment; preparation UI remains pending.
 case 46:if(sub==1){gEventSlots[12]=eventActor?eventActor->pCharacterData->number:0;return EVC_ADVANCE_CONTINUE;}return EVC_ERROR;
 case 51:if(sub==4&&!eventActor)return EVC_ERROR;return Event33_CheckUnitVarious(p);
 case 50:return Event32_SpawnSingleUnit(p);
 case 44:{u32 address=EVT_CMD_ARG32_LE(s);if((s32)address<0)address=gEventSlots[2];int noReda=noRedaNext;noRedaNext=0;return fe8_units_load_event_config(address,1,sub==1||a==2,noReda)<0?EVC_ERROR:EVC_ADVANCE_YIELD;}
 case 47:{struct Unit *u=EventUnit(s[2]);if(!u)return EVC_ADVANCE_CONTINUE;int x=u->xPos,y=u->yPos;
  switch(sub&7){case 0:x=s[3]&255;y=s[3]>>8;break;case 1:{struct Unit *target=EventUnit(s[3]);if(!target)return EVC_ERROR;x=target->xPos;y=target->yPos;break;}
  case 2:if(s[3]==0)x--;if(s[3]==1)x++;if(s[3]==2)y++;if(s[3]==3)y--;break;
  case 3:return fe8_unit_move_queue_index((u8)u->index,queue,gEventSlots[13])<0?EVC_ERROR:EVC_ADVANCE_CONTINUE;
  default:return EVC_ERROR;}if((sub&8)&&fe8_unit_at(x,y)>0)return EVC_ERROR;return fe8_unit_move_index((u8)u->index,x,y)<0?EVC_ERROR:EVC_ADVANCE_CONTINUE;}
 case 48:return fe8_units_busy()?EVC_STOP_YIELD:EVC_ADVANCE_YIELD;
 case 52:{struct Unit *u=EventUnit(a);if(sub>=10&&sub<=12){fe8_units_clear_faction(sub==10?0:sub==11?64:128);return EVC_ADVANCE_CONTINUE;}if(!u)return EVC_ADVANCE_CONTINUE;
  if(sub>=2&&sub<=4){int old=(u8)u->index,next=fe8_unit_change_faction(old,(sub-2)*64);if(next<0)return EVC_ERROR;if(eventActor==u)fe8_event_actor(next);}else if(sub==0){u->state|=US_HIDDEN|US_BIT16|US_BIT26;}else if(sub==1){u->state&=~(US_HIDDEN|US_BIT16|US_BIT26);}else if(sub==13){u->state|=US_HIDDEN;}else if(sub==5){u->curHP=gEventSlots[1]<u->maxHP?gEventSlots[1]:u->maxHP;if(!u->curHP)u->state|=US_DEAD;}
  else if(sub==6)u->state|=US_UNSELECTABLE;else if(sub==7)u->state|=US_HAS_MOVED;else if(sub==8){if(gEventSlots[1]==1)u->state&=~US_NOT_DEPLOYED;else if(!gEventSlots[1])u->state|=US_NOT_DEPLOYED;else if((s32)gEventSlots[1]==-1){if(u->state&US_BIT21)u->state|=US_NOT_DEPLOYED;else u->state&=~US_NOT_DEPLOYED;}else return EVC_ERROR;}else if(sub==15||sub==14)fe8_unit_remove_index((u8)u->index);
  else return EVC_ERROR;return EVC_ADVANCE_CONTINUE;}
 case 54:return Event36_CheckInArea(p);
 case 55:{struct Unit *u=EventUnit(a);if(!u)return EVC_ERROR;if(sub==1){if(UNIT_FACTION(u)==FACTION_BLUE)SetPartyGoldAmount(GetPartyGoldAmount()+gEventSlots[3]);return EVC_ADVANCE_YIELD;}if(sub!=0)return EVC_ERROR;if(itemResolved){itemResolved=0;return EVC_ADVANCE_YIELD;}if(fe8_unit_give(u->pCharacterData->number,gEventSlots[3])>0)return EVC_ADVANCE_YIELD;pendingItem=fe8_unit_make_item(gEventSlots[3]);if(!pendingItem)return EVC_ERROR;pendingUnit=(u8)u->index;pendingChoice=5;pendingOrigin=1;return EVC_STOP_YIELD;} // Native inventory-full choice replaces original popup/ConvoyMenu Procs; no item is silently discarded.
 case 57:{if(sub!=0)return EVC_ERROR;ChangeAiForCharacter(a<0?gEventSlots[2]:a,gEventSlots[1]&255,gEventSlots[1]>>8&255,gEventSlots[1]>>16&255);return EVC_ADVANCE_CONTINUE;}
 case 58:if(sub==0&&s[2])fe8_audio_se(s[2]);return ShowText(s[1]); // Adapted notification/brown-box presentation.
 case 61:if(sub>2)return EVC_ERROR;return Event3D_MenuOverride(p);
 case 63:if(sub==2)return fe8_battle_set_script(queue,gEventSlots[13])<0?EVC_ERROR:EVC_ADVANCE_CONTINUE;else if(sub<=1){struct Unit *a=EventUnit((s16)s[1]<0?-2:s[1]),*b=EventUnit((s16)s[2]<0?-2:s[2]);if(!UNIT_IS_VALID(a)||!UNIT_IS_VALID(b))return EVC_ERROR;return fe8_battle_start(a->pCharacterData->number,b->pCharacterData->number,s[3]&255,s[3]>>8,queue,gEventSlots[13])<0?EVC_ERROR:EVC_ADVANCE_YIELD;}return EVC_ERROR;
 case 59:{if((sub&3)==2)return EVC_ADVANCE_CONTINUE;struct Unit *u=(sub&3)==1?EventUnit(a):NULL;int x=u?u->xPos:s[1]&255,y=u?u->yPos:s[1]>>8;fe8_map_focus(x,y);return EVC_ADVANCE_CONTINUE;}
 case 65:if(p->chapterIndex!=7||(sub!=0&&sub!=1&&sub!=15))return EVC_ERROR;requests=65;return EVC_ADVANCE_YIELD; // Warp presentation boundary; original REMU/REVEAL/STAL retain unit/timing effects.
 case 166:if(preview||s[1]!=0||s[2]!=0||!((p->chapterIndex==5&&s[3]==5)||(p->chapterIndex==6&&s[3]==8)))return EVC_ERROR;requests=166;return EVC_ADVANCE_CONTINUE; // Known original WM placements; travel presentation remains adapted.
 default:return EVC_ERROR;
 }
}
int fe8_event_item_info(int field){return field==0?pendingItem:field==1?pendingUnit:field==2?pendingChoice:field==3?((gPlaySt.chapterIndex==2||gPlaySt.chapterIndex==3||gPlaySt.chapterIndex==4||gPlaySt.chapterIndex==6||gPlaySt.chapterIndex==7)&&fe8_convoy_count()<100):field==4?fe8_convoy_count():field==5?lastRewardItem:field==6?lastRewardUnit:field==7?rewardSerial:-1;}
int fe8_event_item_choose(int slot){if(!pendingItem||slot<0||slot>5)return -1;struct Unit *u=GetUnit(pendingUnit);if(!UNIT_IS_VALID(u))return -2;int item=slot==5?pendingItem:u->items[slot];if(!item)return -3;if(fe8_event_item_info(3)&&fe8_convoy_add(item)<0)return -4;if(slot<5)u->items[slot]=pendingItem;pendingItem=0;itemResolved=pendingOrigin;pendingOrigin=0;fe8_audio_se(0x6a);return 0;}
void fe8_event_reward(int unit,int item,int gold){if(rewardValue||pendingItem){blocked=263;return;}rewardUnit=unit;rewardValue=item;rewardGold=gold;}
void fe8_event_tick(int keys){
 if(pendingItem){if(keys&192)pendingChoice=(pendingChoice+((keys&128)?1:5))%6;if(keys&1)fe8_event_item_choose(pendingChoice);return;}
 if(textId>=0){if(keys&(1|2|8))fe8_event_confirm();return;}
 if(rewardValue&&!fe8_battle_busy()&&blocked<0){struct Unit *u=GetUnit(rewardUnit);if(!UNIT_IS_VALID(u)){blocked=263;return;}lastRewardUnit=rewardUnit;lastRewardItem=rewardGold?-rewardValue:fe8_unit_make_item(rewardValue);rewardSerial++;fe8_audio_se(0x5a);if(rewardGold)SetPartyGoldAmount(GetPartyGoldAmount()+rewardValue);else if(fe8_unit_give(u->pCharacterData->number,rewardValue)<=0){pendingItem=lastRewardItem;pendingUnit=rewardUnit;pendingChoice=5;pendingOrigin=0;}rewardValue=0;return;}
 if(!running||blocked>=0||EventFadePending()||fe8_battle_busy())return;
 if(audioWait>0){audioWait--;return;}
 for(int budget=0;budget<128;budget++){
  int remaining=ScriptRemaining(engine.pEventCurrent),length=remaining?EVT_CMD_LEN(engine.pEventCurrent):0;
  if(length<2||length>remaining){blocked=256;return;}
  int code=engine.pEventCurrent[0]>>8;gEventSlots[0]=0;int result=Command(&engine);executed++;
  if(blocked>=0)return;
  if(result==EVC_ADVANCE_CONTINUE||result==EVC_ADVANCE_YIELD)engine.pEventCurrent+=EVT_CMD_LEN(engine.pEventCurrent);
  if(result==EVC_ERROR){blocked=code;return;}if(result==EVC_END){fe8_background_set(-1);running=0;return;} // Native gameplay event teardown restores the map presentation.
  if(result==EVC_ADVANCE_YIELD||result==EVC_STOP_YIELD)return;
 }
 blocked=257; // Bound work per update; malformed loops cannot hang the browser.
}
static void DispatchChapterJobs(void){
 if(running||blocked>=0||textId>=0||pendingItem||rewardValue||fe8_battle_busy())return;
 while(chapterJobCount){
  int chapter=gPlaySt.chapterIndex;int kind=chapterJobs[0].kind;
  const u32 *list=chapter==7?(kind==1?EventListScr_Ch6_Turn:EventListScr_Ch6_Misc):chapter==5?(kind==1?EventListScr_Ch5x_Turn:EventListScr_Ch5x_Misc):chapter==6?(kind==1?EventListScr_Ch5_Turn:EventListScr_Ch5_Misc):chapter==4?(kind==1?EventListScr_Ch4_Turn:EventListScr_Ch4_Misc):chapter==3?(kind==1?EventListScr_Ch3_Turn:EventListScr_Ch3_Misc):chapter==2?(kind==1?EventListScr_Ch2_Turn:EventListScr_Ch2_Misc):chapter==1?(kind==1?EventListScr_Ch1_Turn:EventListScr_Ch1_Misc):(kind==1?EventListScr_Prologue_Turn:EventListScr_Prologue_Misc);
  int words=chapter==7?(kind==1?sizeof(EventListScr_Ch6_Turn)/4:sizeof(EventListScr_Ch6_Misc)/4):chapter==5?(kind==1?sizeof(EventListScr_Ch5x_Turn)/4:sizeof(EventListScr_Ch5x_Misc)/4):chapter==6?(kind==1?sizeof(EventListScr_Ch5_Turn)/4:sizeof(EventListScr_Ch5_Misc)/4):chapter==4?(kind==1?sizeof(EventListScr_Ch4_Turn)/4:sizeof(EventListScr_Ch4_Misc)/4):chapter==3?(kind==1?sizeof(EventListScr_Ch3_Turn)/4:sizeof(EventListScr_Ch3_Misc)/4):chapter==2?(kind==1?sizeof(EventListScr_Ch2_Turn)/4:sizeof(EventListScr_Ch2_Misc)/4):chapter==1?(kind==1?sizeof(EventListScr_Ch1_Turn)/4:sizeof(EventListScr_Ch1_Misc)/4):(kind==1?sizeof(EventListScr_Prologue_Turn)/4:sizeof(EventListScr_Prologue_Misc)/4);
  eventActor=chapterJobs[0].actor?GetUnit(chapterJobs[0].actor):NULL;
  int cursor=chapterJobs[0].cursor;
  if(cursor>=words||!list[cursor]){for(int i=1;i<chapterJobCount;i++)chapterJobs[i-1]=chapterJobs[i];chapterJobCount--;continue;}
  if(cursor+3>words){blocked=259;return;}
  int code=list[cursor]&65535,flag=list[cursor]>>16;chapterJobs[0].cursor+=3;
  if(CheckFlag(flag))continue;
  struct EventInfo info={.listScript=list+cursor};int matches=code==2?EvCheck02_TURN(&info):code==1?EvCheck01_AFEV(&info):code==11?(eventActor?EvCheck0B_AREA(&info):0):-1;
  if(matches<0){blocked=260;return;}if(!matches)continue;
  if(fe8_event_call(info.script,info.flag)<0){blocked=261;return;}return;
 }
}
int fe8_event_info(int field){switch(field){case 0:return running;case 1:return blocked;case 2:return textId;case 3:return textPage;case 4:return textCount;case 5:return executed;case 6:return preview;case 7:return requests;case 8:return engine.pEventCurrent?engine.pEventCurrent[0]>>8:-1;case 9:return engine.pEventCurrent?engine.pEventCurrent[1]:-1;case 11:return eventTriggerId;case 12:return chapterJobCount;case 13:return eventTriggerAddress;case 14:return eventCallSerial;case 10:return engine.pEventCurrent&&ScriptRemaining(engine.pEventCurrent)>=4?EVT_CMD_ARG32_BE(engine.pEventCurrent):0;default:if(field>=20&&field<34)return gEventSlots[field-20];return -1;}}
int fe8_event_preview(void){
 // Explicit preview of original post-cutscene unit definitions, not a claim that skipped battles/tutorials ran.
 fe8_event_reset();preview=1;gPlaySt.chapterIndex=0;fe8_units_reset();fe8_map_start(0);
 if(fe8_units_load(0x088B3C14,0)<0||fe8_units_load(0x088B3C50,0)<0)return -1;
 fe8_unit_move(2,4,4);fe8_unit_move(1,4,5);struct Unit *seth=fe8_unit_by_pid(2);if(seth)seth->curHP=13;
 fe8_unit_give(1,0x09);fe8_map_focus(4,5);return 0;
}
void fe8_chapter_start(void){
 int chapter=(u8)gPlaySt.chapterIndex;if(chapter==7){gPlaySt.chapterVisionRange=progressChapters[7].fog;gPlaySt.chapterWeatherId=progressChapters[7].weather;}fe8_event_reset();fe8_map_start(chapter);
 int count=fe8_units_restore(gPlaySt.gameSaveSlot);fe8_event_load_permanent(fe8_save_loaded_flags());fe8_save_checkpoint_capture();
 if(chapter==0&&count==0){fe8_event_start(EventScr_Prologue_BeginningScene_ADDRESS);engine.evStateBits|=EV_STATE_FADEDIN;SetDispEnable(0,0,0,0,0);}
 else if(chapter==1){fe8_event_start(EventScr_Ch1_BeginningScene_ADDRESS);}
 else if(chapter==7){fe8_event_start(EventScr_Ch6_BeginningScene_ADDRESS);}
 else if(chapter==5){fe8_event_start(EventScr_Ch5x_BeginningScene_ADDRESS);}
 else if(chapter==6){fe8_event_start(EventScr_Ch5_BeginningScene_ADDRESS);}
 else if(chapter==4){fe8_event_start(EventScr_Ch4_BeginningScene_ADDRESS);}
 else if(chapter==3){fe8_event_start(EventScr_Ch3_BeginningScene_ADDRESS);}
 else if(chapter==2){fe8_event_start(EventScr_Ch2_BeginningScene_ADDRESS);}
 else if(chapter==0x38){fe8_event_start(EventScr_CastleFelia_BeginningScene_ADDRESS);}
 fe8_event_load_permanent(fe8_save_loaded_flags());
}
static void ApplyRequestedChapter(void){
 if(requestedChapter<0||fe8_event_busy()||EventFadePending()||fe8_units_busy())return;
 int to=requestedChapter;requestedChapter=-1;if(!(((gPlaySt.chapterIndex==0&&to==1||gPlaySt.chapterIndex==6&&to==5)&&progressAction==GAME_ACTION_USR_SKIPPED)||((gPlaySt.chapterIndex==1&&to==0x38||gPlaySt.chapterIndex==0x38&&to==2||gPlaySt.chapterIndex==2&&to==3||(gPlaySt.chapterIndex==3||gPlaySt.chapterIndex==27&&engine.chapterIndex==3)&&to==4||gPlaySt.chapterIndex==4&&to==6||engine.chapterIndex==5&&gPlaySt.chapterIndex==8&&to==7)&&progressAction==GAME_ACTION_CLASS_REEL))){blocked=42;progressError=2;return;}
 progressFrom=engine.chapterIndex;progressTo=to;lastWin=CheckFlag(3);lastBoss=CheckFlag(2);
 for(int i=0;i<3;i++)NextRN();StoreRNState(monsterRN);
 fe8_units_chapter_cleanup();u8 permanent[412];memcpy(permanent,eventFlags+100,sizeof(permanent));
 fe8_event_reset();memcpy(eventFlags+100,permanent,sizeof(permanent));
 gPlaySt.chapterIndex=to;gPlaySt.faction=FACTION_GREEN;gPlaySt.chapterTurnNumber=0;gPlaySt.chapterTotalSupportGain=0;
 gPlaySt.chapterVisionRange=progressChapters[to].fog;gPlaySt.chapterWeatherId=progressChapters[to].weather;gPlaySt.tutorial_counter=gPlaySt.tutorial_exec_type=0;gPlaySt.unk48=gPlaySt.unk4A_1=0;
 fe8_map_start(to);for(int i=1;i<64;i++){struct Unit *u=GetUnit(i);if(UNIT_IS_VALID(u)&&!(u->state&US_BIT21))u->state&=~US_NOT_DEPLOYED;}
 fe8_save_checkpoint_capture();
 const u16 *p=ResolveScript(to==7?EventScr_Ch6_BeginningScene_ADDRESS:to==5?EventScr_Ch5x_BeginningScene_ADDRESS:to==1?EventScr_Ch1_BeginningScene_ADDRESS:to==2?EventScr_Ch2_BeginningScene_ADDRESS:to==3?EventScr_Ch3_BeginningScene_ADDRESS:to==4?EventScr_Ch4_BeginningScene_ADDRESS:to==6?EventScr_Ch5_BeginningScene_ADDRESS:EventScr_CastleFelia_BeginningScene_ADDRESS);memset(&engine,0,sizeof(engine));engine.pEventStart=engine.pEventIdk=engine.pEventCurrent=p;engine.execType=EV_EXEC_GAMEPLAY;engine.chapterIndex=to;running=1;progressCount++;
 if(to==0x38||to==2||to==3||to==4||to==5||to==6||to==7){engine.evStateBits|=EV_STATE_FADEDIN;gLCDControlBuffer.blendY=16;gLCDControlBuffer.blendMode=3;}
 // MNCH world-map travel/narration and save menu are not ported: adapted direct entry.
 // Original chapter-intro title/graphics are not yet ported: adapted immediate map presentation.
 SetDispEnable(1,1,1,1,1);
}
void fe8_chapter_tick(int keys){fe8_save_playtime_tick();fe8_gameover_tick(keys);if(fe8_gameover_info(0)>0)return;fe8_battle_tick();fe8_units_tick();fe8_dialogue_tick();fe8_event_tick(keys);ApplyRequestedChapter();DispatchChapterJobs();if(!fe8_event_busy()&&(fe8_map_info(0)==0||fe8_map_info(0)==1||fe8_map_info(0)==2||fe8_map_info(0)==3||fe8_map_info(0)==4||fe8_map_info(0)==5||fe8_map_info(0)==6||fe8_map_info(0)==7)&&fe8_tactical_info(0)<0)fe8_tactical_start();fe8_tactical_tick(keys);}

void fe8_event_test_tick(int keys){Proc_Run(gProcTreeRootArray[4]);fe8_chapter_tick(keys);fe8_map_tick(0);}

int fe8_event_fade_level(void){return gLCDControlBuffer.blendY;}
int fe8_event_fade_mode(void){return gLCDControlBuffer.blendMode;}

void fe8_event_set_flag(int flag){SetFlag(flag);}
int fe8_event_quote(int msg){if(running||textId>=0||blocked>=0||msg<0||ShowText(msg)==EVC_ERROR){blocked=262;return -1;}return 0;}
