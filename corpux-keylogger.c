/*

  corpux-keylogger - Keylogger for the corpux corpus generator

  This program is a derivative work from "xkeylogger - Simple keylogger for X",
  which itself is licensed under GPL 3 or more and Copyright (C) 2012 Andrea 
  Cardaci (who is NOT the author of corpux-keylogger, so don't bother him with
  issues about it).  Therefore, this program is also licensed under GPL 3 or more.

  Modifications with regards to the original xkeylogger code are licensed 
  under CC0 (i.e. public domain like).

  For more details, see the LICENSE file which should be included in this package.

  xkeylogger (and thus corpux-keylogger) is free software: you can redistribute 
  it and/or modify it under the terms of the GNU General Public License as 
  published by the Free Software Foundation, either version 3 of the License, 
  or (at your option) any later version.

  You should have received a copy of the GNU General Public License along with
  corpux-keylogger.  If not, see <http://www.gnu.org/licenses/>.

*/


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XInput.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>

#define XK_MISCELLANY
#include <X11/keysymdef.h>


// From Unicode Control Pictures, U2400-243F
#define VISUAL_BACKSPC 	"\u2408" 	// SYMBOL FOR BACKSPACE
#define VISUAL_INSERT 	"\u241a"	// SYMBOL FOR SUBSTITUTE
#define VISUAL_ESCAPE 	"\u241b"	// SYMBOL FOR ESCAPE
#define VISUAL_DELETE 	"\u2421" 	// SYMBOL FOR DELETE
#define VISUAL_SPACE 	"\u2423" 	// SYMBOL FOR SPACE — open box graphic alternative for \u2420
#define VISUAL_ENTER 	"\u2424"	// SYMBOL FOR NEWLINE

// From Unicode Arrows, U2190-21FF
#define VISUAL_LEFT 	"\u2190"
#define VISUAL_UP 	"\u2191"
#define VISUAL_RIGHT 	"\u2192"
#define VISUAL_DOWN 	"\u2193"
#define VISUAL_PUP 	"\u21de" 
#define VISUAL_PDOWN 	"\u21df"
#define VISUAL_TAB 	"\u21e5"
#define VISUAL_HOME 	"\u21f1"
#define VISUAL_END 	"\u21f2"

#define PROCNAMEBUF 100
#define LOGDIRBUF 100


void usage( const char *pgname )
{
    printf( "Usage : %s [-o outdir]\n" , pgname );
    printf( "\toutdir : directory where to store logfiles (default : \"logs\").\n\n" );
}

static int volatile keepRunning = 1;

void interrupted()
{
    keepRunning = 0;
}

struct keystroke_info
{
    time_t dt;
    KeySym original_keysym;
    unsigned int modifier_mask;
    int translation_available;
    KeySym translated_keysym;
    char translated_char[4];
    Window *focused_window;
};

const char *process_event( const struct keystroke_info *info )
{
    const char *out = malloc( sizeof( char ) );

    /* overload some special keystrokes */
    switch ( info->original_keysym )
    {
    case XK_Return:
    case XK_KP_Enter:
        out = VISUAL_ENTER;
        break;

    case XK_BackSpace:
        out = VISUAL_BACKSPC;
        break;

    case XK_Delete:
    case XK_KP_Delete:
        out = VISUAL_DELETE;
        break;

    case XK_Tab:
    case XK_KP_Tab:
        out = VISUAL_TAB;
        break;

    case XK_Escape:
        out = VISUAL_ESCAPE;
        break;

    case 0x0020: // space
    case XK_KP_Space:
        out = VISUAL_SPACE;
        break;

    case XK_Home:
    case XK_KP_Home:
        out = VISUAL_HOME;
        break;

    case XK_Page_Up:
    case XK_KP_Page_Up:
        out = VISUAL_PUP;
        break;

    case XK_Page_Down:
    case XK_KP_Page_Down:
        out = VISUAL_PDOWN;
        break;

    case XK_End:
    case XK_KP_End:
        out = VISUAL_END;
        break;

    case XK_Insert:
    case XK_KP_Insert:
        out = VISUAL_INSERT;
        break;

    case XK_Left:
    case XK_KP_Left:
        out = VISUAL_LEFT;
        break;

    case XK_Up:
    case XK_KP_Up:
        out = VISUAL_UP;
        break;

    case XK_Right:
    case XK_KP_Right:
        out = VISUAL_RIGHT;
        break;

    case XK_Down:
    case XK_KP_Down:
        out = VISUAL_DOWN;
        break;

    default:
        /* use default translation skipping control characters that are not
           associated to graphocal glyphs */
        if ( info->translation_available &&
             !( info->modifier_mask & ControlMask ) &&
             !( info->modifier_mask & Mod1Mask ) &&
             !( info->modifier_mask & Mod4Mask ) )
            out = info->translated_char;
        /* skip */
        else
            return NULL;
    }

    return out;

}

