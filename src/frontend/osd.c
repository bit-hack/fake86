#include "frontend.h"


static uint32_t _last_disk_tick;


void osd_disk_fdd_used(void) {
  _last_disk_tick = SDL_GetTicks();
}

void osd_disk_hdd_used(void) {
  _last_disk_tick = SDL_GetTicks();
}

bool osd_should_draw_disk(void) {
  return (SDL_GetTicks() - _last_disk_tick) < 250;
}
