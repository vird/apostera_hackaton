#pragma once
#include "SDL/SDL.h"
#include "SDL/SDL_image.h"

const int _display_size_x = 1920;
const int _display_size_y = 1080;
const int _display_bpp    = 32;

SDL_Surface *_display_screen = NULL;
SDL_Surface *display_buffer = NULL;
bool display_init();
bool display_redraw();
