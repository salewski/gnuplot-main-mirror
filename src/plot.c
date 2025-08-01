/* GNUPLOT - plot.c */

/*[
 * Copyright 1986 - 1993, 1998, 2004   Thomas Williams, Colin Kelley
 *
 * Permission to use, copy, and distribute this software and its
 * documentation for any purpose with or without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.
 *
 * Permission to modify the software is granted, but not the right to
 * distribute the complete modified source code.  Modifications are to
 * be distributed as patches to the released version.  Permission to
 * distribute binaries produced by compiling modified sources is granted,
 * provided you
 *   1. distribute the corresponding source modifications from the
 *    released version in the form of a patch file along with the binaries,
 *   2. add special version identification to distinguish your version
 *    in addition to the base release version number,
 *   3. provide your name and address as the primary contact for the
 *    support of your modified version, and
 *   4. retain our contact information in regard to use of the base
 *    software.
 * Permission to distribute the released version of the source code along
 * with corresponding source modifications in the form of a patch file is
 * granted with same provisions 2 through 4 for binary distributions.
 *
 * This software is provided "as is" without express or implied warranty
 * to the extent permitted by applicable law.
]*/

#include "syscfg.h"
#include "plot.h"

#include "alloc.h"
#include "command.h"
#include "eval.h"
#include "fit.h"
#include "gp_hist.h"
#include "gplocale.h"
#include "loadpath.h"
#include "misc.h"
#include "readline.h"
#include "setshow.h"
#include "term_api.h"
#include "util.h"
#include "version.h"
#include "voxelgrid.h"
#include "encoding.h"
#include "save.h"
#include "xdg.h"

#include <signal.h>
#include <setjmp.h>

#ifdef OS2
# define INCL_REXXSAA
# include <os2.h>
#endif /* OS2 */

#if defined(MSDOS) || defined(__EMX__) || (defined(WGP_CONSOLE) && defined(MSVC))
# include <io.h>
#endif

#ifdef _WIN32
# include <windows.h>
# include "win/winmain.h"
# include "win/wcommon.h"
# include <io.h>           /* for isatty */
#endif /* _WIN32 */

/* GNU readline
 * Only required by two files directly,
 * so I don't put this into a header file. -lh
 */
#if defined(HAVE_LIBREADLINE) && !defined(MISSING_RL_TILDE_EXPANSION)
#  include <readline/tilde.h>
   extern int rl_complete_with_tilde_expansion;
#endif

/* BSD editline
*/
#ifdef HAVE_LIBEDITLINE
#  include <editline/readline.h>
#  include <histedit.h>
#endif

/* enable gnuplot history with readline */
#ifdef GNUPLOT_HISTORY
#  ifndef GNUPLOT_HISTORY_FILE
#    define GNUPLOT_HISTORY_FILE "~/.gnuplot_history"
#  endif
/*
 * expanded_history_filename points to the value from 'tilde_expand()',
 * which expands '~' to the user's home directory, or $HOME.
 * Depending on your OS you have to make sure that the "$HOME" environment
 * variable exists.  You are responsible for valid values.
 */
static char *expanded_history_filename;

static void wrapper_for_write_history(void);

#endif				/* GNUPLOT_HISTORY */

TBOOLEAN interactive = TRUE;	/* FALSE if stdin not a terminal */
TBOOLEAN noinputfiles = TRUE;	/* FALSE if there are script files */
TBOOLEAN reading_from_dash=FALSE;	/* True if processing "-" as an input file */
TBOOLEAN skip_gnuplotrc = FALSE;	/* skip system gnuplotrc and ~/.gnuplot */
TBOOLEAN persist_cl = FALSE; 		/* --persist command line option */
TBOOLEAN slow_font_startup = FALSE;	/* --slow command line option */

/* user home directory */
static const char *user_homedir = NULL;

/* user shell */
const char *user_shell = NULL;

/* not static because unset.c refers to it when in debugging mode */
TBOOLEAN successful_initialization = FALSE;

#ifdef X11
extern int X11_args(int, char **); /* FIXME: defined in term/x11.trm */
#endif

/* patch to get home dir, see command.c */
#if defined(MSDOS) || defined(OS2)
char HelpFile[PATH_MAX];
static char progpath[PATH_MAX] = "";
#endif

/* a longjmp buffer to get back to the command line */
static JMP_BUF command_line_env;

static void load_rcfile(int where);
static RETSIGTYPE inter(int anint);
static void init_memory(void);

static int exit_status = EXIT_SUCCESS;

/* Flag for asynchronous handling of Ctrl-C. Used by fit.c and Windows */
TBOOLEAN ctrlc_flag = FALSE;
/* Flag for (asynchronous) term signal on Windows. */
TBOOLEAN terminate_flag = FALSE;

