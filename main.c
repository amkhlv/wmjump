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

#include "wmctrl.h"
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>


#define MAX_PROPERTY_VALUE_LEN 4096
#define p_verbose(...) if (verbose) { \
    fprintf(stderr, __VA_ARGS__); \
}
#define p_verb(...) if (verbose) { \
    fprintf(stdout, __VA_ARGS__); \
}
#define TRUNCATE_TO_LEN 40
#define CSSFILE "wmjump.css"
#define BLACKLISTFILE "wmjump-blacklist"
#define GOBACKFILE "wmjump-previous-window"
#define PIPEFILE "pipe.fifo"
#define MESSAGEFILE "wmjump-message"
#define TIME_SHORT   30000000
#define TIME_MIDDLE 120000000
#define TIME_LONG   160000000
#define BORDERWIDTH 3
#define CHECK 0x2744
#define CHECK1 0x2714


static void do_what_user_said ( Display* dsp, char* next_command );

static void get_list_from_wm(   Display *dsp, 
                                Window *client_list,
                                unsigned long client_list_size,
                                int *number_of_buttons,
                                int *window_number,
                                gchar **title_of_button,
                                gchar **name_of_style,
                                int groupnumber,
                                Window *win_we_leave,
                                gboolean *win_we_leave_is_blacklisted ) ;

static Window window_now_active(Display *dsp);

static void record_active_win (Window wn);

static void our_user_interface(
                                int pipe_descr[2],
                                Window* client_list,
                                int client_list_size,
                                int number_of_buttons, 
                                gchar** title_of_button, 
                                gchar** name_of_style,
                                int* window_number,
                                Window window_we_leave,
                                gboolean window_we_leave_is_blacklisted,
                                int loc_x, int loc_y, int timeout_sec) ;

static char *truncate_title(gchar *st);


gboolean current_only;
gboolean do_check_desktop;
static gboolean given_groupnumber;
static gboolean reverse_list;
static gboolean autodestroy_on_lost_focus;
static gboolean boldface;
static gboolean piped;
static gboolean double_clutch;
static gboolean add_timeout;
static gboolean move_window;


static const gchar* lttrs = "a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z";
static gchar** lttr;
static gchar* home;




