/*
 *  (C) 2000-2004 Alan Donovan
 *
 *  Author: Alan Donovan <adonovan@lcs.mit.edu>
 *
 *  With thanks to:        T.Sato <VEF00200@nifty.ne.jp>.
 *                        http://member.nifty.ne.jp/tsato/tools/xvkbd-e.html
 *  and:                Derek Martin (derek_martin at agilent dot com)
 *                         (for -count and XGetInputFocus)
 *  and:                Hanno Hecker <h.hecker@bee.de>
 *                      (for symbolic keysym and modifier lookup.)
 *
 *  xsendkey.cxx -- General purpose keypress generator
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU General Public License for more details.
 *  <http://www.gnu.org/copyleft/gpl.html>.
 *
 *  $Id: xsendkey.c,v 1.3 2004/11/11 16:10:01 adonovan Exp $
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#  include "windows_x11.h"
#else
#  define NeedFunctionPrototypes 1
#  include <X11/Xlib.h>
#  include <X11/keysym.h>
#  if XlibSpecificationRelease != 6
#      error Requires X11R6
#  endif
#endif

int CustomXErrorHandler(Display *my_display, XErrorEvent *event);
void SendEvent(XKeyEvent *event);
void SendKeyPressedEvent(KeySym keysym, unsigned int shift);

static char *progname = NULL;
static char *displayname = NULL;
static Window window = 0;
static Display *display = NULL;

typedef struct _codepoint {
	char **keyname;
	int shift;
	int keysym;
	unsigned long count;
	struct _codepoint *next;
} codepoint;

void parse(codepoint *pt);
void usage();


int main(int argc, char **argv) {
	progname = argv[0];

	int i, Junk;
	unsigned long count = 1;
	codepoint *point = NULL;
	codepoint *current = NULL;
	codepoint *first = NULL;

	first = malloc(sizeof(codepoint));
	if (first == NULL) {
		fprintf(stderr, "malloc()");
		exit(1);
	}
	point = first;

	for (i=1; i < argc; i++) {
		if (strcmp(argv[i], "-display") == 0) {
			if (++i == argc)
				usage();

			displayname = argv[i];
		}
		else if (strcmp(argv[i], "-count") == 0) {
			if (++i == argc)
				usage();

			count = strtoul(argv[i], NULL, 0);
		}
		else if (strcmp(argv[i], "-window") == 0) {
			if (++i == argc)
				usage();

			if (strcmp(argv[i], "root") == 0)
				window = (Window)-1;
			else
				window = (Window)strtoul(argv[i], NULL, 0);
		}
		else if (argv[i][0] == '-') {
			fprintf(stderr, "%s: bad option `%s'.\n", progname, argv[i]);
			usage();
		}
		else {
			current = malloc(sizeof(codepoint));
			if (current == NULL) {
				fprintf(stderr, "malloc()");
				exit(1);
			}
			current->count = 1;
			current->keyname = &argv[i];
			parse(current);
			if ((*current->keyname)[0] == 0) {
				fprintf(stderr, "%s: you must specify a keyname.\n", progname);
				exit(1);
			}
			point->next = current;
			point = point->next;
		}
	}

	if (displayname == NULL)
		displayname = getenv("DISPLAY");

	if (displayname == NULL)
		displayname = ":0.0";

	display = XOpenDisplay(displayname);

	if (display == NULL) {
		fprintf(stderr, "%s: can't open display `%s'.\n", progname, displayname);
		exit(1);
	}

	if (window == 0)
		XGetInputFocus(display, &window, &Junk);

	if (window == (Window)-1)
		window = RootWindow(display, 0); // FIXME: nonzero screens?

	current = first;
	for (current = first; current != NULL; current = current->next) {
		SendKeyPressedEvent(current->keysym, current->shift);
	}

	XCloseDisplay(display);

	return 0;
}

void usage()
{
	fprintf(stderr,
		"usage: %s [-window <id>|root]\n"
		"            [-count <count>]\n"
		"            [-display <name>] [Modifier+[Modifier+]]Key...\n"
		"(looks up keysyms in <X11/keysymdef.h>)\n"
		"'Alt',   'Control',   'Shift' are replaced by\n"
		"'Alt_L', 'Control_L', 'Shift_L'...\n"
		"  example:\n"
		"     %s Control+Alt+Right\n", progname, progname);
	exit(1);
}

void parse(codepoint *pt) {
	int c;
	char *temp;
	pt->keysym = 0;
	pt->shift  = 0;
	// fprintf(stderr, "keyname: %s\n", keyname);
	temp = strtok((char *)(*pt->keyname), "+");
	while (temp != NULL) {
		// fprintf(stderr, "temp: %s\n", temp);
		if (strcmp(temp, "Alt") == 0)
			c = XK_Alt_L;
		else if (strcmp(temp, "Control") == 0)
			c = XK_Control_L;
		else if (strcmp(temp, "Shift") == 0)
			c = XK_Shift_L;
		else
			c = XStringToKeysym(temp);

		if (c == 0) {
			fprintf(stderr, "converted\n");
			c = atoi(temp);
		}
		if (c == 0) {
			fprintf(stderr, "unknown: %s\n", temp);
			usage();
		}

		switch (c) {
			case XK_Shift_L:
			case XK_Shift_R:
				pt->shift |= ShiftMask;
				break;
			case XK_Control_L:
			case XK_Control_R:
				pt->shift |= ControlMask;
				break;
			case XK_Caps_Lock:
			case XK_Shift_Lock:
				pt->shift |= LockMask;
				break;
			case XK_Meta_L:
			case XK_Meta_R:
			case XK_Alt_L:
			case XK_Alt_R:
				pt->shift |= Mod1Mask;
				break;
			case XK_Super_L:
			case XK_Super_R:
			case XK_Hyper_L:
			case XK_Hyper_R:
				break;
			default:
				pt->keysym = c;
		}
		// fprintf(stderr, "keysym: 0x%x, shift: %d\n", keysym, shift);
		temp = strtok(NULL, "+");
	}
	/* keysym = strtoul(argv[i], NULL, 0); */
}