#ifdef OS2
# include <process.h>
static ULONG RexxInterface(PRXSTRING, PUSHORT, PRXSTRING);
TBOOLEAN CallFromRexx = FALSE;
#endif /* OS2 */

static RETSIGTYPE
inter(int anint)
{
    (void) anint;		/* avoid -Wunused warning */
    (void) signal(SIGINT, (sigfunc) inter);
    (void) signal(SIGFPE, SIG_DFL);	/* turn off FPE trapping */

#ifdef OS2
    if (!strcmp(term->name,"pm")) {
	PM_intc_cleanup();
	/* ??
	  putc('\n', stderr);
	  LONGJMP(command_line_env, TRUE);
	 */
    } else
#endif
#if defined(WGP_CONSOLE)
	/* The Windows console Ctrl-C handler runs in another thread. So a
	   longjmp() would result in crash. Instead, we handle these
	   events asynchronously.
	*/
	ctrlc_flag = TRUE;
	/* Interrupt ConsoleGetch. */
	SendMessage(graphwin->hWndGraph, WM_NULL, 0, 0);
	SendMessage(GetConsoleWindow(), WM_CHAR, 0x20, 0);
#else
    {
    term_reset();
    (void) putc('\n', stderr);
    bail_to_command_line();	/* return to prompt */
    }
#endif
}

/* Normally we return to the command line at a point equivalent
 * to program entry, so most context is lost.  All loops and
 * loaded command files are exited; open files are closed, etc.
 *
 * The exception to this is when an error is encountered during a
 * "fit" command.  In this case the program returns to the same
 * point it would have reached if the fit command had succeeded,
 * possibly inside a loop or a loaded command file.
 */
void
bail_to_command_line()
{
#ifdef _WIN32
    kill_pending_Pause_dialog();
    ctrlc_flag = FALSE;
#endif
    if (fit_env)
	LONGJMP(*fit_env, TRUE);
#if defined (WXWIDGETS) && defined (HAVE_GTK)
    else if (wxt_event_processing) {
	extern JMP_BUF *wxt_env;
	LONGJMP(*wxt_env, TRUE);
    }
#endif
    else
	LONGJMP(command_line_env, TRUE);
}

