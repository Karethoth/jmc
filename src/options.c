#include <stdio.h>
#include <string.h> // strlen strcpy strcat strtol strcmp strdup

#include <mpd/client.h> // mpd_*
#include <SDL/SDL.h> // SDL_*

// resolve conflicting boolean definitions
#undef bool
typedef int bool;
#define true 1
#define false 0

#include "options.h"
#include "gfx.h" // clear_gfx
#include "sdl.h" // finish_sdl

#define ALL_OPT '\00'
#define FULL_OPT '\01'

// general options
char *music_directory = NULL;
int bufsize = 10;
bool verbose = false;
bool local = true;
// SDL options
int width = 1366;
int height = 768;
int screen_bpp = 32;
int screen_flags = SDL_HWSURFACE | SDL_RESIZABLE | SDL_DOUBLEBUF;
SDL_Surface *screen = NULL;
// mpd options
struct mpd_connection *client = NULL;
// gfx.c geometry options
int max_size_opt = 300;
long int bg_color = 0;
double root_size_opt = 0.6;
double size_opt = 0.8;
double pad_opt = 0.04;
double y_off_opt = 0.08;
Uint32 trans;
// globals
char *prog = NULL;
char *version = "0.0.1";
int max_path_length = 10000;

// internal funtion prototypes
bool is_int_string_opt (char *, int *);
bool is_hex_string_opt (char *, long int *);
bool is_float_string_opt (char *, double *);
void print_usage_opt (char);
int get_opt (char *);
int start_sdl (int, int, double);
int set_music_directory (void);
void start_client (char *, unsigned, unsigned);

// add options here
static const char* (option_usage [] [4]) =
{
    //  name    long name       usage
    //  description
    {
        "M",    "host",         "'mpd host'",
        "\tSet the mpd host to connect to. If unset it uses the environment\n"
        "\tvariable MPD_HOST, or the mpd default."
    },
    {
        "P",    "port",         "[mpd port]",
        "\tSet the mpd port to connect with. If unset it uses the\n"
        "\tenvironment variable MPD_PORT, or the mpd default."
    },
    {
        "T",    "timeout",      "[mpd timeout]",
        "\tSet the timeout before the mpd connection expires. If unset it\n"
        "\tuses the environment variable MPD_TIMEOUT, or the mpd default."
    },
    /*
    {
        "R",    "regex",        "{expression}",
        "\tSet a regex expression to filter out unwanted albums. If unset it\n"
        "\tdefaults to empty."
    },
    */
    {
        "b",    "buffer",       "[covers]",
        "\tSet the number of images to load into memory at once. If it is\n"
        "\tsmaller than the number of albums shown on the screen, there will\n"
        "\tbe blank spaces. If it is too large it may take some time to\n"
        "\tcatalog albums, or the program may run out of memory. If unset it\n"
        "\tdefaults to 10. Must be positive or 0."
    },
    {
        "m",    "max",          "[size]",
        "\tSet the maximum size of albums. If unset it defaults to 300. Must\n"
        "\tbe positive."
    },
    {
        "w",    "window",       "[width] [height]",
        "\tSet the size of the window and disallow resizing. If unset it\n"
        "\tdefaults to 800x600 and resizable. Must be positive."
    },
    {
        "d",    "dimensions",   "[width] [height]",
        "\tSet the fullscreen size. It should be set to your exact monitor\n"
        "\tresolution. If unset it defaults to 1366x768. Must be positive."
    },
    {
        "c",    "color",        "[hexidecimal color]",
        "\tSet the background color of the window. If unset it defaults to\n"
        "\tblack (000000)."
    },
    {
        "a",    "album",        "[percent]",
        "\tSet the size of general albums using a decimal percentage of\n"
        "\tcurrently playing album size. If unset it defaults to 0.8. Must\n"
        "\tbe positive or 0."
    },
    {
        "A",    "cur-album",    "[percent]",
        "\tSet the size of the currently playing album using a decimal\n"
        "\tpercentage of screen height. If unset it defaults to 0.6. Must be\n"
        "\tpositive or 0."
    },
    {
        "p",    "padding",      "[percent]",
        "\tSet the size of the padding between album using a decimal\n"
        "\tpercentage of general album size. If unset it defaults to 0.04."
    },
    {
        "t",    "transparency", "[percent]",
        "\tSet the visibility of the reflection using a decimal percentage.\n"
        "\tIf unset it defaults to 0.1. Must be between 0 and 1."
    },
    {
        "y",    "y-off",        "[percent]",
        "\tSet the amount to bump up the currently-playing-album using a\n"
        "\tdecimal percentage of general album size. If unset it defaults to\n"
        "\t0.08."
    },
    {
        "D",    "directory",    "/path/to/music",
        "\tSet the folder that the music database is contained in. If unset,\n"
        "\tit assumes that mpd is configured for a local socket and asks the\n"
        "\tserver for the name of the directory."
    },
    {
        "V",    "verbose",      "(no parameters)",
        "\tPrint excess output during usage. If unset it defaults to off."
    },
    {
        "h",    "help",         "(no parameters)",
        "\tPrint the usage of all options and exit."
    },
    {
        "H",    "help-all",     "(no parameters)",
        "\tPrint the usage of all options with description and exit."
    },
    {
        "v",    "version",      "(no parameters)",
        "\tPrint the version of the program and exit."
    },
    { "\0" }
};

