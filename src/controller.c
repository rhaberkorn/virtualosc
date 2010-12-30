
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include <SDL.h>
#include <SDL_thread.h>

#include "graphics.h"
#include "controls.h"
#include "xml.h"
#include "osc.h"
#include "controller.h"

#define DIE(MSG, ...) {					\
	fprintf(stderr, MSG "\n", ##__VA_ARGS__);	\
	goto err;					\
}

static int Slider_EnqueueMessage(struct Control *c);
static int Field_EnqueueMessage(struct Control *c);
static inline int EnqueueAllControls(struct Tab *tab);

static inline struct Control *GetControl(struct Tab *tab, Uint16 x, Uint16 y);

static inline int UpdateSliderValue(SDL_Surface *s, struct Control *c,
				    SDL_MouseMotionEvent *motion);

static inline int DrawAllControls(SDL_Surface *s, struct Tab *tab);
static void FreeRegistry(void);
static inline int ToggleCursor(void);

static inline int EvalOptions(int argc, char **argv, char **interface,
			      char **host, int *port);
static void quit_wrapper(void);
int main(int argc, char **argv);

struct Display display = {		/* initialize with default config */
	.width = 800,
	.height = 600,

	.bpp = 16,

	.flags = SDL_FULLSCREEN,
	.cursor = SDL_DISABLE
};

#define DEFAULT_FOREGROUND 0,	255,	0
#define DEFAULT_BACKGROUND 0,	0,	0

#define DEFAULT_HOST	"localhost"	/* default OSC server config */
#define DEFAULT_PORT	77777

struct Registry registry = {
	.tabs = NULL,
	.cTabs = 0
};

static int
Slider_EnqueueMessage(struct Control *c)
{
	struct Control_OSC	*osc = &c->OSC;
	struct Slider		*slider = &c->u.slider;

	if (osc->address)
		switch (osc->datatype) {
		case OSC_INT:
		case OSC_FLOAT:
		case OSC_DOUBLE:
		case OSC_BOOL:
			return Osc_EnqueueFloatMessage(osc->address, slider->value);
		}

	return 0;
}

static int
Field_EnqueueMessage(struct Control *c)
{
	struct Control_OSC	*osc = &c->OSC;
	struct Field		*field = &c->u.field;

	if (osc->address)
		switch (osc->datatype) {
		case OSC_INT:
		case OSC_FLOAT:
		case OSC_DOUBLE:
		case OSC_BOOL:
			return Osc_EnqueueFloatMessage(osc->address, field->value);
		}

	return 0;
}

static inline int
EnqueueAllControls(struct Tab *tab)
{
	struct Control *cur = tab->controls;

	for (Uint32 c = tab->cControls; c; c--, cur++)
		switch (cur->type) {
		case SLIDER:
			if (Slider_EnqueueMessage(cur))
				return 1;
			break;

		case FIELD:
			if (cur->u.field.type == FIELD_SWITCH &&
			    Field_EnqueueMessage(cur))
				return 1;
			break;
		}

	return 0;
}

static inline struct Control *
GetControl(struct Tab *tab, Uint16 x, Uint16 y)
{
	struct Control *cur = tab->controls;

	for (Uint32 c = tab->cControls; c; c--, cur++)
		if (x >= cur->geo.x && x <= cur->geo.x + cur->geo.w)
			switch (cur->type) {
			case SLIDER:
				switch (cur->u.slider.type) {
				case SLIDER_SET:
				case SLIDER_RELATIVE:
					if (y >= cur->geo.y && y <= cur->geo.y + cur->geo.h)
						return cur;
					break;

				case SLIDER_BUTTON: {
					struct Slider_Button *button = &cur->u.slider.u.button;

					if (y >= button->button_y && y <= button->button_y +
					    (SLIDER_PADDING(cur) << 1))
						return cur;
					break;
				}
				}
				break;

			case FIELD:
				if (y >= cur->geo.y && y <= cur->geo.y + cur->geo.h)
					return cur;
				break;
			}

	return NULL;
}