#if defined(_WIN32)
int
gnu_main(int argc_orig, char **argv)
#else
int
main(int argc_orig, char **argv)
#endif
{
    int i;

    /* We want the current value of argc to persist across a LONGJMP from int_error().
     * Without this the compiler may put it on the stack, which LONGJMP clobbers.
     * Here we try make it a volatile variable that optimization will not affect.
     * Why do we not have to do the same for argv?   I don't know.
     * But the test cases that broke with generic argc seem fine with generic argv.
     */
    static volatile int argc;
    argc = argc_orig;

/* make sure that we really have revoked root access, this might happen if
   gnuplot is compiled without vga support but is installed suid by mistake */
#ifdef __linux__
    if (setuid(getuid()) != 0) {
	fprintf(stderr,"gnuplot: refusing to run at elevated privilege\n");
	exit(EXIT_FAILURE);
    }
#endif

/* HBB: Seems this isn't needed any more for DJGPP V2? */
/* HBB: disable all floating point exceptions, just keep running... */
#if defined(DJGPP) && (DJGPP!=2)
    _control87(MCW_EM, MCW_EM);
#endif

#ifdef OS2
    {
	int rc = RexxRegisterSubcomExe("GNUPLOT", (PFN) RexxInterface, NULL);
	(void) rc;
# ifdef OS2_IPC
	os2_ipc_setup();
# endif
    }
#endif

/* init progpath and get helpfile from executable directory */
#if defined(MSDOS) || defined(OS2)
    {
	char *s;

#ifdef __EMX__
	_execname(progpath, sizeof(progpath));
#else
	safe_strncpy(progpath, argv[0], sizeof(progpath));
#endif
	/* convert '/' to '\\' */
	for (s = progpath; *s != NUL; s++)
	    if (*s == DIRSEP2)
		*s = DIRSEP1;
	/* cut program name */
	s = strrchr(progpath, DIRSEP1);
	if (s != NULL)
	    s++;
	else
	    s = progpath;
	*s = NUL;
	/* init HelpFile */
	strcpy(HelpFile, progpath);
	strcat(HelpFile, "gnuplot.gih");
	/* remove trailing "bin/" from progpath */
	if ((s != NULL) && (s - progpath >= 4)) {
	    s -= 4;
	    if (strncasecmp(s, "bin", 3) == 0)
		*s = NUL;
	}
    }
#endif /* MSDOS || OS2 */

#if (defined(PIPE_IPC) || defined(_WIN32)) && (defined(HAVE_LIBREADLINE) || (defined(HAVE_LIBEDITLINE)))
    /* Editline needs this to be set before the very first call to readline(). */
    rl_getc_function = getc_wrapper;
#endif

#if defined(HAVE_LIBREADLINE)
    /* T.Walter 1999-06-24: 'rl_readline_name' must be this fix name.
     * It is used to parse a 'gnuplot' specific section in '~/.inputrc'
     * or gnuplot specific commands in '.editrc' (when using editline
     * instead of readline) */
    rl_readline_name = "Gnuplot";
    rl_terminal_name = getenv("TERM");
    using_history();

    #if !defined(MISSING_RL_TILDE_EXPANSION)
    rl_complete_with_tilde_expansion = 1;
    #endif

    #if defined(RL_VERSION_MAJOR)
    /* Starting with readline v8.1 bracketed paste mode is on by default.
     * This breaks multi-line pasted input to gnuplot because it looks like
     * one long run-on line.
     */
    if (RL_VERSION_MAJOR >= 8)
	rl_variable_bind ("enable-bracketed-paste", "off");
    #endif
#endif

#if defined(HAVE_LIBEDITLINE)
    rl_readline_name = "Gnuplot";
    rl_terminal_name = getenv("TERM");
    el = el_init("gnuplot", stdin, stdout, stderr);
    history_init();
#endif

    for (i = 1; i < argc; i++) {
	if (!argv[i])
	    continue;

	if (!strcmp(argv[i], "-c")) {
	    /* The rest of the command line is a scriptfile and its arguments.
	     * Do not try to interpret them here (ignore -V, --help, etc).
	     */
	    break;
	}

	if (!strcmp(argv[i], "-V") || !strcmp(argv[i], "--version")) {
	    /* DEVELOPMENT VERSION */
	    printf("gnuplot %s last modified %s\n",
		    gnuplot_version, gnuplot_date);
	    return 0;

	} else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
	    printf( "Usage: gnuplot [OPTION] ... [FILE]\n"
		    "  -V, --version\n"
		    "  -h, --help\n"
		    "  -p  --persist\n"
		    "  -s  --slow\n"
		    "  -d  --default-settings\n"
		    "  -c  scriptfile ARG1 ARG2 ... \n"
		    "  -e  \"command1; command2; ...\"\n"
		    "gnuplot %s patchlevel %s\n",
		    gnuplot_version, gnuplot_patchlevel);
#ifdef DEVELOPMENT_VERSION
	    printf(
#ifdef DIST_CONTACT
		    "Report bugs to "DIST_CONTACT"\n"
		    "            or %s\n",
#else
		    "Report bugs to %s\n",
#endif
		    bug_email);
#endif
	    return 0;

	} else if (!strncmp(argv[i], "-persist", 2) || !strcmp(argv[i], "--persist")
#ifdef _WIN32
		|| !stricmp(argv[i], "-noend") || !stricmp(argv[i], "/noend")
#endif
		) {
	    persist_cl = TRUE;
	} else if (!strncmp(argv[i], "-slow", 2) || !strcmp(argv[i], "--slow")) {
	    slow_font_startup = TRUE;
	} else if (!strncmp(argv[i], "-d", 2) || !strcmp(argv[i], "--default-settings")) {
	    /* Skip local customization read from ~/.gnuplot */
	    skip_gnuplotrc = TRUE;
	}
    }

    setbuf(stderr, (char *) NULL);

#ifdef HAVE_SETVBUF
    /* This was once setlinebuf(). Docs say this is
     * identical to setvbuf(,NULL,_IOLBF,0), but MS C
     * faults this (size out of range), so we try with
     * size of 1024 instead. [SAS/C does that, too. -lh]
     */
    if (setvbuf(stdout, (char *) NULL, _IOLBF, (size_t) 1024) != 0)
	(void) fputs("Could not linebuffer stdout\n", stderr);

    /* Switching to unbuffered mode causes all characters in the input
     * buffer to be lost. So the only safe time to do it is on program entry.
     * Do any non-X platforms suffer from this problem?
     * EAM - Jan 2013 YES.
     */
    setvbuf(stdin, (char *) NULL, _IONBF, 0);
#endif

    gpoutfile = stdout;

    /* Initialize pre-loaded user variables */
    /* "pi" is hard-wired as the first variable */
    (void) add_udv_by_name("GNUTERM");
    (void) add_udv_by_name("I");
    (void) add_udv_by_name("Inf");
    (void) add_udv_by_name("NaN");
    init_constants();
    /* user-defined variables start immediately after NaN */
    udv_user_head = &(udv_NaN->next_udv);

    init_memory();

    interactive = FALSE;

    /* April 2017:  We used to call init_terminal() here, but now   */
    /* We defer initialization until error handling has been set up. */