// external functions

// add option behaviour here
void parse_opt (char **argv)
{
    char *temp = NULL;
    // default values for mpd/startclient
    char *host = NULL;
    unsigned port = 0;
    unsigned timeout = 0;
    int w = 800, h = 600;
    double reflect = 0.1;

    // set program name
    prog = *(argv++);

    for ( ; *argv != NULL; argv++)
    {
        char argchar = get_opt (*argv);

        if (argchar == '\0')
            fprintf (stderr, "%s: unrecognized option %s\n", prog, *argv);

        else if (argchar == 'M')
        {
            temp = *(++argv);
            if (temp == NULL || temp [0] == '-')
            {
                print_usage_opt ('H');
                --argv;
            }
            else
                host = temp;
        }

        else if (argchar == 'P')
        {
            temp  = *(++argv);
            int i = 0;
            if (temp == NULL || ! is_int_string_opt (temp, &i) || i < 0)
            {
                print_usage_opt ('P');
                --argv;
            }
            else
                port = i;

        }

        else if (argchar == 'T')
        {
            temp = *(++argv);
            int i = 0;
            if (temp == NULL || ! is_int_string_opt (temp, &i) || i < 0)
            {
                print_usage_opt ('T');
                --argv;
            }
            else
                timeout = i;
        }

        else if (argchar == 'b')
        {
            int i;
            temp = *(++argv);
            if (temp == NULL || ! is_int_string_opt (temp, &i) || i < 0)
            {
                print_usage_opt ('b');
                --argv;
            }
            else
                bufsize = i;
        }

        else if (argchar == 'm')
        {
            int i;
            temp = *(++argv);
            if (temp == NULL || ! is_int_string_opt (temp, &i) || i <= 0)
            {
                print_usage_opt ('m');
                --argv;
            }
            else
                max_size_opt = i;
        }

        else if (argchar == 'w')
        {
            int i, j;
            temp = *(++argv);
            if (temp == NULL || ! is_int_string_opt (temp, &i) || i <= 0)
            {
                print_usage_opt ('w');
                --argv;
                continue;
            }
            temp = *(++argv);
            if (temp == NULL || ! is_int_string_opt (temp, &j) || j <= 0)
            {
                print_usage_opt ('w');
                --argv;
            }
            else
            {
                screen_flags &= ~SDL_RESIZABLE;
                w = i;
                h = j;
            }
        }

        else if (argchar == 'd')
        {
            int i, j;
            temp = *(++argv);
            if (temp == NULL || ! is_int_string_opt (temp, &i))
            {
                print_usage_opt ('d');
                --argv;
                continue;
            }
            temp = *(++argv);
            if (temp == NULL || ! is_int_string_opt (temp, &j) || j <= 0)
            {
                print_usage_opt ('s');
                --argv;
            }
            else
            {
                width = i;
                height = j;
            }
        }

        else if (argchar == 'c')
        {
            temp = *(++argv);
            if (! is_hex_string_opt (temp, &bg_color))
            {
                print_usage_opt ('c');
                --argv;
            }
        }

        else if (argchar == 'a')
        {
            temp = *(++argv);
            double d;
            if (temp == NULL || ! is_float_string_opt (temp, &d) || d < 0)
            {
                print_usage_opt ('a');
                --argv;
            }
            else
                size_opt = d;
        }

        else if (argchar == 'A')
        {
            temp = *(++argv);
            double d;
            if (temp == NULL || ! is_float_string_opt (temp, &d) || d < 0)
            {
                print_usage_opt ('A');
                --argv;
            }
            else
                root_size_opt = d;
        }

        else if (argchar == 'p')
        {
            temp = *(++argv);
            if (temp == NULL || ! is_float_string_opt (temp, &pad_opt))
            {
                print_usage_opt ('p');
                --argv;
            }
        }

        else if (argchar == 'y')
        {
            temp = *(++argv);
            if (temp == NULL || ! is_float_string_opt (temp, &y_off_opt))
            {
                print_usage_opt ('y');
                --argv;
            }
        }

        else if (argchar == 'V')
            verbose = true;

        else if (argchar == 't')
        {
            temp = *(++argv);
            double d;
            if (! is_float_string_opt (temp, &d) || d > 1 || d < 0)
            {
                print_usage_opt ('t');
                --argv;
            }
            else
                reflect = d;
        }
        
        else if (argchar == 'D')
        {
            music_directory = *(++argv);
            if (music_directory == NULL || music_directory [0] == '-')
            {
                print_usage_opt ('D');
                --argv;
                music_directory = NULL;
            }
        }

        else if (argchar == 'h')
        {
            print_usage_opt (ALL_OPT);
            exit (0);
        }

        else if (argchar == 'H')
        {
            print_usage_opt (FULL_OPT);
            exit (0);
        }

        else if (argchar == 'v')
        {
            printf ("jmc %s\n", version);
            exit (0);
        }

        else
            fprintf (stderr, "%s: unrecognized option %s\n", prog, *argv);
    }

    start_client (host, port, timeout);
    // post-arg mpd settings
    if (music_directory == NULL)
        set_music_directory ();

    start_sdl (w, h, reflect);
}

