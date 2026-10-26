#include "map.h"
#include "fog.h"
#include "background.h"
#include "dialogue.h"
#include "battle.h"
#include "tactical.h"
#include "generated/menu_types.h"
#include <string.h>
struct Vec2 {short x,y;};
// Only native map state used by the extracted routines; no MMIO or CPU interpreter.
struct BmSt {u8 gameStateBits;struct Vec2 camera,playerCursor,cursorPrevious,cursorTarget,playerCursorDisplay,cameraMax; s16 unk32,unk34;s8 unk36,unk37;};
static struct BmSt gBmSt;
static struct Vec2 gBmMapSize;
static u16 baseCells[64][64],*gBmMapBaseTiles[64],sTilesetConfig[0x1200];
static u8 terrainCells[64][64],fogCells[64][64],movementCells[64][64];
static u8 *gBmMapTerrain[64],*gBmMapFog[64],*gBmMapMovement[64];
static u8 *gTilesetTerrainLookup=(u8 *)(sTilesetConfig+0x1000);
static u8 bgVram[65536],objVram[4096],pixels[240*160*4];
struct MapChange {s8 id;u8 xOrigin,yOrigin,xSize,ySize;const void *data;};
struct MapAsset {const u16 *map,*config;const u8 *gfx1;int gfx1Size;const u8 *gfx2;int gfx2Size;const u16 *palette;int initialX,initialY;};
#include "generated/map_assets.h"
extern struct KeyStatusBuffer *fe8_input_status(void);
extern void fe8_units_render(u8 *,int,int,int);
extern void fe8_units_apply_palette(void);
extern int fe8_event_busy(void);
extern int fe8_event_fade_level(void),fe8_event_fade_mode(void);
static struct KeyStatusBuffer *gKeyStatusPtr;
static int active=-1,frame,sound=-1,errors;
static u8 obstacleHp[64][64];
#define DPAD_ANY 240
#define MAP_MOVEMENT_MAX 120
#define SONG_65 0x65
#define CAMERA_MARGIN_LEFT 48
#define CAMERA_MARGIN_RIGHT 176
#define CAMERA_MARGIN_TOP 32
#define CAMERA_MARGIN_BOTTOM 112
static int GetGameClock(void){return frame;}
static void PlaySoundEffect(int id){sound=id;}
static void RefreshAllLightRunes(void){} // No rune/event state until the event engine is ported.
static int sLastTimeMapCursorDrawn;
static struct Vec2 sLastCoordMapCursorDrawn;
static const u16 *cursorSprite;
static int cursorX,cursorY,cursorOam;
static void PutSprite(int layer,int x,int y,const u16 *sprite,int oam){(void)layer;cursorSprite=sprite;cursorX=x;cursorY=y;cursorOam=oam;}
static struct {int chapterIndex;} gPlaySt;
static u16 gBmMapBuffer[1+64*64];
static const void *GetChapterMapChangesPointer(int chapter){return chapter>=0&&chapter<79?mapChanges[chapter]:NULL;}
static const void *GetChapterMapPointer(int chapter){return chapters[chapter].map;}
static void Decompress(const void *src,void *dst){memcpy(dst,src,2+gBmMapSize.x*gBmMapSize.y*2);}
#include "generated/map_routines.h"
int fe8_map_start(int chapter){
 active=-1;if(chapter<0||chapter>=79)return -1;
 const struct MapAsset *a=&chapters[chapter];
 memset(&gBmSt,0,sizeof(gBmSt));gBmMapSize.x=a->map[0]&255;gBmMapSize.y=a->map[0]>>8;
 memcpy(sTilesetConfig,a->config,sizeof(sTilesetConfig));memset(bgVram,0,sizeof(bgVram));
 memcpy(bgVram+0x8000,a->gfx1,a->gfx1Size);if(a->gfx2Size)memcpy(bgVram+0xc000,a->gfx2,a->gfx2Size);
 for(int y=0;y<64;y++){gBmMapBaseTiles[y]=baseCells[y];gBmMapTerrain[y]=terrainCells[y];gBmMapFog[y]=fogCells[y];gBmMapMovement[y]=movementCells[y];}
 for(int y=0;y<gBmMapSize.y;y++)for(int x=0;x<gBmMapSize.x;x++){baseCells[y][x]=a->map[1+y*gBmMapSize.x+x];fogCells[y][x]=1;movementCells[y][x]=MAP_MOVEMENT_MAX;}
 RefreshTerrainBmMap();memset(obstacleHp,0,sizeof(obstacleHp));for(int y=0;y<gBmMapSize.y;y++)for(int x=0;x<gBmMapSize.x;x++){if(terrainCells[y][x]==27&&(y==0||terrainCells[y-1][x]!=27))obstacleHp[y][x]=chapterWallHp[chapter];else if(terrainCells[y][x]==51)obstacleHp[y][x]=20;}
 fe8_palette_reset();memcpy(gPaletteBuffer+96,a->palette,320);memcpy(gPaletteBuffer+256,mapUiPalette,64);fe8_units_apply_palette();
 memset(objVram,0,sizeof(objVram));for(int row=0;row<4;row++)memcpy(objVram+row*1024,mapUi+row*18*32,18*32);
 gBmSt.cameraMax.x=gBmMapSize.x*16-240;gBmSt.cameraMax.y=gBmMapSize.y*16-160;
 if(gBmSt.cameraMax.x<0)gBmSt.cameraMax.x=0;if(gBmSt.cameraMax.y<0)gBmSt.cameraMax.y=0;
 SetCursorMapPosition(a->initialX<gBmMapSize.x?a->initialX:0,a->initialY<gBmMapSize.y?a->initialY:0);
 gKeyStatusPtr=fe8_input_status();frame=0;sound=-1;cursorSprite=NULL;gPlaySt.chapterIndex=chapter;active=chapter;SetDispEnable(1,0,0,0,1);return 0;
}
void fe8_map_stop(void){active=-1;cursorSprite=NULL;}
int fe8_map_active(void){return active>=0;}
void fe8_map_tick(int keys){
 if(active<0)return;frame++;
 if(fe8_event_busy()||fe8_tactical_locks_cursor()){PutMapCursor(gBmSt.playerCursorDisplay.x,gBmSt.playerCursorDisplay.y,0);return;}
 // Accept the original repeat bits only when interpolation has reached its target.
 if(gBmSt.playerCursorDisplay.x==gBmSt.cursorTarget.x&&gBmSt.playerCursorDisplay.y==gBmSt.cursorTarget.y)HandleMapCursorInput(keys&240);
 HandleMoveMapCursor(4);HandleMoveCameraWithMapCursor(4);
 // Native renderer redraws the viewport; original camera's sub-tile state is retained.
 if(gBmSt.camera.x<0)gBmSt.camera.x=0;if(gBmSt.camera.x>gBmSt.cameraMax.x)gBmSt.camera.x=gBmSt.cameraMax.x;
 if(gBmSt.camera.y<0)gBmSt.camera.y=0;if(gBmSt.camera.y>gBmSt.cameraMax.y)gBmSt.camera.y=gBmSt.cameraMax.y;
 PutMapCursor(gBmSt.playerCursorDisplay.x,gBmSt.playerCursorDisplay.y,0);
}
static void Pixel(int x,int y,int color){if(x<0||y<0||x>=240||y>=160)return;u8 *p=pixels+(y*240+x)*4;for(int c=0;c<3;c++){int v=(color>>(c*5))&31;p[c]=(v<<3)|(v>>2);}p[3]=255;}
int fe8_map_tile(int x,int y,int part){
 if(active<0||x<0||y<0||x>=gBmMapSize.x||y>=gBmMapSize.y||part<0||part>3)return -1;
 fogCells[y][x]=fe8_fog_visible(x,y);u16 out[64]={0};DisplayBmTile(out,0,0,x,y);return out[(part>>1)*32+(part&1)];
}
u8 *fe8_map_pixels(void){
 if(active<0)return pixels;fe8_fog_refresh();
 for(int y=0;y<160;y++)for(int x=0;x<240;x++){
  int wx=x+gBmSt.camera.x,wy=y+gBmSt.camera.y;
  if(wx>=gBmMapSize.x*16||wy>=gBmMapSize.y*16){Pixel(x,y,0);continue;}
  int bx=wx>>4,by=wy>>4,id=baseCells[by][bx];int part=((wy&8)>>2)+((wx&8)>>3);
  int tile=((fe8_fog_visible(bx,by)?6:11)<<12)+sTilesetConfig[id+part],px=wx&7,py=wy&7;
  if(tile&0x400)px=7-px;if(tile&0x800)py=7-py;
  // SetupBackgrounds default BG3 character base is 0x8000 (hardware.c).
  int offset=0x8000+(tile&1023)*32+py*4+px/2;int index=(bgVram[offset]>>((px&1)*4))&15;
  Pixel(x,y,gPaletteBuffer[((tile>>12)&15)*16+index]);
 }
 fe8_tactical_render_range(pixels,gBmSt.camera.x,gBmSt.camera.y,frame);
 fe8_units_render(pixels,gBmSt.camera.x,gBmSt.camera.y,frame);
 if(cursorSprite)for(int s=0;s<cursorSprite[0];s++){
  int a=cursorSprite[1+s*3],b=cursorSprite[2+s*3],c=cursorSprite[3+s*3]+cursorOam;
  int sx=(b&511);if(sx>=256)sx-=512;int sy=a&255;if(sy>=128)sy-=256;
  for(int y=0;y<8;y++)for(int x=0;x<8;x++){
   int px=b&0x1000?7-x:x,py=b&0x2000?7-y:y;int pos=(c&1023)*32+py*4+px/2;
   if(pos>=sizeof(objVram)){errors++;continue;}int index=(objVram[pos]>>((px&1)*4))&15;
   if(index)Pixel(cursorX+sx+x,cursorY+sy+y,gPaletteBuffer[256+((c>>12)&15)*16+index]);
  }
 }
 fe8_tactical_render_menu(pixels);
 fe8_battle_render(pixels,gBmSt.camera.x,gBmSt.camera.y);
 fe8_background_render(pixels);
 fe8_dialogue_render(pixels);
 fe8_tactical_render_item_menu(pixels);
 fe8_tactical_render_trade(pixels);
 int level=fe8_event_fade_level(),mode=fe8_event_fade_mode();
 if(level>0)for(int i=0;i<240*160;i++)for(int c=0;c<3;c++){int value=pixels[i*4+c]>>3;if(mode==2)value+=((31-value)*level)>>4;else value=(value*(16-level))>>4;pixels[i*4+c]=(value<<3)|(value>>2);}
 if(!fe8_display_enabled())for(int i=0;i<240*160;i++)pixels[i*4]=pixels[i*4+1]=pixels[i*4+2]=0;
 return pixels;
}
int fe8_map_terrain(int x,int y){if(active<0||x<0||y<0||x>=gBmMapSize.x||y>=gBmMapSize.y)return -1;return GetTrueTerrainAt(x,y);}
int fe8_map_info(int field){switch(field){case 0:return active;case 1:return gBmMapSize.x;case 2:return gBmMapSize.y;case 3:return gBmSt.playerCursor.x;case 4:return gBmSt.playerCursor.y;case 5:return gBmSt.camera.x;case 6:return gBmSt.camera.y;case 7:return gBmSt.playerCursorDisplay.x;case 8:return gBmSt.playerCursorDisplay.y;case 9:return sound;case 10:return errors;default:return -1;}}

