
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <SDL.h>
#include <expat.h>

#include "controls.h"
#include "controller.h"
#include "xml.h"

#define FOREACH_ATTR(VAR, ATTS) \
	for (const XML_Char **VAR = ATTS; *VAR; VAR += 2)

static inline int CaseEnumMap(const char *p, const char *value, int s);
static int DecodeColorAttrib(const XML_Char *str, SDL_Color *color);

static void XMLCALL Interface_CollectCharacterData(void *ud, const XML_Char *s,
						   int len);
static inline void Interface_PrepareCDBuffer(XML_Parser parser,
					     XML_EndElementHandler handler);
static void XMLCALL Interface_StartElement(void *ud, const XML_Char *name,
					   const XML_Char **atts);
static void XMLCALL Interface_EndSlider(void *ud, const XML_Char *name);
static void XMLCALL Interface_EndSwitch(void *ud, const XML_Char *name);

static XML_Char Interface_CDBuffer[32]; /* character data buffer */

static inline int
CaseEnumMap(const char *p, const char *value, int s)
{
	while (*p) {
		if (!strcasecmp(p, value))
			return s;

		p += strlen(p) + 1;
		s++;
	}

	return -1;
}

static int
DecodeColorAttrib(const XML_Char *str, SDL_Color *color)
{
	static const char colors[] = /* red	green	blue	*/
		"black\0"	     /* 0	0	0	*/
		"blue\0"	     /* 0	0	255	*/
		"green\0"	     /* 0	255	0	*/
		"cyan\0"	     /* 0	255	255	*/
		"red\0"		     /* 255	0	0	*/
		"magenta\0"	     /* 255	0	255	*/
		"yellow\0"	     /* 255	255	0	*/
		"white\0";	     /* 255	255	255	*/
	Sint8 c;

	if (*str == '#') { /* hex-encoded RGB color */
		Uint32	v;
		char	*p;

		if (strlen(++str) != 6)
			return 1;

		v = strtoul(str, &p, 16);
		if (*p)
			return 1;

		color->b = v >> 0 & 0xFF;
		color->g = v >> 8 & 0xFF;
		color->r = v >> 16 & 0xFF;

		return 0;
	}

		/* else: assume color string */

	if ((c = CaseEnumMap(colors, str, 0)) == -1)
		return 1;

	color->b = ((c >>= 0) & 0x1)*255; /* we're working with single bytes, so it should be */
	color->g = ((c >>= 1) & 0x1)*255; /* endianess-independent */
	color->r = ((c >>  1) & 0x1)*255;

	return 0;
}

/*
 * TODO: rewrite this - use a pointer to the end of the buffer
 * maybe rewrite the parser user data stuff (use a structure)
 */

static void XMLCALL
Interface_CollectCharacterData(void *ud, const XML_Char *s, int len)
{
	if (strlen(Interface_CDBuffer) + len < sizeof(Interface_CDBuffer))
		strncat(Interface_CDBuffer, s, len);
	else
		XML_StopParser(ud, XML_FALSE);
}

static inline void
Interface_PrepareCDBuffer(XML_Parser parser, XML_EndElementHandler handler)
{
	*Interface_CDBuffer = '\0';
	XML_SetCharacterDataHandler(parser, Interface_CollectCharacterData);
	XML_SetElementHandler(parser, NULL, handler);
}

