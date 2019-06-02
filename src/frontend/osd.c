#include "frontend.h"


static uint32_t _last_disk_tick;
static bool _is_active;


void osd_disk_fdd_used(void) {
  _last_disk_tick = SDL_GetTicks();
}

void osd_disk_hdd_used(void) {
  _last_disk_tick = SDL_GetTicks();
}

bool osd_should_draw_disk(void) {
  return (SDL_GetTicks() - _last_disk_tick) < 250;
}

bool osd_is_active(void) {
  return _is_active;
}

void osd_open(void) {
}

void osd_close(void) {
}

void osd_on_event(const SDL_Event *t) {
}
