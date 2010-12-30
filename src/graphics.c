
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include <SDL.h>

#include "controller.h"
#include "graphics.h"

#define GENERIC_LOCK(S) {				\
	if (SDL_MUSTLOCK(S) && SDL_LockSurface(S))	\
		return 1;				\
}

#define GENERIC_UNLOCK(S) {				\
	if (SDL_MUSTLOCK(S))				\
		SDL_UnlockSurface(S);			\
	return 0;					\
}

#define XYTOPTR(S, X, Y, BPP)	((Uint8*)(S)->pixels + (Y)*(S)->pitch + (X)*(BPP))
#define SETPIXEL(P, BPP, C)	memcpy(P, &(C), BPP)
#define SAFE_SETPIXEL(P, MAX, BPP, C) {	\
	if (P < MAX)			\
		SETPIXEL(P, BPP, C);	\
}

int
Graphics_DrawHLine(SDL_Surface *s, Uint16 x, Uint16 y, Uint16 l, Uint32 color)
{
	Uint8	bpp = s->format->BytesPerPixel;
	Uint8	*p = XYTOPTR(s, x, y, bpp);

	GENERIC_LOCK(s);

	while (l--) {
		SETPIXEL(p, bpp, color);
		p += bpp;
	}

	GENERIC_UNLOCK(s);
}

int
Graphics_DrawVLine(SDL_Surface *s, Uint16 x, Uint16 y, Uint16 l, Uint32 color)
{
	Uint8	bpp = s->format->BytesPerPixel;
	Uint8	*p = XYTOPTR(s, x, y, bpp);

	GENERIC_LOCK(s);

	while (l--) {
		SETPIXEL(p, bpp, color);
		p += s->pitch;
	}

	GENERIC_UNLOCK(s);
}

int
Graphics_DrawRect(SDL_Surface *s, SDL_Rect *rect, Uint32 color)
{
	Uint8	bpp = s->format->BytesPerPixel;
	Uint8	*p = XYTOPTR(s, rect->x + 1, rect->y, bpp);

	Uint32	d;

	GENERIC_LOCK(s);

	if (rect->w > 2) {
		d = (rect->h - 1)*s->pitch;

		for (Uint16 w = rect->w - 2; w; w--, p += bpp) {
			SETPIXEL(p, bpp, color);
			SETPIXEL(p + d, bpp, color);
		}
	}

	d = (rect->w - 1)*bpp;
	for (Uint16 h = rect->h; h; h--, p += s->pitch) {
		SETPIXEL(p, bpp, color);
		SETPIXEL(p - d, bpp, color);
	}

	GENERIC_UNLOCK(s);
}

int
Graphics_GradFillRect(SDL_Surface *s, SDL_Rect *rect, Uint16 max,
		      SDL_Color *top, SDL_Color *bottom, SDL_Color *last)
{
	Uint8		bpp = s->format->BytesPerPixel;
	Uint8		*p = XYTOPTR(s, rect->x, rect->y + rect->h, bpp);

	float		cr = bottom->r, cg = bottom->g, cb = bottom->b;
	float		diff_r = (float)(top->r - cr)/max;
	float		diff_g = (float)(top->g - cg)/max;
	float		diff_b = (float)(top->b - cb)/max;

	Sint8		p_diff = bpp;

	GENERIC_LOCK(s);

	for (Uint16 h = rect->h; h; h--) {
		Uint32 color = SDL_MapRGB(s->format, cr, cg, cb);

		for (Uint16 w = rect->w; w; w--, p += p_diff)
			SETPIXEL(p, bpp, color);

		cr += diff_r;
		cg += diff_g;
		cb += diff_b;

		p -= s->pitch + p_diff;
		p_diff = -p_diff;
	}

	if (last) {
		last->r = cr;
		last->g = cg;
		last->b = cb;
	}

	GENERIC_UNLOCK(s);
}

int
Graphics_WriteText(SDL_Surface *s, Uint16 x, Uint16 y, const char *text,
		   Uint32 color)
{
	Uint8	bpp = s->format->BytesPerPixel;
	Uint8	*p = XYTOPTR(s, x + FONTWIDTH, y, bpp);
	Uint8	*max = XYTOPTR(s, 0, s->h, 0);

	GENERIC_LOCK(s);

	for (const char *c = text; *c; c++, p += bpp*FONTWIDTH) {
		const Uint8	*line = Font_Face[*c];
		Uint8		*py = p;

		for (Uint8 yc = FONTHEIGHT; yc; yc--, line++, py += s->pitch) {
			Uint8	col = *line;
			Uint8	*px = py;

			for (Uint8 xc = FONTWIDTH; xc; xc--, col >>= 1, px -= bpp)
				if (col & 1)
					SAFE_SETPIXEL(px, max, bpp, color);
		}
	}

	GENERIC_UNLOCK(s);
}

int
Graphics_printf(SDL_Surface *s, Uint16 x, Uint16 y, Uint32 color,
		const char *format, ...)
{
	char	buffer[1024];
	va_list	args;

	int	ret;

	va_start(args, format);
	ret = vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	return ret != -1 && ret < sizeof(buffer) &&
	       !Graphics_WriteText(s, x, y, buffer, color) ? ret : -1;
}

