
#ifndef __GRAPHICS_H
#define __GRAPHICS_H

#include <SDL.h>

#include "controller.h"
#include "fontface.h"
		/* ^ defines FONTWIDTH/FONTHEIGHT */

int Graphics_DrawHLine(SDL_Surface *s, Uint16 x, Uint16 y, Uint16 l, Uint32 color);
int Graphics_DrawVLine(SDL_Surface *s, Uint16 x, Uint16 y, Uint16 l, Uint32 color);
int Graphics_DrawRect(SDL_Surface *s, SDL_Rect *rect, Uint32 color);
int Graphics_GradFillRect(SDL_Surface *s, SDL_Rect *rect, Uint16 max,
			  SDL_Color *top, SDL_Color *bottom, SDL_Color *last);
int Graphics_WriteText(SDL_Surface *s, Uint16 x, Uint16 y, const char *text,
		       Uint32 color);
int Graphics_printf(SDL_Surface *s, Uint16 x, Uint16 y, Uint32 color,
		    const char *format, ...);


static inline int Graphics_BlankRect(SDL_Surface *s, SDL_Rect *rect);
static inline int Graphics_FillRect(SDL_Surface *s, SDL_Rect *rect, Uint32 color);

static inline int
Graphics_BlankRect(SDL_Surface *s, SDL_Rect *rect)
{
	return SDL_FillRect(s, rect, display.background);
}

static inline int
Graphics_FillRect(SDL_Surface *s, SDL_Rect *rect, Uint32 color)
{
	return SDL_FillRect(s, rect, color);
}

#endif
