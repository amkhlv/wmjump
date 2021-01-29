/* license 

wmjump
a program for the keyboard navigation of the desktop; 
use the keyboard to switch between windows and workspaces

Author: Andrei Mikhailov <a.mkhlv@gmail.com>

Copyright (C) 2008

This program is free software which I release under the GNU General Public
License. You may redistribute and/or modify this program under the terms
of that license as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

To get a copy of the GNU General Puplic License,  write to the
Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/* license {{{ */
/* 

wmctrl
A command line tool to interact with an EWMH/NetWM compatible X Window Manager.

Author, current maintainer: Tomas Styblo <tripie@cpan.org>

Copyright (C) 2003

This program is free software which I release under the GNU General Public
License. You may redistribute and/or modify this program under the terms
of that license as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

To get a copy of the GNU General Puplic License,  write to the
Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/
/* }}} */


#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/Xmu/WinUtil.h>
#include <glib.h>

#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */



#define MAX_PROPERTY_VALUE_LEN 4096
#define SELECT_WINDOW_MAGIC ":SELECT:"
#define ACTIVE_WINDOW_MAGIC ":ACTIVE:"


/* declarations of static functions *//*{{{*/
gboolean wm_supports (Display *disp, const gchar *prop);
Window *get_client_list (Display *disp, unsigned long *size);
int client_msg(Display *disp, Window win, char *msg, 
        unsigned long data0, unsigned long data1, 
        unsigned long data2, unsigned long data3,
        unsigned long data4);
int list_windows (Display *disp);
int list_desktops (Display *disp);
int showing_desktop (Display *disp);
int change_viewport (Display *disp);
int change_geometry (Display *disp);
int change_number_of_desktops (Display *disp);
int switch_desktop (Display *disp);
int wm_info (Display *disp);
gchar *get_output_str (gchar *str, gboolean is_utf8);
int action_window (Display *disp, Window win, char mode);
int action_window_pid (Display *disp, char mode);
int action_window_str (Display *disp, char mode);
int activate_window (Display *disp, Window win, 
        gboolean switch_desktop);
int close_window (Display *disp, Window win);
int longest_str (gchar **strv);
int window_to_desktop (Display *disp, Window win, int desktop);
void window_set_title (Display *disp, Window win, char *str, char mode);
gchar *get_window_title (Display *disp, Window win);
gchar *get_window_class (Display *disp, Window win);
gchar *get_property (Display *disp, Window win, 
        Atom xa_prop_type, gchar *prop_name, unsigned long *size);
void init_charset(void);
int window_move_resize (Display *disp, Window win, char *arg);
int window_state (Display *disp, Window win, char *arg);
Window Select_Window(Display *dpy);
Window get_active_window(Display *dpy);
unsigned long get_timestamp (Display* dsp);
/*}}}*/

struct optstruct {
    int verbose;
    int force_utf8;
    int show_class;
    int show_pid;
    int show_geometry;
    int match_by_id;
	int match_by_cls;
    int full_window_title_match;
    int wa_desktop_titles_invalid_utf8;
    char *param_window;
    char *param;
}; 
extern struct optstruct options;

extern gboolean envir_utf8;

extern struct timespec wait_time_short;
extern struct timespec wait_time_middle;
extern struct timespec wait_time_long;
extern gboolean verbose;