int main(int argc, char **argv)
{
 current_only = False;
 do_check_desktop = False;
 given_groupnumber = False;
 autodestroy_on_lost_focus = True;
 boldface = False;
 piped = False;
 double_clutch = True;
 add_timeout = False;
 move_window = False;
 verbose = False;

 int groupnumber = 0;
 int loc_x = 0; int loc_y = 0;
 int timeout_sec;
 

/* scanning the command line : */
 int ia; 
 for (ia = 0; ia < argc; ia++) 
   { 
   if ((strcmp(argv[ia],"-c") == 0) || (strcmp(argv[ia],"--current") == 0)) 
        { current_only=True; }
   else if ((strcmp(argv[ia],"-r") == 0) || (strcmp(argv[ia],"--reverse") == 0)) 
        { reverse_list=True; }
   else if ((strcmp(argv[ia],"-p") == 0) || (strcmp(argv[ia],"--persist") == 0)) 
        { autodestroy_on_lost_focus = False; }
   else if (strcmp(argv[ia],"--rich") == 0)   { boldface=True; }
   else if (strcmp(argv[ia],"--pipe") == 0)   { piped=True; }
   else if (strcmp(argv[ia],"--easy") == 0)   { double_clutch=False; }
   else if (sscanf(argv[ia],"--timeout=%d", &timeout_sec) == 1) { add_timeout=True; }
   else if ((strcmp(argv[ia],"-v") == 0) || (strcmp(argv[ia],"--verbose") == 0))
        { verbose = True;  }
   else if (sscanf(argv[ia],"--location=%dx%d", &loc_x, &loc_y) == 2)
        { move_window=True; }
   if (strcmp(argv[ia],"-g") == 0)
            {
             ia++; 
             if (sscanf(argv[ia],"%d",&groupnumber) == 1) 
                   {given_groupnumber = True;} else {
                             printf("\n**** parsing error of -g ***\n");
                                                     };
            }
   else if (strncmp(argv[ia],"-g",2) == 0)
            {
             if (sscanf(argv[ia],"-g%d",&groupnumber) == 1)  
                {given_groupnumber = True;}
            }
   }

 if ((given_groupnumber) && (verbose)) {
  printf("\nwmjump invoked with -g %d => assuming we are on workspace No. %d ...\n\n",
         groupnumber, groupnumber);
                        }

 lttr = g_strsplit(lttrs, "," , 0);

 wait_time_short.tv_sec=0; wait_time_short.tv_nsec=TIME_SHORT;
 wait_time_middle.tv_sec=0; wait_time_middle.tv_nsec=TIME_SHORT;
 wait_time_long.tv_sec=0; wait_time_long.tv_nsec=TIME_LONG;

 setlocale(LC_ALL, "");
 init_charset();

 home = getenv("HOME");

 /* Checking if ~/.wmjump exists, and if not creating it: */
 gchar *wmjump_dirname = g_strconcat(home,"/.wmjump/",NULL);
 if (!g_file_test(wmjump_dirname, G_FILE_TEST_EXISTS)) {
     g_print ("Creating %s directory. \n", wmjump_dirname);
     if (g_mkdir(wmjump_dirname, 0700)==-1) { 
        fputs("ERROR: Cannot create directory ~/.wmjump \n", stderr);
        return EXIT_FAILURE; }
 } 

 /* Checking the pipe: */
    if (piped) {
        remove(g_strconcat(home,"/.wmjump/",PIPEFILE,NULL));
        mknod( g_strconcat(home,"/.wmjump/",PIPEFILE,NULL), S_IFIFO|0666, 0);
    }

 /* ---------------- MAIN LOOP --------------- */
 /* The loop is needed only in piped mode, in normal mode no need to loop */
 gboolean not_enough = True;
 while (not_enough) { if (verbose) printf("wmjump is entering main loop\n"); 
  /* Pipework: */
  int pipe_descr[2];
  if (pipe(pipe_descr) != 0) {printf("wmjump: --- could not open pipe ---\n"); return EXIT_FAILURE; }
 
  if (fork()) { /* parent process */ 
     char next_command[100];
     close(pipe_descr[1]);
     if ( read(pipe_descr[0], next_command, sizeof(next_command)) == -1 ) {
                        printf("wmjump: --- could not read the pipe ---\n"); return EXIT_FAILURE; }
 
 
 
     Display *disp;
     if (! (disp = XOpenDisplay(NULL))) {
         fputs("Cannot open display.\n", stderr);
         return EXIT_FAILURE;
     }
 
     do_what_user_said (disp, next_command);
 
     XCloseDisplay(disp);
     int child_status;
     wait(&child_status);
     not_enough = False ;
     if (! WIFEXITED(child_status)) { printf("wmjump: error") ;}
     else { if (piped) {if (WEXITSTATUS(child_status) == 3)  {not_enough = True;} }}
     /* if ((child_status/256 == 3) && piped ) {not_enough = True;} */
     }
  else {  /* child process */
     close(pipe_descr[0]);
     int exit_code = 0;
 
     if (piped) {
         FILE *named_pipe;
         named_pipe = fopen(g_strconcat(home,"/.wmjump/",PIPEFILE,NULL), "r");
         gchar go_string[20];
         gchar *line = fgets(go_string, 20, named_pipe);
         if      (strcmp(go_string,"all\n") == 0)     { current_only=False; exit_code = 3; }
         else if (strcmp(go_string,"current\n") == 0) { current_only=True;  exit_code = 3; }
         else {  /* exiting the program */
             printf("wmjump: EXITING\n");
             char a[100];
             sprintf(a,"do_nothing");
             if ( write(pipe_descr[1], a, strlen(a)+1) == -1 ) 
                 printf("wmjump: --- could not write to pipe ---\n"); 
             exit(0);
         }
 
         fclose(named_pipe);
     }
 
     if (current_only) { do_check_desktop=True; }
 
     Display *disp;
     if (! (disp = XOpenDisplay(NULL))) {
         fputs("wmjump: Cannot open display.\n", stderr);
         return EXIT_FAILURE;
     }
 
     Window *client_list_direct_order;
     unsigned long client_list_size;
     if ((client_list_direct_order = get_client_list(disp, &client_list_size)) == NULL) {
         printf("wmjump: Cannot get client list");
         return EXIT_FAILURE; 
     }
     
     int szofw=sizeof(Window);
     Window *client_list = g_malloc(client_list_size*szofw);
     /* reversing the list if required */
     int j; 
     for (j=0; j<client_list_size/szofw; j++)
         { 
           if (reverse_list) 
                { client_list[j] = client_list_direct_order[client_list_size/szofw - j - 1]; }
           else { client_list[j] = client_list_direct_order[j]; }
         }
 
     int number_of_buttons;
     int* window_number = malloc(client_list_size / sizeof(Window) * sizeof(int));
     gchar** title_of_button; 
     gchar** name_of_style;
     title_of_button = g_malloc0( ( client_list_size/sizeof(Window) + 1) * sizeof(gchar*) );
     name_of_style   = g_malloc0( ( client_list_size/sizeof(Window) + 1) * sizeof(gchar*) );
     Window win_we_leave;
     gboolean win_we_leave_is_blacklisted ;
 
     get_list_from_wm(   disp, 
                         client_list, 
                         client_list_size, 
                         &number_of_buttons, 
                         window_number,
                         title_of_button,
                         name_of_style,
                         groupnumber,
                         &win_we_leave,
                         &win_we_leave_is_blacklisted) ;
 
 
     XSync(disp,FALSE);
     XCloseDisplay(disp);
     
     our_user_interface(pipe_descr,
                        client_list,
                        client_list_size,
                        number_of_buttons, 
                        title_of_button, 
                        name_of_style,
                        window_number,
                        win_we_leave,
                        win_we_leave_is_blacklisted,
                        loc_x, loc_y, timeout_sec);
 
     g_free(client_list); client_list = NULL ;
     g_free(client_list_direct_order); client_list_direct_order = NULL ;
     free(window_number); window_number = NULL ;
     g_strfreev(title_of_button);  title_of_button = NULL ;
     g_strfreev(name_of_style);  name_of_style = NULL ;
     exit(exit_code);
     }
 }  if (verbose) { printf("wmjump exiting main loop\n"); 
                   printf("=======================================================\n\n"); }
 g_strfreev(lttr); lttr = NULL ;
 exit(0);
}


