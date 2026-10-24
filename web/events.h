#ifndef FE8_EVENTS_H
#define FE8_EVENTS_H
#include "compat.h"
int fe8_event_call(u32 address,int trigger);
int fe8_event_flag(int id);
void fe8_event_phase(void);
void fe8_event_post_action(void);
void fe8_event_reset(void);
void fe8_event_actor(int index);
int fe8_event_tile_command(int x,int y);
int fe8_event_seize(int index);
int fe8_event_unlock(int index,int x,int y);
int fe8_event_pillage(int index);
void fe8_event_show_icons(void);
void fe8_event_pack_permanent(u8 *out);
void fe8_event_load_permanent(const u8 *data);
void fe8_event_reward(int unit,int item,int gold);
int fe8_event_item_info(int field);
int fe8_event_item_choose(int slot);
int fe8_event_visit(int index);
int fe8_event_talk_available(int actor,int target);
int fe8_event_talk(int actor,int target);
int fe8_progress_info(int field);
void fe8_progress_reset(void);
int fe8_event_start(u32 address);
void fe8_event_tick(int keys);
int fe8_event_busy(void);
int fe8_event_info(int field);
void fe8_event_advance(void);
void fe8_event_confirm(void);
int fe8_event_preview(void);
int fe8_event_story_chapter(void);
void fe8_chapter_start(void);
void fe8_chapter_tick(int keys);
void fe8_event_set_flag(int flag);
int fe8_event_quote(int message);
int fe8_event_shop_item(int x,int y,int row);
#endif