static inline int
UpdateSliderValue(SDL_Surface *s, struct Control *c,
		  SDL_MouseMotionEvent *motion)
{
	struct Slider	*slider = &c->u.slider;
	Uint16		padding = SLIDER_PADDING(c);

	if (slider->show_value) {
		SDL_Rect text;
		int len = snprintf(NULL, 0, "%g/%g", slider->value, slider->max);

		if (len == -1)
			return 1;

		text.x = c->geo.x;
		text.y = c->geo.y + c->geo.h + 1;
		text.w = len*FONTWIDTH;
		text.h = FONTHEIGHT;

		if (Graphics_BlankRect(s, &text))
			return 1;
		SDL_UpdateRects(s, 1, &text);
	}

	switch (slider->type) {
	case SLIDER_SET:
	case SLIDER_BUTTON:
		if (motion->y <= c->geo.y + padding)
			slider->value = slider->max;
		else if (motion->y >= c->geo.y + c->geo.h - padding)
			slider->value = slider->min;
		else
			Controls_SetSliderValue(
				slider,
				(1. - (double)(motion->y - c->geo.y)/c->geo.h)*
				(slider->max - slider->min) + slider->min);

		if (slider->type == SLIDER_BUTTON) {
			struct Slider_Button	*button = &slider->u.button;
			Uint16			x;

			button->button_y += motion->yrel;

			if (button->button_y < (x = c->geo.y + 1) ||
			    button->button_y > (x = c->geo.y + c->geo.h -
								(padding << 1) - 1))
				button->button_y = x;
		}

		break;

	case SLIDER_RELATIVE: {
		double v = slider->value - (double)motion->yrel/c->geo.h*
						(slider->max - slider->min);

		if (v > slider->max)
			slider->value = slider->max;
		else if (v < slider->min)
			slider->value = slider->min;
		else
			Controls_SetSliderValue(slider, v);

		break;
	}
	}

	return 0;
}

static inline int
DrawAllControls(SDL_Surface *s, struct Tab *tab)
{
	struct Control *cur = tab->controls;

	for (Uint32 c = tab->cControls; c; c--, cur++)
		if (Controls_Draw(s, cur))
			return 1;

	return 0;
}

/*
 * not that useful right now but might become handy later when we need to load
 * interface dynamically
 */

static void
FreeRegistry(void)
{
	for (struct Tab *tab = registry.tabs; registry.cTabs;
						registry.cTabs--, tab++) {
		for (struct Control *control = tab->controls; tab->cControls;
							tab->cControls--, control++) {
			free(control->OSC.address);

			switch (control->type) {
			case SLIDER:
				free(control->u.slider.label);
				break;

			case FIELD:
				free(control->u.field.label);
				break;
			}
		}

		free(tab->controls);
		free(tab->label);
	}

	free(registry.tabs);
	memset(&registry, 0, sizeof(struct Registry));
}

static inline int
ToggleCursor(void)
{
	return (display.cursor = display.cursor == SDL_ENABLE ?
						SDL_DISABLE : SDL_ENABLE);
}

static inline int
EvalOptions(int argc, char **argv, char **interface, char **host, int *port)
{
	int		c;
	char		*p;

	while ((c = getopt(argc, argv, "hg:b:fci:r:p:d")) != -1)
		switch (c) {
		case '?':
		case 'h':
			if ((p = strrchr(argv[0], PATH_SEPARATOR)))
				p++;
			else
				p = argv[0];

			printf(WINDOW_TITLE "\n\n"
			       "%s\t-h\t\t"		"Show this help\n"
			       "\t\t-i INTERFACE\t"	"Interface XML file\n"
			       "\t\t-g WIDTHxHEIGHT\t"	"Screen resolution\n"
			       "\t\t-b BPP\t\t"		"Color depth\n"
			       "\t\t-f\t\t"		"Toggle fullscreen mode\n"
			       "\t\t-c\t\t"		"Toggle mouse cursor display\n"
			       "\t\t-r HOST\t\t"	"Remote host (OSC server)\n"
			       "\t\t-p PORT\t\t"	"Remote port\n"
			       "\t\t-d\t\t"		"Disable OSC message dispatching\n\n",
			       p);

			return 1;

		case 'g':
			if (sscanf(optarg, "%ux%u", &display.width,
				   &display.height) != 2)
				return 1;
			break;

		case 'b':
			display.bpp = strtoul(optarg, &p, 10);
			if (*p)
				return 1;
			break;

		case 'f':
			display.flags = (display.flags | SDL_FULLSCREEN) &
					~(display.flags & SDL_FULLSCREEN);
						/* ^ toggles fullscreen flag */
			break;

		case 'c':
			ToggleCursor();
			break;

		case 'i':
			*interface = optarg;
			break;

		case 'r':
			*host = optarg;
			break;

		case 'p':
			*port = strtoul(optarg, &p, 10);
			if (*p)
				return 1;
			break;

		case 'd':
			*host = NULL;
			break;
		}

	return 0;
}

static void
quit_wrapper(void)
{
	SDL_Quit();
}