static Window window_now_active(Display* disp) 
    {
    Atom _NET_ACTIVE_WINDOW = XInternAtom(disp, "_NET_ACTIVE_WINDOW", False);
    Window active_win = None;
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop_return = NULL;
    XGetWindowProperty(disp, DefaultRootWindow(disp), _NET_ACTIVE_WINDOW, 0L, sizeof(Window),
                                     False, XA_WINDOW, &actual_type,
                                     &actual_format, &nitems, &bytes_after,
                                     &prop_return);
    active_win = *(Window *)prop_return;
    return active_win;
    XFree(prop_return);
}

static void record_active_win (Window win) 
    {
    gchar *filename = g_strconcat(home,"/.wmjump/",GOBACKFILE,NULL) ;
    FILE *file_with_old_win;
    file_with_old_win = fopen(filename, "w");
    fprintf(file_with_old_win,"%x\n",(int)win);
    fclose(file_with_old_win);
    g_free(filename); filename = NULL ;
}

static void do_what_user_said ( Display* disp1, char* next_command ) 
    {
    unsigned long tstamp = 0;
    tstamp = get_timestamp(disp1);
    /* printf("wmjump: TIMESTAMP %ld\n", tstamp); */
    if      ( g_strrstr(next_command,g_strconcat("do_nothing",NULL))) {
            }
    else if ( g_strrstr(next_command,g_strconcat("go_back",NULL))) {
            int wnum ;
            sscanf(next_command,"go_back to %x",&wnum);
            if (verbose) {printf("\n"); printf("wmjump: going back to window %x\n",wnum); }
            int j, ntimes = double_clutch ? 2 : 1 ;
            for (j=0; j<ntimes; j++) {
               if (double_clutch) { nanosleep(&wait_time_middle,NULL); }
               client_msg(disp1, (Window)wnum, "_NET_ACTIVE_WINDOW", 0, tstamp, 0, 0, 0);
               XFlush(disp1);
                              }
            }
    else if ( g_strrstr(next_command,g_strconcat("switch_to_",NULL))) {
            int target;
            Window win1;
            sscanf(next_command,"switch_to_%d",&target);
            if (verbose) printf("wmjump: switching to desktop %d\n",target);
            client_msg(disp1, DefaultRootWindow(disp1), "_NET_CURRENT_DESKTOP", 
                (unsigned long)target, tstamp, 0, 0, 0);
            XSync(disp1,FALSE);
            if (double_clutch) { nanosleep(&wait_time_middle ,NULL); }
            win1 = DefaultRootWindow(disp1);
            client_msg(disp1, win1, "_NET_ACTIVE_WINDOW", 0, tstamp, 0, 0, 0);
            XMapRaised(disp1, win1);
            }
   else    {
            int wnum ;
            sscanf(next_command,"%x",&wnum);
            if (verbose) {printf("\n"); printf("wmjump: switching to window %x\n",wnum); }
            int j, ntimes = double_clutch ? 2 : 1 ;
            for (j=0; j<ntimes; j++){
               if (double_clutch) { nanosleep(&wait_time_middle,NULL); }
               client_msg(disp1, (Window)wnum, "_NET_ACTIVE_WINDOW", 0, tstamp, 0, 0, 0);
               XFlush(disp1);
                             }
            }
    }