# if defined(_WIN32) && !defined(WGP_CONSOLE)
    interactive = TRUE;
# else
    interactive = isatty(fileno(stdin));
# endif

    /* Note: we want to know whether this is an interactive session so that we can
     * decide whether or not to write status information to stderr.  The old test
     * for this was to see if (argc > 1) but the addition of optional command line
     * switches broke this.  What we really wanted to know was whether any of the
     * command line arguments are file names or an explicit in-line "-e command".
     */
    for (i = 1; i < argc; i++) {
# ifdef _WIN32
	if (!stricmp(argv[i], "/noend"))
	    continue;
# endif
	if ((argv[i][0] != '-') || (argv[i][1] == 'e') || (argv[i][1] == 'c') ) {
	    interactive = FALSE;
	    break;
	}
    }

#ifdef VMS
    vms_init_screen();
#endif /* VMS */

    if (!SETJMP(command_line_env, 1)) {
	/* first time */
	interrupt_setup();
	get_user_env();
	init_loadpath();
	init_locale();

	memset(&sm_palette, 0, sizeof(sm_palette));
	init_fit();		/* Initialization of fitting module */
#ifdef READLINE
	/* When using the built-in readline, we set the initial
	   encoding according to the locale as this is required
	   to properly handle keyboard input. */
	init_encoding();
#endif
	init_gadgets();

	/* Now that error handling is in place, it is safe to parse GNUTERM
	 * during terminal initialization.  However if terminal initialization
	 * exits via int_error(), the session and history state initialization
	 * can only be reached via a GOTO from the error return.
	 */
	init_terminal();
	push_terminal(0);	/* remember the initial terminal */

	/* atexit processing is done in reverse order. We want
	 * the generic terminal shutdown in term_reset to be executed before
	 * any terminal specific cleanup requested by individual terminals.
	 */
	gp_atexit(term_reset);

#ifdef X11
	/* the X11 terminal removes tokens that it recognizes from argv. */
	if (term && !strcmp(term->name, "x11")) {
	    int n = X11_args(argc, argv);
	    argv += n;
	    argc -= n;
	}
#endif
	FALLBACK_ONCE_FROM_INIT_FAILURE:

	/* Version 6:  defer splash page until after initialization */
	if (interactive)
	    show_version(stderr);
	else
	    show_version(NULL); /* Only load GPVAL_COMPILE_OPTIONS */

	/* update GPVAL_ variables available to user */
	update_gpval_variables(3);

	/* Execute commands in ~/.gnuplot */
	init_session();

	if (interactive)
	   fprintf(stderr, "\n\tTerminal type is now %s\n", term->name);

	if (interactive && term != 0) {		/* not unknown */
#ifdef GNUPLOT_HISTORY
#if (defined(HAVE_LIBREADLINE) || defined(HAVE_LIBEDITLINE)) && !defined(_WIN32)
	    expanded_history_filename = tilde_expand(GNUPLOT_HISTORY_FILE);
#else
	    expanded_history_filename = gp_strdup(GNUPLOT_HISTORY_FILE);
	    gp_expand_tilde(&expanded_history_filename);
#endif
	    if (read_history(expanded_history_filename)) {
#ifdef USE_XDG_BASEDIR
		/* use $XDG_STATE_DIR/gnuplot_history only if
		 * $XDG_STATE_DIR exists or can be created */
		char *xdgpath = xdg_get_path(kXDGStateHome, "gnuplot_history",
				FALSE /* no subdir */, TRUE /* create */);
		if (xdgpath) {
		    free(expanded_history_filename);
		    expanded_history_filename = xdgpath;
		    read_history(expanded_history_filename);
		}
#endif /* USE_XDG_BASEDIR */
	    }

	    /*
	     * It is safe to ignore the return values of 'atexit()' and
	     * 'on_exit()'. In the worst case, there is no history of your
	     * current session and you have to type it all again in your next
	     * session.
	     */
	    gp_atexit(wrapper_for_write_history);
#endif /* GNUPLOT_HISTORY */

#if defined(READLINE) && defined(WGP_CONSOLE)
	    fprintf(stderr, "Encoding set to '%s'.\n", encoding_names[encoding]);
#endif
	}			/* if (interactive && term != 0) */
    } else {
	/* come back here from int_error() */
	if (!successful_initialization) {
	    /* Only warn and attempt a fall-back on the first failure.
	     * There is no guarantee that we will come out of this in a
	     * usable state, but the only truly safe alternative would be
	     * to give up and exit the program.
	     */
	    successful_initialization = TRUE;
	    fprintf(stderr, "WARNING: Error during initialization\n");
	    fprintf(stderr, "         Check initialization files and environment variables (e.g. GNUTERM)\n");
	    change_term("unknown", 7);
	    goto FALLBACK_ONCE_FROM_INIT_FAILURE;
	}
	if (interactive == FALSE)
	    exit_status = EXIT_FAILURE;
#ifdef HAVE_READLINE_RESET
	else {
	    /* reset properly readline after a SIGINT+longjmp */
	    rl_reset_after_signal ();
	}
#endif

	/* If we were in a "load", "call", or nested bracketed clause 
	 * clean up the lf_push/pop stack and restore shadowed variables.
	 */
	clause_reset_after_error();
	lf_reset_after_error();

	/* We are certainly no longer in a plot command */
	inside_plot_command = FALSE;

	SET_CURSOR_ARROW;

#ifdef VMS
	/* after catching interrupt */
	/* VAX stuffs up stdout on SIGINT while writing to stdout,
	   so reopen stdout. */
	if (gpoutfile == stdout) {
	    if ((stdout = freopen("SYS$OUTPUT", "w", stdout)) == NULL) {
		/* couldn't reopen it so try opening it instead */
		if ((stdout = fopen("SYS$OUTPUT", "w")) == NULL) {
		    /* don't use int_error here - causes infinite loop! */
		    fputs("Error opening SYS$OUTPUT as stdout\n", stderr);
		}
	    }
	    gpoutfile = stdout;
	}
#endif /* VMS */

	/* Why a goto?  Because we exited the loop below via int_error */
	/* using LONGJMP.  The compiler was not expecting this, and    */
	/* "optimized" the handling of argc and argv such that simply  */
	/* entering the loop again from the top finds them messed up.  */
	/* If we reenter the loop via a goto then there is some hope   */
	/* that code reordering does not hurt us.                      */
	if (reading_from_dash && interactive)
	    goto RECOVER_FROM_ERROR_IN_DASH;
	reading_from_dash = FALSE;

	if (!interactive && !noinputfiles) {
	    term_reset();
	    gp_exit(EXIT_FAILURE);	/* exit on non-interactive error */
	}
    }

    /* load filenames given as arguments */
    while (--argc > 0) {
	    ++argv;
	    c_token = 0;
	    if (!strncmp(*argv, "-persist", 2) || !strcmp(*argv, "--persist")
#ifdef _WIN32
		|| !stricmp(*argv, "-noend") || !stricmp(*argv, "/noend")
#endif
	    ) {
		FPRINTF((stderr,"'persist' command line option recognized\n"));
	    } else if (strcmp(*argv, "-") == 0) {
#if defined(_WIN32) && !defined(WGP_CONSOLE)
		TextShow(&textwin);
		interactive = TRUE;
#else
		interactive = isatty(fileno(stdin));
#endif

RECOVER_FROM_ERROR_IN_DASH:
		reading_from_dash = TRUE;
		while (!com_line());
		reading_from_dash = FALSE;
		interactive = FALSE;
		noinputfiles = FALSE;

	    } else if (strcmp(*argv, "-e") == 0) {
		int save_state = interactive;
		--argc; ++argv;
		if (argc <= 0) {
		    fprintf(stderr, "syntax:  gnuplot -e \"commands\"\n");
		    return 0;
		}
		interactive = FALSE;
		noinputfiles = FALSE;
		do_string(*argv);
		interactive = save_state;

	    } else if (!strncmp(*argv, "-slow", 2) || !strcmp(*argv, "--slow")) {
		slow_font_startup = TRUE;

	    } else if (!strncmp(*argv, "-d", 2) || !strcmp(*argv, "--default-settings")) {
		/* Ignore this; it already had its effect */
		FPRINTF((stderr, "ignoring -d\n"));

	    } else if (strcmp(*argv, "-c") == 0) {
		/* Pass command line arguments to the gnuplot script in the next
		 * argument. This consumes the remainder of the command line
		 */
		interactive = FALSE;
		noinputfiles = FALSE;
		--argc; ++argv;
		if (argc <= 0) {
		    fprintf(stderr, "syntax:  gnuplot -c scriptname args\n");
		    gp_exit(EXIT_FAILURE);
		}
		call_argc = GPMIN(9, argc - 1);
		for (i = 0; i < call_argc; i++) {
		    /* Need to stash argv[i] somewhere visible to load_file() */
		    call_args[i] = gp_strdup(argv[i + 1]);
		}

		load_file(loadpath_fopen(*argv, "r"), gp_strdup(*argv), 5);
		gp_exit(EXIT_SUCCESS);

	    } else if (*argv[0] == '-') {
		fprintf(stderr, "unrecognized option %s\n", *argv);
	    } else {
		interactive = FALSE;
		noinputfiles = FALSE;
		load_file(loadpath_fopen(*argv, "r"), gp_strdup(*argv), 4);
	    }
    }

    /* take commands from stdin */
    if (noinputfiles) {
	while (!com_line())
	    ctrlc_flag = FALSE; /* reset asynchronous Ctrl-C flag */
    }