int fe8_map_change(int id,int revert){
 if(active<0||id<0||id>127||!GetMapChange(id))return -1;
 if(revert)RevertMapChange(id);else ApplyMapChangesById(id);
 RefreshTerrainBmMap();return 0;
}
int fe8_map_change_at(int x,int y){if(active<0||x<0||y<0||x>=gBmMapSize.x||y>=gBmMapSize.y)return -1;return GetMapChangeIdAt(x,y);}

void fe8_map_focus(int x,int y){if(active<0)return;if(x<0)x=0;if(y<0)y=0;if(x>=gBmMapSize.x)x=gBmMapSize.x-1;if(y>=gBmMapSize.y)y=gBmMapSize.y-1;SetCursorMapPosition(x,y);gBmSt.camera.x=x*16-120;gBmSt.camera.y=y*16-80;if(gBmSt.camera.x<0)gBmSt.camera.x=0;if(gBmSt.camera.x>gBmSt.cameraMax.x)gBmSt.camera.x=gBmSt.cameraMax.x;if(gBmSt.camera.y<0)gBmSt.camera.y=0;if(gBmSt.camera.y>gBmSt.cameraMax.y)gBmSt.camera.y=gBmSt.cameraMax.y;gBmSt.camera.x&=~15;gBmSt.camera.y&=~15;}

static int ObstacleRoot(int x,int y){if(active<0||x<0||y<0||x>=gBmMapSize.x||y>=gBmMapSize.y)return -1;if(terrainCells[y][x]==27){while(y>0&&terrainCells[y-1][x]==27)y--;}else if(terrainCells[y][x]!=51)return -1;return y;}
int fe8_map_obstacle_hp(int x,int y){int root=ObstacleRoot(x,y);return root<0?-1:obstacleHp[root][x];}
int fe8_map_obstacle_max(int x,int y){return ObstacleRoot(x,y)<0?-1:terrainCells[y][x]==51?20:chapterWallHp[active];}
int fe8_map_obstacle_update(int x,int y,int hp){int root=ObstacleRoot(x,y);if(root<0||hp<0||hp>fe8_map_obstacle_max(x,y)||!obstacleHp[root][x])return -1;if(!hp){int change=fe8_map_change_at(x,y);if(change<0)return -2;if(fe8_map_change(change,0)<0)return -3;}obstacleHp[root][x]=hp;return 0;} // Native trap HP/terrain update; original map changes retained, crossfade adapted.