static void get_list_from_wm(   Display *disp, 
                                Window *client_list,
                                unsigned long client_list_size,
                                int *number_of_buttons,
                                int *window_number,
                                gchar **title_of_button,
                                gchar **name_of_style,
                                int groupnumber,
                                Window *win_we_leave,
                                gboolean  *win_we_leave_is_blacklisted) {

    /* Look up which window classes are blacklisted: */
    gchar *blacklist;
    gchar *file_blacklist_in_home_dir = g_strconcat(home,"/.wmjump/",BLACKLISTFILE,NULL) ;
    if ( g_file_test(file_blacklist_in_home_dir, G_FILE_TEST_EXISTS) )
        {
        g_file_get_contents( file_blacklist_in_home_dir , &blacklist,NULL,NULL);
        }
    else
        {
        g_file_get_contents( g_strconcat("/etc/wmjump/",BLACKLISTFILE,NULL) , &blacklist,NULL,NULL);
        }
    g_free(file_blacklist_in_home_dir); file_blacklist_in_home_dir = NULL ;

    unsigned long *desktop_viewport = NULL;
    unsigned long desktop_viewport_size = 0;
    int desk_size_x = 0;
    int desk_size_y = 0;
    int screen_size_x = 0; 
    int screen_size_y = 0; 
    int desk_left_marg = 0;
    int desk_top_marg = 0;


    *win_we_leave_is_blacklisted = TRUE ; 
    *number_of_buttons = 0;
    Window active_window = window_now_active(disp);
    *win_we_leave = active_window;
    p_verb("active window = %x\n",(int)active_window);
    gchar *itemclass ;
    /* Check if active_window is one from the client list: */
    gboolean active_window_is_strange = TRUE ;
    int i ;
    for (i = 0; i < client_list_size / sizeof(Window); i++) {
        if (active_window == client_list[i]) { active_window_is_strange = FALSE ; break ; }
        }
    if (active_window_is_strange) { p_verbose("wmjump: *** STRANGE ACTIVE WINDOW: win=%x ***\n", (int)active_window); 
        itemclass = "UNDETECTED" ; }
    else {
        gchar *itemclass = get_window_class(disp,active_window);
        gchar *class_is_blacklisted = g_strrstr(g_strconcat("\n",blacklist,"\n",NULL),
                                                g_strconcat("\n",itemclass,"\n",NULL));
        if (class_is_blacklisted == NULL) { *win_we_leave_is_blacklisted = FALSE ; } 
         }
    
    for (i = 0; i < client_list_size / sizeof(Window); i++) {
        gchar *title_utf8 = get_window_title(disp, client_list[i]); 
        gchar *title_out = get_output_str(title_utf8, TRUE);
        
        if (verbose) printf("%d %s\n", i, title_out ? title_out : "N/A");

        gchar *itemclass = get_window_class(disp,client_list[i]);

        if (verbose) printf("window class: %s WINDOW: %x\n",itemclass,(gint)client_list[i]);
        if (verbose) printf("_______________________\n");

        gchar *class_is_blacklisted = g_strrstr(g_strconcat("\n",blacklist,"\n",NULL),
                                                g_strconcat("\n",itemclass,"\n",NULL));

        if (class_is_blacklisted == NULL) {
            unsigned long *desktop;
        
            if ((desktop = (unsigned long *)get_property(disp, client_list[i],
                    XA_CARDINAL, "_NET_WM_DESKTOP", NULL)) == NULL) {
                if ((desktop = (unsigned long *)get_property(disp, client_list[i],
                        XA_CARDINAL, "_WIN_WORKSPACE", NULL)) == NULL) {
                    printf("wmjump: Cannot find desktop ID of the window.\n"); }}

            gboolean in_scope;
            int junkx, junky;
            unsigned int wwidth, wheight, bw, depth;
            Window junkroot;
            int xm,ym;
     
            if (do_check_desktop) {
                unsigned long *now_desktop;
                if (given_groupnumber) { 
                                         unsigned long uldn = (unsigned long)groupnumber;
                                         now_desktop = &uldn;
                                       }
                else {
                    if (active_window_is_strange) { p_verbose("wmjump: *** COULD NOT DETECT ACTIVE WINDOW ***\n"); 
                                              unsigned long fake_desktop = 9999; 
                                              now_desktop = &fake_desktop; }
                    else {
                        if ((now_desktop = (unsigned long *)get_property(disp, active_window,
                            XA_CARDINAL, "_NET_WM_DESKTOP", NULL)) == NULL) {
                        if ((now_desktop = (unsigned long *)get_property(disp, active_window,
                            XA_CARDINAL, "_WIN_WORKSPACE", NULL)) == NULL) {
                        printf("wmjump: Cannot find desktop ID of the window.\n"); }}
                    }
                }
                in_scope = (*desktop == *now_desktop);
                if (! active_window_is_strange) { g_free(now_desktop); now_desktop = NULL ; } 
                                   } 
            else {in_scope = True;}

            if ( in_scope || !current_only ) {

            int n = *number_of_buttons;
            window_number[n] = i;

            gchar desk_id[12]; /* xx,yy perhaps use some unicode instead of comma */
            sprintf(desk_id, "%d", (int)*desktop+1);
            gchar *title_tr;
            title_tr = truncate_title(title_out);
            gchar *itemtitle ;
            /* Now want to set the label string; put asterisk if active */
            gchar *lttr_bf;
            if (boldface) { lttr_bf = g_strconcat("<span weight=\"bold\">", lttr[n],
                                                  "</span>", NULL); }
            else    { lttr_bf = lttr[n]; }
            if ( client_list[i] == active_window ) {
                gchar asterisk[8];
                sprintf(asterisk, "%lc", CHECK);
                if (boldface) {
                    itemtitle = g_strconcat("<big>", asterisk, " ", desk_id," (",lttr_bf,") ",
                        "<u>",title_tr,"</u>"," (",lttr_bf,") ", asterisk, "</big>", NULL);
                                             }
                else { itemtitle = g_strconcat(asterisk, " ", desk_id," (",lttr_bf,") ",
                                        title_tr," (",lttr_bf,") ", asterisk, NULL);
                      }
            } else { 
                itemtitle = g_strconcat(desk_id," (",lttr_bf,") ",
                                        title_tr," (",lttr_bf,") ",NULL); 
            }    
            title_of_button[n]=itemtitle; 
            name_of_style[n] = itemclass ;

            (*number_of_buttons)++  ;
            if ( *number_of_buttons == g_strv_length(lttr) ) break;
            } 
            g_free(desktop);  desktop = NULL ;
            }
    }
    title_of_button[*number_of_buttons]=NULL;
    name_of_style[*number_of_buttons]=NULL;

    g_free(blacklist); blacklist = NULL ;
    g_free(desktop_viewport); desktop_viewport = NULL ;
}