#ifdef _WIN32
    /* On Windows, handle 'persist' by keeping the main input loop running (windows/wxt), */
    /* but only if there are any windows open. Note that qt handles this properly. */
    if (persist_cl) {
	if (WinAnyWindowOpen()) {
#ifdef WGP_CONSOLE
	    if (!interactive) {
		/* no further input from pipe */
		while (WinAnyWindowOpen())
		win_sleep(100);
	    } else
#endif
	    {
		interactive = TRUE;
		while (!com_line())
		    ctrlc_flag = FALSE; /* reset asynchronous Ctrl-C flag */
		interactive = FALSE;
	    }
	}
    }
#endif

#if (defined(HAVE_LIBREADLINE) || defined(HAVE_LIBEDITLINE)) && defined(GNUPLOT_HISTORY)
#if !defined(HAVE_ATEXIT) && !defined(HAVE_ON_EXIT)
    /* You should be here if you neither have 'atexit()' nor 'on_exit()' */
    wrapper_for_write_history();
#endif /* !HAVE_ATEXIT && !HAVE_ON_EXIT */
#endif /* (HAVE_LIBREADLINE || HAVE_LIBEDITLINE) && GNUPLOT_HISTORY */

#ifdef OS2
    RexxDeregisterSubcom("GNUPLOT", NULL);
