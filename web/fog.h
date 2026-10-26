#ifndef FE8_FOG_H
#define FE8_FOG_H
void fe8_fog_refresh(void);
int fe8_fog_visible(int x,int y);
int fe8_fog_enabled(void);
void fe8_fog_phase(int faction);
int fe8_fog_can_torch(int index,int slot);
int fe8_fog_use_torch(int index,int slot);
#endif