static void register_events( Display *display , Window root )
{
    int i , n;
    XDeviceInfo *devices;

    /* get all input devices */
    devices = XListInputDevices( display , &n );

    for ( i = 0 ; i < n ; i++ )
    {
        /* register events for each slave keyboard device since it's hard to
           know the device id of the real one */
        if ( devices[i].use == IsXExtensionKeyboard )
        {
            XDevice *device;
            int KEY_PRESS_TYPE;
            XEventClass event_class;

            /* open device and register the event */
            device = XOpenDevice( display , devices[i].id );
            DeviceKeyPress( device , KEY_PRESS_TYPE , event_class );
            XSelectExtensionEvent( display , root , &event_class , 1 );
        }
    }

    XFreeDeviceList( devices );
}

static XIC get_input_context( Display *display )
{
    int i;
    XIM xim;
    XIMStyles *xim_styles;
    XIMStyle xim_style;
    XIC xic;

    /* open input method */
    assert( xim = XOpenIM( display , NULL , NULL , NULL ) );

    /* fetch styles  */
    assert( !XGetIMValues( xim , XNQueryInputStyle , &xim_styles , NULL ) );
    assert( xim_styles != NULL );

    /* search wanted style */
    for ( xim_style = 0 , i = 0 ; i < xim_styles->count_styles ; i++ )
    {
        if ( xim_styles->supported_styles[i] ==
             ( XIMPreeditNothing | XIMStatusNothing ) )
        {
            xim_style = xim_styles->supported_styles[i];
            break;
        }
    }
    assert( xim_style != 0 );

    /* create input context */
    assert( xic = XCreateIC( xim , XNInputStyle , xim_style , NULL ) );

    XFree( xim_styles );
    return xic;
}

int translate_device_key_event( XIC xic , XDeviceKeyEvent *event ,
                                KeySym *out_keysym , char *out_string )
{
    XKeyEvent key_event;
    Status status;
    int length;

    /* build associated key event */
    key_event.type = KeyPress;
    key_event.serial = event->serial;
    key_event.send_event= event->send_event;
    key_event.display = event->display;
    key_event.window = event->window;
    key_event.root = event->root;
    key_event.subwindow = event->subwindow;
    key_event.time = event->time;
    key_event.state = event->state;
    key_event.keycode = event->keycode;
    key_event.same_screen = event->same_screen;

    /* translate the keystroke */
#ifdef X_HAVE_UTF8_STRING
    length = Xutf8LookupString( xic , &key_event , out_string , 4 ,
                              out_keysym , &status );
#else
    length = XmbLookupString( xic , &key_event , out_string , 4 ,
                              out_keysym , &status );
#endif
    if ( status == XLookupBoth )
    {
        out_string[length] = '\0';
        return 1;
    }

    return 0;
}

int get_window_property( Display *display , Window window , const char *name , const char *type , void *data )
{
    Atom name_atom;
    Atom type_atom;
    Atom actual_type;
    int format , status;
    unsigned long n_items , after;

    /* get atoms */
    name_atom = XInternAtom( display , name , True );
    type_atom = XInternAtom( display , type , True );

    /* get window property */
    status = XGetWindowProperty( display , window , name_atom , False , 0xffff , False ,
                                 type_atom , &actual_type , &format ,
                                 &n_items , &after , ( unsigned char ** )data );

    return status == Success && type_atom == actual_type && n_items > 0;

}

int get_window_name( Display *display , Window window , char *name , unsigned long **PID )
{
    if ( window )
    {
        int ret = 0;

        ret = get_window_property( display , window , "_NET_WM_PID" , "CARDINAL" , PID );

        if (ret)
        {
            char procfilename[30];
            char *procname = malloc(PROCNAMEBUF);
            sprintf( procfilename , "/proc/%lu/comm" , **PID );
            FILE *procfile;
            procfile = fopen( procfilename , "r" );
            if ( !procfile )
                fprintf( stderr , "WTF ? No %s file !" , procfilename );
            else
            {
                fgets( procname , PROCNAMEBUF , procfile );
                if ( strlen(procname) > 0 )
                {
                    procname [ strlen(procname) - 1 ] = 0;
                    if ( strrchr( procname , '/' ) != NULL )
                        procname = strrchr( procname , '/' ) + sizeof(char);
                    strcpy( name, procname);
                }
                else
                    strcpy( name, "misc" );
            }
            fclose( procfile );

            return 1;
        }
    }

    /* e.g. no windows on the root */
    PID = NULL;
    strcpy( name, "misc" );
    return 1;
}