#endif

    /* HBB 20040223: Not all compilers like exit() to end main() */
    /* exit(exit_status); */
#if ! defined(_WIN32)
    /* Windows does the cleanup later */
    gp_exit_cleanup();
#endif
    return exit_status;
}


/* Set up to catch interrupts */
void
interrupt_setup()
{
    (void) signal(SIGINT, (sigfunc) inter);

#ifdef SIGPIPE
    /* ignore pipe errors, this might happen with set output "|head" */
    (void) signal(SIGPIPE, SIG_IGN);
#endif /* SIGPIPE */
}


/*
 * Initialize 'constants' stored as variables (user could mangle these)
 */
void
init_constants()
{
    (void) Gcomplex(&udv_pi.udv_value, M_PI, 0.0);
    udv_NaN = get_udv_by_name("NaN");
    (void) Gcomplex(&(udv_NaN->udv_value), not_a_number(), 0.0);
    udv_I = get_udv_by_name("I");
    (void) Gcomplex(&(udv_I->udv_value), 0.0, 1.0);
    udv_Inf = get_udv_by_name("Inf");
    (void) Gcomplex(&(udv_Inf->udv_value), INFINITY, 0.0);
}

/*
 * Initialize graphics context, color palette, local preferences.
 * Called both at program startup and by "reset session".
 */
void
init_session()
{
	/* Disable pipes and system commands during initialization */
	successful_initialization = FALSE;

	/* Undefine any previously-used variables */
	del_udv_by_name("",TRUE);

	/* Clear any previous customization of linetypes */
	while (first_perm_linestyle != NULL)
	    delete_linestyle(&first_perm_linestyle, NULL, first_perm_linestyle);

	/* Restore default colors before loading local preferences */
	set_colorsequence(1);

	/* Reset program variables not handled by 'reset' */
	overflow_handling = INT64_OVERFLOW_TO_FLOAT;
	suppress_warnings = FALSE;

	/* Reset voxel data structures if supported */
	init_voxelsupport();

	/* Make sure all variables start in the same state 'reset'
	 * would set them to.
	 */
	reset_command();	/* FIXME: this does c_token++ */
	load_rcfile(0);		/* System-wide gnuplotrc if configured */

	/* After this point we allow pipes and system commands */
	successful_initialization = TRUE;

	load_rcfile(2);		/* ~/.gnuplot */
	load_rcfile(3);		/* ~/.config/gnuplot/gnuplotrc */

#if !defined(WIN32) && !defined(OS2) && !defined(MSDOS)
	/* Save initial state variables to a file so that later
	 * "save changes" can use them to determine what has changed.
	 */
	if (savefp == NULL) {
	    if (!(savefp = tmpfile()))
		perror("Cannot open initial state save file");
	    else {
		save_all(savefp);
		fflush(savefp);
	    }
	}
#endif
}

/*
 * Read commands from an initialization file.
 * where = 0: look for gnuplotrc in system shared directory
 * where = 1: look for .gnuplot in current directory (DEPRECATED)
 * where = 2: look for .gnuplot in user's home directory
 */