int CustomXErrorHandler(Display *my_display, XErrorEvent *event) {
	fprintf(stderr, "%s: XSendEvent(0x%lx) failed.\n", progname, window);
	return 1;
}

void SendEvent(XKeyEvent *event) {
	XSync(display, False);
	XSetErrorHandler(CustomXErrorHandler);
	XSendEvent(display, window, True, KeyPressMask, (XEvent*)event);
	XSync(display, False);
	XSetErrorHandler(NULL);
}

void SendKeyPressedEvent(KeySym keysym, unsigned int shift) {
	XKeyEvent event;

	// Meta not yet implemented (Alt used instead ;->)
	int meta_mask = 0;

	event.display = display;
	event.window = window;
	event.root = RootWindow(display, 0); // XXX nonzero screens?
	event.subwindow = None;
	event.time = CurrentTime;
	event.x = 1;
	event.y = 1;
	event.x_root = 1;
	event.y_root = 1;
	event.same_screen = True;
	event.type = KeyPress;
	event.state = 0;

	//
	// press down shift keys one at a time...
	//

	if (shift & ShiftMask) {
		event.keycode = XKeysymToKeycode(display, XK_Shift_L);
		SendEvent(&event);
		event.state |= ShiftMask;
	}
	if (shift & ControlMask) {
		event.keycode = XKeysymToKeycode(display, XK_Control_L);
		SendEvent(&event);
		event.state |= ControlMask;
	}

	if (shift & Mod1Mask) {
		event.keycode = XKeysymToKeycode(display, XK_Alt_L);
		SendEvent(&event);
		event.state |= Mod1Mask;
	}
	if (shift & meta_mask) {
		event.keycode = XKeysymToKeycode(display, XK_Meta_L);
		SendEvent(&event);
		event.state |= meta_mask;
	}

	//
	//  Now with shift keys held down, send event for the key itself...
	//


	// fprintf(stderr, "sym: 0x%x, name: %s\n", keysym, keyname);
	if (keysym != NoSymbol) {
		event.keycode = XKeysymToKeycode(display, keysym);
		// fprintf(stderr, "code: 0x%x, %d\n", event.keycode, event.keycode );
		SendEvent(&event);

		event.type = KeyRelease;
		SendEvent(&event);
	}

	//
	// Now release all the shift keys...
	//

	if (shift & ShiftMask) {
		event.keycode = XKeysymToKeycode(display, XK_Shift_L);
		SendEvent(&event);
		event.state &= ~ShiftMask;
	}
	if (shift & ControlMask) {
		event.keycode = XKeysymToKeycode(display, XK_Control_L);
		SendEvent(&event);
		event.state &= ~ControlMask;
	}
	if (shift & Mod1Mask) {
		event.keycode = XKeysymToKeycode(display, XK_Alt_L);
		SendEvent(&event);
		event.state &= ~Mod1Mask;
	}
	if (shift & meta_mask) {
		event.keycode = XKeysymToKeycode(display, XK_Meta_L);
		SendEvent(&event);
		event.state &= ~meta_mask;
	}
}