int get_current_window( Display *display , Window **window )
{
    Window root;

    root = DefaultRootWindow( display );
    return get_window_property( display , root , "_NET_ACTIVE_WINDOW" , "WINDOW" , window );
}

int main( int argc , char *argv[] )
{

    char *logdir = malloc( LOGDIRBUF );
    char *logname = malloc( LOGDIRBUF + PROCNAMEBUF );
    strcpy( logdir, "./logs/" ); // TODO : custom log directory


    switch ( argc )
    {
        case 0:
            fprintf( stderr , "WTF ?! argc = 0 ?\n" );
            exit( 1 );
            break;

        case 1: // default case
            break;

        case 2:
            if ( ! strncmp( argv[1] , "-h" , 2 ) || ! strncmp( argv[1] , "--help" , 6 ))
            {
                usage( argv[0] );
                exit( 0 );
            }
            else if ( ! strncmp( argv[1] , "-o" , 2) )
            {
                fprintf( stderr , "ERROR : option -o expects an argument.\n" );
                usage( argv[0] );
                exit( 1 );
            }
            else
            {
                fprintf( stderr , "ERROR : invalid argument (%s).\n" , argv[1] );
                usage( argv[0] );
                exit( 1 );
            }
            break;

        case 3:
            if ( ! strncmp( argv[1] , "-o" , 2) )
            {
                strcpy( logdir , argv[2] );
                printf( "Output directory : %s.\n" , logdir );
            }
            else
            {
                fprintf( stderr , "ERROR : invalid argument (%s).\n" , argv[1] );
                usage( argv[0] );
                exit( 1 );
            }
            break;

        default:
            fprintf( stderr , "ERROR : too many arguments.\n" );
            usage( argv[0] );
            exit( 1 );
    }

    Display *display;
    int screen;
    Window root;
    XIC xic;

    /* open display */
    display = XOpenDisplay( NULL );
    if ( !display )
    {
        fprintf( stderr , "Cannot open display" );
        return EXIT_FAILURE;
    }

    /* create logfiles */

    if ( mkdir( logdir , S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH ) == -1 && errno != EEXIST )
    {
        fprintf( stderr , "ERROR : Cannot create %s directory — exiting." , logdir );
        exit(1);
    }


    /* get variables */
    screen = DefaultScreen( display );
    root = RootWindow( display , screen );

    /* get input context */
    xic = get_input_context( display );

    /* register events for every keyboard */
    register_events( display , root );

    sprintf( logname, "%s/%s", logdir, "misc" );

    FILE *logfile;
    logfile = fopen( logname , "a" );

    unsigned long oldPID = 0;
    unsigned long *PID = malloc( sizeof( unsigned long ) );

    signal( SIGINT , interrupted );
    signal( SIGTERM , interrupted );

    /* event loop */
    time_t tprev = 0;
    while ( keepRunning )
    {
        XEvent event;
        XDeviceKeyEvent *device_event;
        struct keystroke_info info;

        /* wait for the next event */
        XNextEvent( display , &event );

        /* fill keystroke info */
        device_event = ( XDeviceKeyEvent * )&event;
        info.dt = device_event->time - tprev;
        tprev = device_event->time;
        info.original_keysym =
            XkbKeycodeToKeysym( display , device_event->keycode , 0 , 0 );
        info.modifier_mask = device_event->state;

        /* get window */
        get_current_window( display , &info.focused_window );

        /* get process name */
        char *procname = malloc( PROCNAMEBUF );
        get_window_name( display , *info.focused_window , procname , &PID );
        if ( PID!= NULL && *PID != oldPID )
        {
            sprintf(logname, "%s/%s", logdir, procname);
            fclose(logfile);
            logfile = fopen(logname, "a");
        }

        free( procname );

        /* translate keystroke */
        info.translation_available =
            translate_device_key_event( xic ,
                                        device_event ,
                                        &info.translated_keysym ,
                                        info.translated_char );

        /* process the event */
        const char *out = process_event( &info );

        if ( out )
        {
                fprintf( logfile , "%d %s\n" , (int)info.dt , out );
                fflush( logfile );
        }

        /* cleanup */ 
        XFree( info.focused_window );

    }

    fclose( logfile );
    free( logdir );
    free( logname );
    free( PID );
    XCloseDisplay( display );
}