static void
load_rcfile(int where)
{
    FILE *plotrc = NULL;
    char *rcfile = NULL;

    if (skip_gnuplotrc)
	return;

    if (where == 0) {
#ifdef GNUPLOT_SHARE_DIR
# if defined(_WIN32) || defined(MSDOS) || defined(OS2)
	rcfile = RelativePathToGnuplot(GNUPLOT_SHARE_DIR "\\gnuplotrc");
# else
	rcfile = (char *) gp_alloc(strlen(GNUPLOT_SHARE_DIR) + 1 + strlen("gnuplotrc") + 1, "rcfile");
	strcpy(rcfile, GNUPLOT_SHARE_DIR);
	PATH_CONCAT(rcfile, "gnuplotrc");
# endif
	plotrc = fopen(rcfile, "r");
#endif

    } else if (where == 2 && user_homedir) {
	/* length of homedir + directory separator + length of file name + \0 */
	int len = (user_homedir ? strlen(user_homedir) : 0) + 1 + strlen(PLOTRC) + 1;
	rcfile = gp_alloc(len, "rcfile");
	strcpy(rcfile, user_homedir);
	PATH_CONCAT(rcfile, PLOTRC);
	plotrc = fopen(rcfile, "r");
    } else if (where == 3) {
#ifdef USE_XDG_BASEDIR
	rcfile = xdg_get_path(kXDGConfigHome, "gnuplotrc",
			TRUE /* use subdir */, FALSE /* not create */);
	if (rcfile)
	    plotrc = fopen(rcfile, "r");
#endif
    }

    if (plotrc) {
	char *rc = gp_strdup(rcfile ? rcfile : PLOTRC);
	load_file(plotrc, rc, 3);
	push_terminal(0); /* needed if terminal or its options were changed */
    }

    free(rcfile);
}


void
get_user_env()
{
    if (user_homedir == NULL) {
	const char *env_home;

	if ((env_home = getenv(HOME))
#ifdef _WIN32
	    || (env_home = appdata_directory())
	    || (env_home = getenv("USERPROFILE"))
#endif
	    || (env_home = getenv("HOME"))
	   )
	    user_homedir = (const char *) gp_strdup(env_home);
	else if (interactive)
	    int_warn(NO_CARET, "no HOME found");
    }
    if (user_shell == NULL) {
	const char *env_shell;

	if ((env_shell = getenv("SHELL")) == NULL)
#if defined(MSDOS) || defined(_WIN32) || defined(OS2)
	    if ((env_shell = getenv("COMSPEC")) == NULL)
#endif
		env_shell = SHELL;

	user_shell = (const char *) gp_strdup(env_shell);
    }
}

/* expand tilde in path
 * path cannot be a static array!
 * tilde must be the first character in *pathp;
 * we may change that later
 */
void
gp_expand_tilde(char **pathp)
{
    if (!*pathp)
	int_error(NO_CARET, "Cannot expand empty path");

    if ((*pathp)[0] == '~' && (*pathp)[1] == DIRSEP1) {
	if (user_homedir) {
	    size_t n = strlen(*pathp);

	    *pathp = gp_realloc(*pathp, n + strlen(user_homedir), "tilde expansion");
	    /* include null at the end ... */
	    memmove(*pathp + strlen(user_homedir) - 1, *pathp, n + 1);
	    memcpy(*pathp, user_homedir, strlen(user_homedir));
	} else
	    int_warn(NO_CARET, "HOME not set - cannot expand tilde");
    }
}


static void
init_memory()
{
    extend_input_line();
    extend_token_table();
    replot_line = gp_strdup("");
}


#ifdef OS2

