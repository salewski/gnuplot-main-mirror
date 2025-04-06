/*
 *
 *    G N U P L O T  --  plot.h
 *
 *  Copyright (C) 1986 Colin Kelley, Thomas Williams
 *
 *  You may use this code as you wish if credit is given and this message
 *  is retained.
 *
 *  Please e-mail any useful additions to vu-vlsi!plot so they may be
 *  included in later releases.
 *
 *  This file should be edited with 4-column tabs!  (:set ts=4 sw=4 in vi)
 */

#define PATCHLEVEL 0

#define PROGRAM "gnuplot"
#define PROMPT "gnuplot> "
#define SHELL "/bin/sh"
#ifdef vms
#define HELP  ""
#else /* vms */
#define HELP  "/usr/local/bin/help gnuplot"
#endif

#define TRUE 1
#define FALSE 0

#define Pi 3.141592653589793

#define MAX_PLOTS 9                     /* max number of overlapping plots */
#define MAX_LINE_LEN 255        /* maximum number of chars allowed on line */
#define MAX_TOKENS 200
#define MAX_ID_LEN 20           /* max length of an identifier */

#ifdef PC
#define MAX_UDFS 30                     /* max number of user-defined functions */
#else /* PC */
#define MAX_UDFS 100
#endif /* PC */

#define MAX_VALUES 50           /* max number of user-defined constants */
#define MAX_AT_LEN 100          /* max number of entries in action table */
#define STACK_DEPTH 100
#define NO_CARET (-1)

#define SAMPLES 160                     /* default number of samples for a plot */
#define ZERO    1e-8            /* default for 'zero' set option */

/*
 * note about HUGE:  this number is just used as a flag for really
 *   big numbers, so it doesn't have to be the absolutely biggest number
 *   on the machine.
 */

#ifdef PC
#define  HUGE 1e38
#endif /* PC */

#define END_OF_COMMAND (c_token == num_tokens || equals(c_token,";"))
#define push(arg) f_pushc(arg)  /* same thing! */

#define top_of_stack stack[s_p]

typedef int BOOLEAN;
typedef int (*FUNC_PTR)();

enum {
        C_PI, NEXT_VALUE
};

enum operators {
        PUSH, PUSHC, PUSHD, CALL, TERNIARY, LNOT, BNOT, UMINUS, LOR, LAND, BOR,
        XOR, BAND, EQ, NE, GT, LT, GE, LE, PLUS, MINUS, MULT, DIV, MOD, POWER,
        SF_START
};

enum DATA_TYPES {
        INT, CMPLX
};

enum PLOT_TYPES {
        FUNC, DATA
};

enum PLOT_STYLE {
        LINES, POINTS, IMPULSES
};

struct cmplx {
        double real, imag;
};

struct value {
        enum DATA_TYPES type;
        union {
                char *str_val;
                int int_val;
                struct cmplx cmplx_val;
        } v;
};

struct lexical_unit {
        BOOLEAN is_token;               /* true if token, false if a value */
        struct value l_val;
        int start_index;        /* index of first char in token */
        int length;             /* length of token in chars */
};

struct at_entry {               /* action table entry */
        int index;              /* index into function table */
        struct value arg;
};

struct at_type {
        int count;
        struct at_entry actions[MAX_AT_LEN];
};

struct ft_entry {               /* standard function table entry */
        char *ft_name;          /* pointer to name of this function */
        FUNC_PTR funct;         /* address of function to call */
};

struct udft_entry {             /* user-defined function table entry */
        char udft_name[MAX_ID_LEN+1];/* name of this function entry */
        struct at_type at;      /* action table to execute */
        char definition[MAX_LINE_LEN+1]; /* definition of function as typed */
        struct value dummy_value;/* current value of dummy variable */
};

struct vt_entry {               /* value table entry */
        char vt_name[MAX_ID_LEN+1];/* name of this value entry */
        BOOLEAN vt_undef;               /* true if not defined yet */
        struct value vt_value;  /* value it has */
};

struct coordinate {
        BOOLEAN undefined;      /* TRUE if value off screen */
#ifdef PC
        float x, y;                     /* memory is tight on PCs! */
#else
        double x, y;
#endif /* PC */
};

struct curve_points {
        enum PLOT_TYPES plot_type;
        enum PLOT_STYLE plot_style;
        char title[MAX_LINE_LEN + 1];
        int count;
        struct coordinate *points;
};

struct termentry {
        char name[MAX_ID_LEN + 1];
        unsigned int xmax,ymax,v_char,h_char,v_tic,h_tic;
        FUNC_PTR init,reset,text,graphics,move,vector,linetype,lrput_text,
                ulput_text,point;
};

/*
 * SS$_NORMAL is "normal completion", STS$M_INHIB_MSG supresses
 * printing a status message.
 * SS$_ABORT is the general abort status code.
 from:  Martin Minow
        decvax!minow
 */
#ifdef  vms
#include                <ssdef.h>
#include                <stsdef.h>
#define IO_SUCCESS      (SS$_NORMAL | STS$M_INHIB_MSG)
#define IO_ERROR        SS$_ABORT
#endif /* vms */

#ifndef IO_SUCCESS      /* DECUS or VMS C will have defined these already */
#define IO_SUCCESS      0
#endif
#ifndef IO_ERROR
#define IO_ERROR        1
#endif