int main(int argc, char **argv)
{
	SDL_Surface		*s = NULL;

	SDL_Event		event;
	SDL_MouseButtonEvent	*button = &event.button;
	SDL_MouseMotionEvent	*motion = &event.motion;

	struct Tab		*curTab;
	struct Control		*cur = NULL;

	char			*interface = NULL;
	char			*host = DEFAULT_HOST;	/* default config, s.a. */
	int			port = DEFAULT_PORT;

	int			socket_fd = -1;
	SDL_Thread		*oscThread = NULL;

	/* TODO: update global (display) default config by evaluating a
	   config XML file */

	if (EvalOptions(argc, argv, &interface, &host, &port))
		DIE("Error during command line option pasing.");

	if (!interface)
		DIE("You have to specify an interface definition (-i option).");

	if (host) {
		if ((socket_fd = Osc_Connect(host, port)) < 0)
			DIE("Couldn't create and connect socket.");

		if (!(oscThread = Osc_InitThread(&socket_fd)))
			DIE("Error initializing the OSC sending thread.");
	}

	if (SDL_Init(SDL_INIT_VIDEO))
		DIE("Couldn't initialize video subsystem.");

	atexit(quit_wrapper);

	SDL_WM_SetCaption(WINDOW_TITLE, NULL);
#ifdef __OS2__
	SDL_ShowCursor(SDL_DISABLE);
#endif
	SDL_ShowCursor(display.cursor);

	if (!(s = SDL_SetVideoMode(display.width, display.height,
				   display.bpp, display.flags)))
		DIE("Couldn't set video mode.");

				/* default config, s.a. */
	display.foreground = SDL_MapRGB(s->format, DEFAULT_FOREGROUND);	
	display.background = SDL_MapRGB(s->format, DEFAULT_BACKGROUND);

	if (Xml_ReadInterface(interface, s))
		DIE("Error parsing interface definition.");

	curTab = registry.tabs;	/* first tab */

	if (host && EnqueueAllControls(curTab))
		DIE("Couldn't enqueue OSC message.");

			/* draw control interface */

	if (Graphics_BlankRect(s, NULL))
		DIE("Couldn't set background color.");
	SDL_UpdateRect(s, 0, 0, 0, 0);

	if (DrawAllControls(s, curTab))
		DIE("Couldn't draw control.");

	while (SDL_WaitEvent(&event))
		switch (event.type) {
		case SDL_KEYUP:
			switch (event.key.keysym.sym) {
			case SDLK_f: /* toggle (f)ullscreen */
				if (!SDL_WM_ToggleFullScreen(s))
					DIE("Couldn't toggle fullscreen state.");
#ifndef __OS2__			/* at least in OS/2 we need to redraw the screen */
				break;
#endif

			case SDLK_r: /* refresh */
				SDL_UpdateRect(s, 0, 0, 0, 0);

				if (display.cursor == SDL_ENABLE) {
					SDL_ShowCursor(SDL_DISABLE);
					SDL_ShowCursor(SDL_ENABLE);
				}
				break;

			case SDLK_c: /* toggle cursor */
				SDL_ShowCursor(ToggleCursor());
				break;

			case SDLK_q: /* quit */
			case SDLK_ESCAPE:
				goto finish;
			}
			break;

		case SDL_MOUSEBUTTONUP:
			if (cur) {
				switch (cur->type) {
				case FIELD: {
					struct Field *field = &cur->u.field;

					field->value = field->type == FIELD_BUTTON ?
									0 : !field->value;

					if (host && Field_EnqueueMessage(cur))
						DIE("Couldn't enqueue OSC message.");

					if (Controls_Field(s, cur))
						DIE("Couldn't draw control.");

					break;
				}
				}

				cur = NULL;
			}
			break;

		case SDL_MOUSEBUTTONDOWN:
			if (button->button == SDL_BUTTON_LEFT &&
			    (cur = GetControl(curTab, motion->x, motion->y)))
				switch (cur->type) {
				case FIELD: {
					struct Field *field = &cur->u.field;

					if (field->type == FIELD_BUTTON) {
						field->value = 1;

						if (host && Field_EnqueueMessage(cur))
							DIE("Couldn't enqueue OSC message.");

						if (Controls_Field(s, cur))
							DIE("Couldn't draw control.");
					}
					break;
				}
				}

		case SDL_MOUSEMOTION:
			if (cur)
				switch (cur->type) {
				case SLIDER:
					if (UpdateSliderValue(s, cur, motion))
						DIE("Couldn't update control value.");

					if (host && Slider_EnqueueMessage(cur))
						DIE("Couldn't enqueue OSC message.");

					if (Controls_Slider(s, cur))
						DIE("Couldn't draw control.");
					break;
				}
			break;

		case SDL_USEREVENT:
			switch ((enum Controller_Event)event.user.code) {
			case CONTROLLER_ERR_THREAD:
				DIE("Error during OSC thread execution.");
			}
			break;

		case SDL_QUIT: /* quit */
			goto finish;
		}
	DIE("Error retrieving event.");

finish:

	FreeRegistry();
	SDL_FreeSurface(s);
	if (host) {
		Osc_Disconnect(socket_fd);
		if (Osc_TerminateThread())
			return 1;
	}

	return 0;

err:

	FreeRegistry();
	if (s)
		SDL_FreeSurface(s);
	if (oscThread)
		Osc_TerminateThread();
	if (socket_fd > 0)
		Osc_Disconnect(socket_fd);

	return 1;
}