static void XMLCALL
Interface_StartElement(void *ud, const XML_Char *name, const XML_Char **atts)
{
	XML_Parser	parser = (XML_Parser)ud;
	SDL_Surface	*s = XML_GetUserData(parser);

	struct Tab	*tab;
	SDL_Color	color;

	if (!strcasecmp(name, "interface")) {
		FOREACH_ATTR(a, atts)
			if (!strcasecmp(*a, "foreground")) {
				if (DecodeColorAttrib(a[1], &color))
					goto err;

				display.foreground = SDL_MapRGB(s->format, color.r, color.g, color.b);
			} else if (!strcasecmp(*a, "background")) {
				if (DecodeColorAttrib(a[1], &color))
					goto err;

				display.background = SDL_MapRGB(s->format, color.r, color.g, color.b);
			} else
				goto err;
	} else if (!strcasecmp(name, "tab")) {
		registry.cTabs++;
		registry.tabs = realloc(registry.tabs,
					registry.cTabs*sizeof(struct Tab));
		if (!registry.tabs)
			goto allocerr;

		tab = registry.tabs + registry.cTabs - 1;
		memset(tab, 0, sizeof(struct Tab));

		FOREACH_ATTR(a, atts)
			if (!strcasecmp(*a, "label")) {
				if (!(tab->label = strdup(a[1])))
					goto allocerr;
			} else
				goto err;

		if (!tab->label)
			goto err;
	} else if (!strcasecmp(name, "slider") ||
		   !strcasecmp(name, "button") ||
		   !strcasecmp(name, "switch")) {
			   	/* common for all controls */

		struct Control *control;

		if (!registry.tabs)
			goto err;
		tab = registry.tabs + registry.cTabs - 1;

		tab->cControls++;
		tab->controls = realloc(tab->controls,
					tab->cControls*sizeof(struct Control));
		if (!tab->controls)
			goto allocerr;

		control = tab->controls + tab->cControls - 1;
		memset(control, 0, sizeof(struct Control));
				/* ^ already sets some default values (0/NULL) */

		FOREACH_ATTR(a, atts)
			if (!strcasecmp(*a, "geo")) {
				float x, y, w, h;

				if (sscanf(a[1], "%f %f %f %f",
					   &x, &y, &w, &h) != 4)
					goto err;

				control->geo.x = x*display.width/100;
				control->geo.y = y*display.height/100;
				control->geo.w = w*display.width/100;
				control->geo.h = h*display.height/100;
			} else if (!strcasecmp(*a, "OSCAddress")) {
				/* FIXME: check the OSC address string */
				if (!(control->OSC.address = strdup(a[1])))
					goto allocerr;
			} else if (!strcasecmp(*a, "OSCDataType")) {
				control->OSC.datatype = CaseEnumMap(OSC_DATATYPE, a[1], OSC_INT);
				if (control->OSC.datatype == -1)
					goto err;
			}

				/* control-specific */

		if (!strcasecmp(name, "slider")) {
			struct Slider	*slider = &control->u.slider;
			struct Paint	*paint = &slider->paint;

			paint->u.plain.color = display.foreground;
					/* ^ slider color defaults to foreground color */

			FOREACH_ATTR(a, atts)
				if (!strcasecmp(*a, "type")) {
					slider->type = CaseEnumMap(SLIDER_TYPE, a[1], SLIDER_BUTTON);
					if (slider->type == -1)
						goto err;
				} else if (!strcasecmp(*a, "color")) {
					char *p;

					if ((p = strchr(a[1], ' '))) {
						paint->type = PAINT_GRAD;

						*p++ = '\0';
						if (DecodeColorAttrib(a[1], &paint->u.grad.top) ||
						    DecodeColorAttrib(p, &paint->u.grad.bottom))
							goto err;
					} else {
						if (DecodeColorAttrib(a[1], &color))
							goto err;
						paint->u.plain.color = SDL_MapRGB(s->format, color.r, color.g, color.b);
					}
				} else if (!strcasecmp(*a, "min")) {
					if (sscanf(a[1], "%lf", &slider->min) == EOF)
						goto err;
				} else if (!strcasecmp(*a, "max")) {
					if (sscanf(a[1], "%lf", &slider->max) == EOF)
						goto err;
				} else if (!strcasecmp(*a, "step")) {
					if (sscanf(a[1], "%lf", &slider->step) == EOF)
						goto err;
				} else if (!strcasecmp(*a, "label")) {
					if (!(slider->label = strdup(a[1])))
						goto allocerr;
				} else if (!strcasecmp(*a, "showValue"))
					slider->show_value = strcasecmp(a[1], "false");

			if (slider->min >= slider->max || slider->step < 0)
				goto err;

			Interface_PrepareCDBuffer(parser, Interface_EndSlider);
		} else { /* button/switch field */
			struct Field *field = &control->u.field;

			control->type = FIELD;

			field->color = display.foreground; /* default color */

			FOREACH_ATTR(a, atts)
				if (!strcasecmp(*a, "color")) {
					if (DecodeColorAttrib(a[1], &color))
						goto err;
					field->color = SDL_MapRGB(s->format, color.r, color.g, color.b);
				} else if (!strcasecmp(*a, "label")) {
					if (!(field->label = strdup(a[1])))
						goto allocerr;
				}

				/* field->type is FIELD_BUTTON by default */

			if (!strcasecmp(name, "switch")) {
				field->type = FIELD_SWITCH;
				Interface_PrepareCDBuffer(parser, Interface_EndSwitch);
			}
		}
	} else
		goto err;

	return;

err:

	fprintf(stderr, "Error parsing interface file (line %u, column %u)\n",
		XML_GetCurrentLineNumber(parser),
		XML_GetCurrentColumnNumber(parser));

allocerr:

	XML_StopParser(parser, XML_FALSE);
}

