
#ifndef __CONTROLS_H
#define __CONTROLS_H

#include <SDL.h>

#include "osc.h"

struct Paint {
	enum Paint_Type {
		PAINT_PLAIN = 0,
		PAINT_GRAD
	} type;

	union {
		struct Paint_Plain {
			Uint32 color;
		} plain;

		struct Paint_Grad {
			SDL_Color top;
			SDL_Color bottom;
		} grad;
	} u;
};

struct Control {
	enum Control_Type {
		SLIDER = 0,
		FIELD
	} type;

	SDL_Rect geo;

	struct Control_OSC {
		char			*address;
		enum Osc_DataType	datatype;
	} OSC;

	union {
		struct Slider {
			enum Slider_Type {
				SLIDER_BUTTON = 0,
				SLIDER_SET,
				SLIDER_RELATIVE
			} type;
			#define SLIDER_TYPE	\
				"button\0"	\
				"set\0"		\
				"relative\0"

			struct Paint	paint;

			double		min;
			double		max;
			double		step;

			double		value;

			char		*label;
			Uint8		show_value;

			union {
				struct Slider_Button {
					Uint16 button_y;
				} button;
			} u;
		} slider;

		struct Field {
			enum Field_Type {
				FIELD_BUTTON = 0,
				FIELD_SWITCH
			} type;

			Uint32	color;
			Uint8	value;
			char	*label;
		} field;
	} u;
};

				/* slider inner padding is 1/2000 of its area */
#define SLIDER_PADDING(C) ((C)->geo.w*(C)->geo.h/2000)

static inline void Controls_SetSliderValue(struct Slider *slider, double value);
static inline void Controls_InitSliderButton(struct Control *c);

int Controls_Slider(SDL_Surface *s, struct Control *c);
int Controls_Field(SDL_Surface *s, struct Control *c);
int Controls_Draw(SDL_Surface *s, struct Control *c);

static inline void
Controls_SetSliderValue(struct Slider *slider, double value)
{
	slider->value = slider->step ?
			(Uint32)((value - slider->min)/slider->step+.5)*
			slider->step + slider->min : value;
}

static inline void
Controls_InitSliderButton(struct Control *c)
{
	struct Slider	*slider = &c->u.slider;
	Uint16		padding = SLIDER_PADDING(c);

	slider->u.button.button_y = c->geo.y + (c->geo.h - (padding << 1))*
					(1. - (slider->value - slider->min)/
					 (slider->max - slider->min)) - padding;
}

#endif

