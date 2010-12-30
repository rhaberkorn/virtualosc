
#ifndef __CONTROLLER_H
#define __CONTROLLER_H

#include <SDL.h>

#include "controls.h"

extern struct Display {
	int	width;
	int	height;

	int	bpp;

	Uint32	flags;
	int	cursor;

	Uint32	foreground;
	Uint32	background;
} display;

extern struct Registry {
	struct Tab {
		struct Control	*controls;
		Uint32		cControls;

		char		*label;
	} *tabs;
	Uint32 cTabs;
} registry;

#ifndef PACKAGE_NAME
#define PACKAGE_NAME "Virtual OSC Controller"
#endif
#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION "dev"
#endif

#define WINDOW_TITLE PACKAGE_NAME " (" PACKAGE_VERSION ")"

#ifndef PATH_SEPARATOR
# if defined(__OS2__) || defined(__NT__)
#  define PATH_SEPARATOR '\\'
# else
#  define PATH_SEPARATOR '/'
# endif
#endif

enum Controller_Event {
	CONTROLLER_OK = 0,
	CONTROLLER_ERR_THREAD
};

#endif

