#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "display.h"

void _display_apply_surface(int x, int y, SDL_Surface* source, SDL_Surface* destination, SDL_Rect* clip) {
  //Holds offsets
  SDL_Rect offset;
  
  //Get offsets
  offset.x = x;
  offset.y = y;
  
  //Blit
  SDL_BlitSurface(source, clip, destination, &offset);
}

bool display_init() {
  //Initialize all SDL subsystems
  if (SDL_Init(SDL_INIT_EVERYTHING | SDL_INIT_NOPARACHUTE) == -1) {
    return false;
  }
  signal(SIGINT, SIG_DFL);
  signal(SIGTERM, SIG_DFL);
  
  //Set up the _display_screen
  _display_screen = SDL_SetVideoMode(_display_size_x, _display_size_y, _display_bpp, SDL_HWSURFACE | SDL_FULLSCREEN);
  
  //If there was an error in setting up the _display_screen
  if (_display_screen == NULL) {
    return false;
  }
  
  //Set the window caption
  SDL_WM_SetCaption("Flip Test", NULL);
  
  display_buffer = SDL_CreateRGBSurface(0, 1920, 1080, 32, 0, 0, 0, 0);
  
  //If everything initialized fine
  return true;
}

bool display_redraw() {
  _display_apply_surface(0, 0, display_buffer, _display_screen, NULL);
  if (SDL_Flip(_display_screen) == -1) return false;
  
  return true;
}