static void our_user_interface(  
                            int pipe_descr[2],
                            Window* client_list,
                            int client_list_size,
                            int number_of_buttons, 
                            gchar** title_of_button, 
                            gchar** name_of_style,
                            int* window_number,
                            Window win_we_leave,
                            gboolean win_we_leave_is_blacklisted,
                            int loc_x, int loc_y, int timeout_sec) {

    gtk_init(NULL, NULL);
    g_object_set (gtk_settings_get_default (), "gtk-error-bell", False, NULL);
    GtkCssProvider *cssProvider  = gtk_css_provider_new();
    gtk_css_provider_load_from_path(cssProvider,   g_strconcat(home, "/.wmjump/",CSSFILE,NULL), NULL);

    GtkWidget *mainwin , *vbox, *message_frame;
    GtkWidget *itembut[client_list_size];
    GtkWidget *itemlabel[client_list_size];

    vbox = gtk_vbox_new (1, 0); 
    gtk_box_set_homogeneous(GTK_BOX(vbox), FALSE);
    message_frame = gtk_frame_new (NULL); 
    gtk_frame_set_shadow_type(GTK_FRAME(message_frame), GTK_SHADOW_OUT);

    void set_my_css_provider(GtkWidget *w) {
        gtk_style_context_add_provider(
                gtk_widget_get_style_context(w),
                GTK_STYLE_PROVIDER(cssProvider),
                GTK_STYLE_PROVIDER_PRIORITY_USER
                );
    }
    void add_my_css_class(GtkWidget *w, gchar *classname) {
        gtk_style_context_add_class(gtk_widget_get_style_context(w), classname);
    }

    void send_command_to_switch_desktop (int i) {
        char a[100];
        int len = sprintf(a,"switch_to_%d",i);
        if ( write(pipe_descr[1], a, len+1) == -1 ) printf("wmjump: --- could not write to pipe ---\n");
        /* it is important to have strlen(a)+1 because extra 1 accounts for NULL */
        if (! win_we_leave_is_blacklisted ) { record_active_win(win_we_leave); }
        }

    void send_command_to_activate_window (int i) {
        
        if (double_clutch) {
            Display *disp;
            disp = XOpenDisplay(NULL) ;
            activate_window(disp,(Window)(client_list[window_number[i]]),TRUE);
            XSync(disp,FALSE);
            XCloseDisplay(disp);
        }

        gint s;
        s = (gint)(client_list[window_number[i]]);
        char a[100];
        int len = sprintf(a,"%x",s);
        if ( write(pipe_descr[1], a, len+1) == -1 ) printf("wmjump: --- could not write to pipe ---\n");
        if (! win_we_leave_is_blacklisted ) { record_active_win(win_we_leave); }
        }

    void send_command_to_go_back () {
        gchar *filename = g_strconcat(home,"/.wmjump/",GOBACKFILE,NULL) ;
        FILE *file_with_old_win;
        char a[100];
        file_with_old_win = fopen(filename, "r");
        if (file_with_old_win != NULL)  {
            gchar wstring[20];
            char *line = fgets(wstring, 20, file_with_old_win);
            fclose(file_with_old_win);
            if (line != NULL) {
                int wnum ;
                sscanf(wstring,"%x\n",&wnum);

                if (double_clutch) {
                    Display *disp;
                    disp = XOpenDisplay(NULL) ;
                    activate_window(disp,(Window)wnum,TRUE);
                    XSync(disp,FALSE);
                    XCloseDisplay(disp);
                    }

                sprintf(a,"go_back to %x", wnum);
            }
            else {
                sprintf(a,"do_nothing");
            }
        } else {
            sprintf(a,"do_nothing");
        }
        if ( write(pipe_descr[1], a, strlen(a)+1) == -1 ) {
            printf("wmjump: --- could not write to pipe ---\n");  }
        if (! win_we_leave_is_blacklisted ) { record_active_win(win_we_leave); }
    }

    void send_command_to_do_nothing () {
        char a[100];
        int len = sprintf(a,"do_nothing");
        if ( write(pipe_descr[1], a, len+1) == -1 ) printf("wmjump: --- could not write to pipe ---\n"); 
        }

    void send_command_to_switch_viewport (int rw, int clmn) {
        char a[100];
        int len = sprintf(a,"compiz_%d_%d", rw, clmn);
        if ( write(pipe_descr[1], a, len+1) == -1 ) printf("wmjump: --- could not write to pipe ---\n"); 
        if (! win_we_leave_is_blacklisted ) { record_active_win(win_we_leave); }
        }

    void mainwin_destroy() { gtk_main_quit();}

    gchar   *message_file = g_strconcat(home,"/.wmjump/",MESSAGEFILE,NULL);
    gchar   *message1 ;
    gchar   *message ;
    message = g_strconcat("",NULL);
    if ( g_file_test(message_file, G_FILE_TEST_EXISTS) ) 
        { g_file_get_contents( message_file , &message1, NULL, NULL); 
          message = g_strconcat(message1,NULL); } 
    else message1 = g_strconcat("",NULL);
    g_free(message1); message1 = NULL ;
    g_free(message_file) ; message_file = NULL ;


    mainwin = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_wmclass(GTK_WINDOW(mainwin), "wmjump", "wmjump");
    gtk_window_set_title(GTK_WINDOW(mainwin), "wmjump");
    gtk_window_set_decorated (GTK_WINDOW(mainwin),FALSE);
    if (move_window) {
        gtk_window_set_gravity (GTK_WINDOW(mainwin),GDK_GRAVITY_NORTH); 
        gtk_window_move(GTK_WINDOW(mainwin), loc_x, loc_y);
                    }
    gtk_window_set_type_hint (GTK_WINDOW(mainwin),GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_container_set_border_width (GTK_CONTAINER(mainwin),BORDERWIDTH);
    gchar *windowname = "main_window";
    if (current_only) windowname = "main_window_currentonly";
    set_my_css_provider(mainwin);
    add_my_css_class(mainwin,windowname);
    g_signal_connect (G_OBJECT (mainwin), "destroy", G_CALLBACK (mainwin_destroy), NULL);

    void mainwin_lostfocus() { 
        if (add_timeout && autodestroy_on_lost_focus) {
            p_verb("wmjump:  either timeout or lost focus => exiting  (to persist on lost focus start with -p option) \n"); 
        } else if (add_timeout) {
            p_verb("wmjump:  timeout => exiting \n"); 
        } else if (autodestroy_on_lost_focus) {
            p_verb("wmjump:  lost focus => exiting  (to persist on lost focus start with -p option) \n"); 
        }
        send_command_to_do_nothing();
        gtk_main_quit();     }
    /* This is borrowed from 
       http://stackoverflow.com/questions/1925568/how-to-give-keyboard-focus-to-a-pop-up-gtk-window   :
    */
    void on_window_show(GtkWidget *w, gpointer user_data) {
      /* grabbing might not succeed immediately... */
    }
    g_signal_connect(G_OBJECT(mainwin), "show", G_CALLBACK(on_window_show), NULL);

    if (autodestroy_on_lost_focus) {
        g_signal_connect (G_OBJECT (mainwin), "focus-out-event", G_CALLBACK (mainwin_lostfocus), NULL);
                                   }
    if (add_timeout) { g_timeout_add(timeout_sec * 1000, (GSourceFunc) mainwin_lostfocus, NULL) ; }
    gtk_container_add ((GtkContainer*) mainwin, vbox);
    
    if ( strlen(message) > 0 ) { 
        GtkWidget *messagearea ;
        GtkTextBuffer *buffer;
        messagearea = (GtkWidget*) gtk_text_view_new ();
        gtk_text_view_set_editable(messagearea, False);
        gtk_text_view_set_cursor_visible(messagearea, False);
        set_my_css_provider(messagearea);
        add_my_css_class(messagearea, "top_message_area");
        gtk_text_view_set_justification (GTK_TEXT_VIEW (messagearea), GTK_JUSTIFY_CENTER) ;
        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (messagearea));
        gchar *trailing_newline ;
        trailing_newline = g_strrstr(message,g_strconcat("\n",NULL));
        if ( trailing_newline != NULL ) { *trailing_newline = ' '  ; } 
        gtk_text_buffer_set_text (buffer, message, -1);
        gtk_text_view_set_editable (GTK_TEXT_VIEW (messagearea), FALSE);
        gtk_container_add((GtkContainer*) message_frame, messagearea);
        gtk_box_pack_start (GTK_BOX(vbox), message_frame, FALSE, TRUE, 0); 
        } 

    g_free(message); message = NULL ;

    void itembut_click(gpointer x) { 
        int j;
        for (j=0; j < number_of_buttons; j++) {
            if ((GtkWidget*) x == itembut[j]) { 
              printf("\n");
              printf("*** mouse is disabled for now ***\n");
              printf("to go to  window (%s)  press key %s\n", lttr[j], lttr[j]); 
              printf("*********************************\n");
            j=number_of_buttons; }}}

    /* Creating buttons: */
    gchar btn_prefix[5] = "wbtn_";
    int j ;
    for (j = 0; j < number_of_buttons; j++) {
            itembut[j] = (GtkWidget*) gtk_button_new_with_label (title_of_button[j]); 
            gtk_button_set_relief(GTK_BUTTON(itembut[j]),GTK_RELIEF_NONE);
            g_signal_connect ( G_OBJECT (itembut[j]), "clicked",G_CALLBACK (itembut_click), 
            (gpointer)itembut[j] );
            itemlabel[j] =gtk_bin_get_child(GTK_BIN(itembut[j]));
            gtk_label_set_use_markup((gpointer)itemlabel[j], boldface);
            set_my_css_provider(itembut[j]);
            add_my_css_class(itembut[j], g_strconcat(btn_prefix, name_of_style[j], NULL));
            add_my_css_class(itembut[j], "wmjump_button");
            gtk_box_pack_start (GTK_BOX(vbox), itembut[j], FALSE, TRUE, 0); }
    if (number_of_buttons == 0) {
            GtkWidget *empty_label = gtk_label_new("*** NO WINDOWS ***");
            gtk_box_pack_start (GTK_BOX(vbox), empty_label, FALSE, TRUE, 0); }

    void on_mainwin_key_press_event (   GtkWidget    *widget,
                                        GdkEventKey  *event,
                                        gpointer     user_data ) {
        int num, j;
        num=-1;
        int ascii_code = event->keyval;
        if (ascii_code == 32) {
            gtk_main_quit();
            num=1000;
            send_command_to_go_back();
        }
        else {
        for (j=0; j < 9; j++) {
            if (ascii_code == (49+j) ) {
                gtk_main_quit();
                send_command_to_switch_desktop(j);
                num=j; j=100; }}
        for (j=0; j < number_of_buttons; j++) { 
            if (ascii_code == (97+j) ) { 
                gtk_main_quit();
                send_command_to_activate_window(j);
                if (double_clutch) { nanosleep(&wait_time_long,NULL); }
                num=j; j=number_of_buttons;
                }}
             }
        if (num == -1) { send_command_to_do_nothing(); gtk_main_quit();}
    }   
 
    
    g_signal_connect (G_OBJECT (mainwin), "key_press_event",
                      G_CALLBACK (on_mainwin_key_press_event),
                      NULL);

    gtk_widget_show_all (mainwin);
    gtk_main ();

}


static gchar *truncate_title(gchar *x) {
    int len,split_l,split_r;
    len=0;
    split_l=4*(TRUNCATE_TO_LEN/8);
    split_r=4*(TRUNCATE_TO_LEN/8);
    if (x==NULL){ return "---" ;}
    else {
        gchar *x_ret = x;
        len = g_utf8_strlen(x,-1);
        if (len>TRUNCATE_TO_LEN)
            {
            gchar y[4*(split_l+1)], z[4*(split_r+1)];
            g_utf8_strncpy (y,x,split_l);
            g_utf8_strncpy (z,g_utf8_strreverse(x,-1),split_r);
            x_ret = g_strconcat(y,"(...)",g_utf8_strreverse(z,-1),NULL); 
            }
        gchar *xsafe[3];
        if (boldface) { xsafe[2] = g_strjoinv("&amp;", g_strsplit(x_ret   , "&", -1));
                        xsafe[1] = g_strjoinv("&lt;" , g_strsplit(xsafe[2], "<", -1)); 
                        xsafe[0] = g_strjoinv("&gt;" , g_strsplit(xsafe[1], ">", -1));
                      }
        else {xsafe[0] = x_ret;}
        return xsafe[0];
    }
}
    