int
ExecuteMacro(char *argv, int namelength)
{
    RXSTRING rxRc;
    RXSTRING rxArg[2];
    int rxArgCount = 0;
    char pszName[CCHMAXPATH];
    char *rxArgStr;
    short sRc;
    long rc;

    if (namelength >= sizeof(pszName))
	return 1;
    safe_strncpy(pszName, argv, sizeof(pszName));
    rxArgStr = &argv[namelength];
    RXSTRPTR(rxRc) = NULL;

    /*
       REXX standard calling (gnuplot 3.7pl1 and above):
       The program name is not supplied and so all actual arguments
       are in a single string:
       Parse Arg param
       We even handle blanks like cmd.exe when calling REXX programs.
     */

    if (*rxArgStr) {
	MAKERXSTRING(rxArg[0], rxArgStr, strlen(rxArgStr));
	rxArgCount++;
    }

    CallFromRexx = TRUE;
    rc = RexxStart(
		      rxArgCount,
		      rxArg,
		      pszName,
		      NULL,
		      "GNUPLOT",
		      RXCOMMAND,
		      NULL,
		      &sRc,
		      &rxRc);
    CallFromRexx = FALSE;

   /* am: a word WRT errors codes:
      the negative ones don't seem to have symbolic names, you can get
      them from the OREXX reference, they're not in REXX Programming Guide -
      no idea where to retrieve them from a Warp 3 reference ??
      The positive ones are somehow referenced in REXXPG
   */
    if (rc < 0) {
	/* REXX error */
    } else if (rc > 0) {
	/* Interpreter couldn't be started */
	if (rc == -4)
	   /* run was cancelled, but don't give error message */
	    rc = 0;
    } else if (rc==0) {
	/* all was fine */
    }

/* We don't we try to use rxRc ?
   BTW, don't use free() instead since it's allocated inside RexxStart()
   and not in our executable using the EMX libraries */
   if (RXSTRPTR(rxRc))
       /* I guess it's NULL if something major went wrong,
	  NULL strings are usually not part of the REXX language ... */
       DosFreeMem(rxRc.strptr);

   return rc;
}

/* Rexx command line interface */
ULONG
RexxInterface(PRXSTRING rxCmd, PUSHORT pusErr, PRXSTRING rxRc)
{
    int rc;
    static JMP_BUF keepenv;
    int cmdlen;

    memcpy(keepenv, command_line_env, sizeof(JMP_BUF));
    if (!SETJMP(command_line_env, 1)) {
	/* Set variable gp_input_line.
	   Watch out for line length of NOT_ZERO_TERMINATED strings ! */
	cmdlen = rxCmd->strlength + 1;
	safe_strncpy(gp_input_line, rxCmd->strptr, gp_input_line_len);
	gp_input_line[cmdlen] = NUL;
	rc = do_line();
	*pusErr = RXSUBCOM_OK;
	rxRc->strptr[0] = rc + '0';
	rxRc->strptr[1] = NUL;
	rxRc->strlength = strlen(rxRc->strptr);
    } else {
/*
   We end up here when bail_to_command_line() is called.
   Therefore sometimes this call should be avoided when
   executing a REXX program (e.g. 'Cancel' from
   PM GUI after a 'pause -1' command)
*/
	*pusErr = RXSUBCOM_ERROR;
	RexxSetHalt(getpid(), 1);
    }
    memcpy(command_line_env, keepenv, sizeof(JMP_BUF));
    return 0;
}
#endif /* OS2 */

#ifdef GNUPLOT_HISTORY

/* cancel_history() can be called by terminals that fork a helper process
 * to make sure that the helper doesn't trash the history file on exit.
 */
void
cancel_history()
{
    expanded_history_filename = NULL;
}

# if defined(HAVE_LIBREADLINE) || defined(HAVE_LIBEDITLINE)

static void
wrapper_for_write_history()
{
    if (!expanded_history_filename)
	return;
    if (history_is_stifled())
	unstifle_history();
    if (gnuplot_history_size >= 0)
	stifle_history (gnuplot_history_size);

    /* returns 0 on success */
    if (write_history(expanded_history_filename))
	fprintf (stderr, "Warning:  Could not write history file!!!\n");

    unstifle_history();
}

# else /* HAVE_LIBREADLINE || HAVE_LIBEDITLINE */

/* version for gnuplot's own write_history */
static void
wrapper_for_write_history()
{
    /* What we really want to do is truncate(expanded_history_filename),
       but this is only available on BSD compatible systems */
    if (!expanded_history_filename)
	return;
    remove(expanded_history_filename);
    if (gnuplot_history_size < 0)
	write_history(expanded_history_filename);
    else
	write_history_n(gnuplot_history_size, expanded_history_filename, "w");
}

# endif /* HAVE_LIBREADLINE || HAVE_LIBEDITLINE */
#endif /* GNUPLOT_HISTORY */

void
restrict_popen()
{
    if (!successful_initialization)
	int_error(NO_CARET,"Pipes and shell commands not permitted during initialization");
}


#if defined(MSDOS) || defined(OS2)
/* retrieve path relative to gnuplot executable */
char *
RelativePathToGnuplot(const char * path)
{
    char * rel_path, *s;

    rel_path = (char * ) gp_alloc(strlen(progpath) + strlen(path) + 1, "RelativePathToGnuplot");
    strcpy(rel_path, progpath);
    /* progpath is guaranteed to have a trailing slash */
    strcat(rel_path, path);
    /* convert slashes to backslashes */
    for (s = rel_path;*s != NUL; s++)
	if (*s == DIRSEP2)
	    *s = DIRSEP1;
    return rel_path;
}
#endif
