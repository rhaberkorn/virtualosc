
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <SDL.h>

#include "graphics.h"
#include "controls.h"
#include "controller.h"

int
Controls_Slider(SDL_Surface *s, struct Control *c)
{
	struct Slider	*slider = &c->u.slider;
	struct Paint	*paint = &slider->paint;

	Uint32		border_color;

	Uint16		padding = SLIDER_PADDING(c);
	SDL_Rect	bar;

	if (Graphics_BlankRect(s, &c->geo))
		return 1;

	bar.x = c->geo.x + padding;
	bar.w = c->geo.w - (padding << 1);

	if (slider->step) { /* stepwise */
		float	cr, cg, cb;
		float	diff_r, diff_g, diff_b;

		Uint32	color;

		Uint16	steps = (slider->max - slider->min)/slider->step;
		float	box_s = (float)(c->geo.h - padding)/steps;
		float	fy = c->geo.y + c->geo.h - box_s;

		bar.h = box_s - padding;

		switch (paint->type) {
		case PAINT_PLAIN:
			color = border_color = paint->u.plain.color;
			break;

		case PAINT_GRAD: {
			struct Paint_Grad *grad = &paint->u.grad;

			cr = grad->bottom.r;
			cg = grad->bottom.g;
			cb = grad->bottom.b;

			diff_r = (float)(grad->top.r - cr)/steps;
			diff_g = (float)(grad->top.g - cg)/steps;
			diff_b = (float)(grad->top.b - cb)/steps;

			break;
		}
		}

		for (double i = slider->min; i < slider->value;
						i += slider->step, fy -= box_s) {
			if (paint->type == PAINT_GRAD) {
				color = SDL_MapRGB(s->format, cr, cg, cb);

				cr += diff_r;
				cg += diff_g;
				cb += diff_b;
			}

			bar.y = fy;

			if (Graphics_FillRect(s, &bar, color))
				return 1;
		}

		if (paint->type == PAINT_GRAD)
			border_color = SDL_MapRGB(s->format, cr, cg, cb);
	} else { /* continious */
		Uint16 max_h = c->geo.h - (padding << 1);

		bar.h = (slider->value - slider->min)*max_h/
						(slider->max - slider->min);
		bar.y = c->geo.y + c->geo.h - padding - bar.h;

		switch (paint->type) {
		case PAINT_PLAIN:
			border_color = paint->u.plain.color;
			if (Graphics_FillRect(s, &bar, border_color))
				return 1;

			break;

		case PAINT_GRAD: {
			SDL_Color border;

			if (Graphics_GradFillRect(s, &bar, max_h,
						  &paint->u.grad.top,
						  &paint->u.grad.bottom, &border))
				return 1;

			border_color = SDL_MapRGB(s->format, border.r, border.g,
						  border.b);
			break;
		}
		}
	}

	if (Graphics_DrawRect(s, &c->geo, border_color))
		return 1;

	if (slider->type == SLIDER_BUTTON) {
		bar.w = c->geo.w - 2;	/* reuse 'bar' */
		bar.h = padding << 1;
		bar.x = c->geo.x + 1;
		bar.y = slider->u.button.button_y;

		if (Graphics_FillRect(s, &bar, border_color))
			return 1;
	}

	SDL_UpdateRects(s, 1, &c->geo);

			/* slider label */

	if (slider->label) {
		if (Graphics_WriteText(s, c->geo.x, c->geo.y - FONTHEIGHT,
				       slider->label, border_color))
			return 1;
		SDL_UpdateRect(s, c->geo.x, c->geo.y - FONTHEIGHT,
			       strlen(slider->label)*FONTWIDTH, FONTHEIGHT);
	}

			/* slider value */

	if (slider->show_value) {
		Uint16 y = c->geo.y + c->geo.h + 1;
		int len = Graphics_printf(s, c->geo.x, y, border_color,
					  "%g/%g", slider->value, slider->max);

		if (len == -1)
			return 1;

		SDL_UpdateRect(s, c->geo.x, y, len*FONTWIDTH, FONTHEIGHT);
	}

	return 0;
}

int
Controls_Field(SDL_Surface *s, struct Control *c)
{
	struct Field *field = &c->u.field;

	if (Graphics_BlankRect(s, &c->geo))
		return 1;

	if (field->value) {
		if (Graphics_FillRect(s, &c->geo, field->color))
			return 1;
	} else if (Graphics_DrawRect(s, &c->geo, field->color))
		return 1; 

	if (field->label &&
	    Graphics_WriteText(s, c->geo.x + c->geo.w/2 -
			       strlen(field->label)*(FONTWIDTH/2),
			       c->geo.y + c->geo.h/2 - FONTHEIGHT/2,
			       field->label, field->value ?
			       display.background : field->color))
		return 1;

	SDL_UpdateRects(s, 1, &c->geo);
	return 0;
}

int
Controls_Draw(SDL_Surface *s, struct Control *c)
{
	switch (c->type) {
	case SLIDER:
		return Controls_Slider(s, c);
	case FIELD:
		return Controls_Field(s, c);
	}

	return 1;
}