// internal functions

bool is_int_string_opt (char *s, int *i)
{
    char *t = s;
    if (*s == '-')
        s++;
    while (*s != '\0' && isdigit (*s))
        s++;
    if (*s == '\0')
    {
        *i = atoi (t);
        return true;
    }
    return false;
}

bool is_hex_string_opt (char *s, long int *l)
{
    if (strlen (s) != 6)
        return false;
    char *t = s;
    char c;
    while (*s != '\0' &&
            (isdigit (*s) || ((c = toupper (*s)) <= 'F' && c >= 'A')))
        s++;
    if (*s == '\0')
    {
        *l = strtol ((const char *) t, NULL, 16);
        return true;
    }
    return false;
}

bool is_float_string_opt (char *s, double *d)
{
    bool decimal = false;
    char *t = s;
    if (*s == '-')
        s++;
    while (*s != '\0' && (isdigit (*s) || *s == '.'))
    {
        if (*s == '.')
        {
            if (decimal)
                break;
            else
                decimal = true;
        }
        s++;
    }
    if (*s == '\0')
    {
        *d = atof ((const char *) t);
        return true;
    }
    return false;
}

void print_usage_opt (char o)
{
    printf ("usage:\n");
    char c;

    for (int i = 0; (c = option_usage [i] [0] [0]) != '\0'; i++)
        if (c == o || o == ALL_OPT || o == FULL_OPT)
        {
            printf ("-%-4c --%-16s%s\n", c, option_usage [i] [1],
                    option_usage [i] [2]);
            if (o == FULL_OPT)
                printf ("%s\n", option_usage [i] [3]);
            else if (o != ALL_OPT)
                break;
        }
}