static void XMLCALL
Interface_EndSlider(void *ud, const XML_Char *name)
{
	XML_Parser	parser = (XML_Parser)ud;

	struct Tab	*tab = registry.tabs + registry.cTabs - 1;
	struct Control	*control = tab->controls + tab->cControls - 1;
	struct Slider	*slider = &control->u.slider;

	double		val;

	if (strcasecmp(name, "slider")) {
		XML_StopParser(parser, XML_FALSE);
		return;
	}

	if (*Interface_CDBuffer) {
		if(sscanf(Interface_CDBuffer, "%lf", &val) == EOF ||
		   val < slider->min || val > slider->max) {
			XML_StopParser(parser, XML_FALSE);
			return;
		}
	} else
		val = slider->min;

	Controls_SetSliderValue(slider, val);

	if (slider->type == SLIDER_BUTTON)
		Controls_InitSliderButton(control);

	XML_SetCharacterDataHandler(parser, NULL);
	XML_SetElementHandler(parser, Interface_StartElement, NULL);
}

static void XMLCALL
Interface_EndSwitch(void *ud, const XML_Char *name)
{
	XML_Parser	parser = (XML_Parser)ud;

	struct Tab	*tab = registry.tabs + registry.cTabs - 1;
	struct Control	*control = tab->controls + tab->cControls - 1;

	if (strcasecmp(name, "switch")) {
		XML_StopParser(parser, XML_FALSE);
		return;
	}

	if (*Interface_CDBuffer)
		control->u.field.value = !strcasecmp(Interface_CDBuffer, "true");

	XML_SetCharacterDataHandler(parser, NULL);
	XML_SetElementHandler(parser, Interface_StartElement, NULL);
}

int
Xml_ReadInterface(const char *file, SDL_Surface *surface)
{
	FILE		*f;
	XML_Parser	parser;

	Uint8		buf[1024];
	size_t		len;

	if (!(f = fopen(file, "r")))
		return 1;

	if (!(parser = XML_ParserCreate(NULL))) {
		fclose(f);
		return 1;
	}

	XML_UseParserAsHandlerArg(parser);
	XML_SetUserData(parser, surface);
	XML_SetStartElementHandler(parser, Interface_StartElement);

	do {
		len = fread(buf, 1, sizeof(buf), f);

		if ((ferror(f) && errno != EINTR) ||
		    XML_Parse(parser, buf, len, feof(f)) == XML_STATUS_ERROR) {
			XML_ParserFree(parser);
			fclose(f);
			return 1;
		}
	} while (!feof(f));

	XML_ParserFree(parser);
	fclose(f);
	return !registry.tabs;
}