int get_opt (char *s)
{
    int len = strlen (s);

    if (len < 2)
        return '\0';

    if (s [0] != '-')
        return '\0';

    if (len == 2)
        return s [1];

    if (s [1] != '-')
        return '\0';

    s += 2;
    int c;
    for (int i = 0; (c = option_usage [i] [0] [0]) != '\0'; i++)
    {
        if (strcmp (s, option_usage [i] [1]) == 0)
            break;
    }
    return c;
}

int start_sdl (int w, int h, double reflect)
{
    if (SDL_Init (SDL_INIT_EVERYTHING) == -1)
    {
        fprintf (stderr, "%s: %s\n", prog, SDL_GetError ());
        return -1;
    }

    screen = SDL_SetVideoMode (w, h, screen_bpp, screen_flags);
    // fix automatically set tiled dimensions
    const SDL_VideoInfo *info = SDL_GetVideoInfo ();
    screen = SDL_SetVideoMode (info->current_w, info->current_h,
                               screen_bpp, screen_flags);
    if (screen == NULL)
    {
        fprintf (stderr, "%s: %s\n", prog, SDL_GetError ());
        SDL_Quit ();
        return -1;
    }

    SDL_WM_SetCaption (prog, NULL);

    clear_gfx (screen);

    if (SDL_Flip (screen) == -1)
    {
        fprintf (stderr, "%s: %s\n", prog, SDL_GetError ());
        finish_sdl ();
        return -1;
    }

    trans = 0xFF * (1 - reflect);
    trans = (trans << 16) | (trans << 8) | trans;

    return 0;
}

void start_client (char *host, unsigned port, unsigned timeout)
{
    // default values found in options.c
    client = mpd_connection_new (host, port, timeout);

    if (mpd_connection_get_error (client) != MPD_ERROR_SUCCESS) 
    {
        fprintf (stderr, "%s: %s\n", prog,
                mpd_connection_get_error_message (client));
        exit (-1);
    }
}

int set_music_directory (void)
{
    if (mpd_connection_cmp_server_version (client, 0, 17, 0) < 0)
    {
        fprintf (stderr, "%s: please update mpd to at least 0.17.1\n", prog);
        return 1;
    }

    if (! mpd_send_command (client, "config", NULL))
    {
        fprintf (stderr, "%s: %s\n", prog,
                mpd_connection_get_error_message (client));
        return 1;
    }

    struct mpd_pair *pair =
        mpd_recv_pair_named (client, "music_directory");

    if (pair != NULL)
    {
        int length;
        music_directory = strdup (pair->value);
        char *temp = realloc
            (music_directory, sizeof (char) * (strlen (music_directory) + 2));
        if (temp != NULL)
            music_directory = temp;
        mpd_return_pair (client, pair);

        if (music_directory [(length = strlen (music_directory))] != '/')
        {
            strcat (music_directory, "/");
        }
    }
    else
    {
        (void) mpd_connection_get_error (client);
        fprintf (stderr, "%s: %s\n", prog,
                mpd_connection_get_error_message (client));
        return 1;
    }

    if (!  (mpd_response_finish (client)
        || mpd_connection_clear_error (client)))
    {
        fprintf (stderr, "%s: %s\n", prog,
                mpd_connection_get_error_message (client));
        return 1;
    }

    return 0;
}
