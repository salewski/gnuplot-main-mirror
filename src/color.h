/* GNUPLOT - color.h */

/* invert the gray for negative figure (default is positive) */

/*[
 *
 * Petr Mikulik, December 1998 -- June 1999
 * Copyright: open source as much as possible
 *
]*/


/*
In general, this file deals with colours, and in the current gnuplot
source layout it would correspond to structures and routines found in
driver.h, term.h and term.c.

Here we define structures which are required for the communication
of palettes between terminals and making palette routines.
*/

#ifndef COLOR_H
#define COLOR_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

typedef enum colortype {
	TC_DEFAULT	= 0,	/* Use default color, set separately */
	TC_LT		= 1,	/* Use the color of linetype <n> */
	TC_LINESTYLE	= 2,	/* Use the color of line style <n> */
	TC_RGB		= 3,	/* Explicit RGB triple provided by user */
	TC_CB		= 4,	/* "palette cb <value>" */
	TC_FRAC		= 5,	/* "palette frac <value> */
	TC_Z		= 6,	/* "palette z" */
	TC_VARIABLE	= 7,	/* only used for "tc", never "lc" */
	TC_COLORMAP	= 8	/* "palette colormap" */
} colortype;

/* Generalized pm3d-compatible color specifier
 * Supplements basic linetype choice */
typedef struct t_colorspec {
    colortype type;		/* TC_<type> definitions below */
    union {
	int lt;			/* used for TC_LT, TC_LINESTYLE */
	unsigned rgbcolor;	/* used for TC_RGB */
    };
    double value;		/* used for TC_CB and TC_FRAC */
} t_colorspec;

#define DEFAULT_COLORSPEC {.type=TC_DEFAULT, .lt=0, .value=0.0}
#define BLACK_COLORSPEC {.type=TC_LT, .lt=LT_BLACK, .value=0.0}
#define BACKGROUND_COLORSPEC {.type=TC_LT, .lt=LT_BACKGROUND, .value=0.0}
#define DEFAULT_FILL_COLORSPEC {.type=TC_DEFAULT, .lt=LT_BLACK, .value=0.0}

/* EAM July 2004 - Disentangle polygon support and PM3D support  */
/* a point (with integer coordinates) for use in polygon drawing */
typedef struct {
    int x, y;
    int style;
} gpiPoint;

#include "gp_types.h"
#include "eval.h"

/*
 *    color modes
 */
typedef enum {
    SMPAL_COLOR_MODE_NONE = '0',
    SMPAL_COLOR_MODE_GRAY = 'g',      /* grayscale only */
    SMPAL_COLOR_MODE_RGB = 'r',       /* one of several fixed transforms */
    SMPAL_COLOR_MODE_FUNCTIONS = 'f', /* user defined transforms */
    SMPAL_COLOR_MODE_GRADIENT = 'd',  /* interpolated table:
				       * explicitly defined or read from file */
    SMPAL_COLOR_MODE_CUBEHELIX = 'c',
    SMPAL_COLOR_MODE_VIRIDIS = 'v'
} palette_color_mode;

/*
 *    color gradient type
 */
typedef enum {
    SMPAL_GRADIENT_TYPE_NONE     = 0,
    SMPAL_GRADIENT_TYPE_SMOOTH   = 1, /* smooth palette */
    SMPAL_GRADIENT_TYPE_DISCRETE = 2, /* (full) discrete palette */
    SMPAL_GRADIENT_TYPE_MIXED    = 3, /* partially discrete palette */
} palette_gradient_type;

#define CHECK_SMPAL_IS_DISCRETE_GRADIENT (sm_palette.colorMode == SMPAL_COLOR_MODE_GRADIENT && sm_palette.gradient_type == SMPAL_GRADIENT_TYPE_DISCRETE)

/* Contains a colour in RGB scheme.
   Values of  r, g and b  are all in range [0;1] */
typedef struct {
    double r, g, b;
} rgb_color;

/* Contains a colour in RGB scheme.
   Values of  r, g and b  are uchars in range [0;255] */
typedef struct {
    unsigned char r, g, b;
} rgb255_color;


/* a point (with double coordinates) for use in polygon drawing */
/* the "c" field is used only inside the routine pm3d_plot() */
typedef struct {
    double x, y, z, c;
} gpdPoint;


/* to build up gradients:  whether it is really red, green and blue or maybe
 * hue saturation and value in col depends on cmodel */
typedef struct {
  double pos;
  rgb_color col;
} gradient_struct;


/*
  inverting the colour for negative picture (default is positive picture)
  (for pm3d.positive)
*/
#define SMPAL_NEGATIVE  'n'
#define SMPAL_POSITIVE  'p'


/* Declaration of smooth palette, i.e. palette for smooth colours */

typedef struct {
  /** Constants: **/

  /* (Fixed) number of formulae implemented for gray index to RGB
   * mapping in color.c.  Usage: somewhere in `set' command to check
   * that each of the below-given formula R,G,B are lower than this
   * value. */
  int colorFormulae;

  /** Values that can be changed by `set' and shown by `show' commands: **/

  /* can be SMPAL_COLOR_MODE_GRAY or SMPAL_COLOR_MODE_RGB */
  palette_color_mode colorMode;
  /* mapping formulae for SMPAL_COLOR_MODE_RGB */
  int formulaR, formulaG, formulaB;
  char positive;		/* positive or negative figure */

  /* Now the variables that contain the discrete approximation of the
   * desired palette of smooth colours as created by make_palette in
   * pm3d.c.  This is then passed into terminal's make_palette, who
   * transforms this [0;1] into whatever it supports.  */

  /* Only this number of colour positions will be used even though
   * there are some more available in the discrete palette of the
   * terminal.  Useful for multiplot.  Max. number of colours is taken
   * if this value equals 0.  Unused by: PostScript */
  int use_maxcolors;
  /* Number of colours used for the discrete palette. Equals to the
   * result from term->make_palette(NULL), or restricted by
   * use_maxcolor.  Used by: pm, gif. Unused by: PostScript */
  int colors;
  /* Table of RGB triplets resulted from applying the formulae. Used
   * in the 2nd call to term->make_palette for a terminal with
   * discrete colours. Unused by PostScript which calculates them
   * analytically. */
  rgb_color *color;

  /** Variables used by some terminals **/

  /* Option unique for output to PostScript file.  By default,
   * ps_allcF=0 and only the 3 selected rgb color formulae are written
   * into the header preceding pm3d map in the file.  If ps_allcF is
   * non-zero, then print there all color formulae, so that it is easy
   * to play with choosing manually any color scheme in the PS file
   * (see the definition of "/g"). Like that you can get the
   * Rosenbrock multiplot figure on my gnuplot.html#pm3d demo page.
   * Note: this option is used by all terminals of the postscript
   * family, i.e. postscript, pslatex, epslatex, so it will not be
   * comfortable to move it to the particular .trm files. */
  TBOOLEAN ps_allcF;

  /* These variables are used to define interpolated color palettes:
   * gradient is an array if (gray,color) pairs.  This array is
   * gradient_num entries big.
   * Interpolated tables are used if colorMode==SMPAL_COLOR_MODE_GRADIENT */
  int gradient_num;
  gradient_struct *gradient;
  /* Smallest nonzero gradient[i+1] - gradient[i].  If this is < (1/colors)
   * Then a truncated gray value may miss the gradient it belongs in. */
  double smallest_gradient_interval;

  /* 
   * Identifier of color gradient type which is one of, 
   *   1. Smooth gradient (SMPAL_GRADIENT_TYPE_SMOOTH)
   *   2. Discrete gradient (SMPAL_GRADIENT_TYPE_DISCRETE)
   *   3. Smooth and Discrete Mixed gradient (SMPAL_GRADIENT_TYPE_MIXED)
   * This value set by the routine 'check_palette_gradient_type'.
   */
  int gradient_type;

  /* the used color model: RGB, HSV, CMY */
  int cmodel;

  /* Three mapping function for gray->RGB/HSV/CMY/etc. mapping
   * used if colorMode == SMPAL_COLOR_MODE_FUNCTIONS */
  struct udft_entry Afunc;  /* R for RGB, H for HSV, C for CMY, ... */
  struct udft_entry Bfunc;  /* G for RGB, S for HSV, M for CMY, ... */
  struct udft_entry Cfunc;  /* B for RGB, V for HSV, Y for CMY, ... */

  /* gamma for gray scale and cubehelix palettes only */
  double gamma;

  /* control parameters for the cubehelix palette scheme */
  double cubehelix_start;	/* offset (radians) from colorwheel 0 */
  double cubehelix_cycles;	/* number of times round the colorwheel */
  double cubehelix_saturation;	/* color saturation */

  /* offset for HSV color mapping */
  double HSV_offset;		/* offset (radians) from colorwheel 0 */

} t_sm_palette;



/* GLOBAL VARIABLES */

extern t_sm_palette sm_palette;
extern int enable_reset_palette;


/* ROUTINES */


void init_color(void);  /* call once to initialize variables */


/*
  Make the colour palette. Return 0 on success
  Put number of allocated colours into sm_palette.colors
*/
int make_palette(void);
void reset_palette(void);

void invalidate_palette(void);
void check_palette_gradient_type(void);

void set_palette(void);

/*
   Send current colour to the terminal
*/
void set_color( double gray );
void set_rgbcolor_var( unsigned int rgbvalue );
void set_rgbcolor_const( unsigned int rgbvalue );

/*
  Draw colour smooth box
*/
void draw_color_smooth_box(int plot_mode);

/*
 Support for user-callable routines
*/
void f_hsv2rgb(union argument *);
void f_rgbcolor(union argument *);
void f_palette(union argument *);

/*
 * miscellaneous color conversions
 */
unsigned int rgb_from_colorspec(struct t_colorspec *tc);
unsigned int rgb_from_gray( double gray );

/*
 * Support for colormaps (named palettes)
 */
unsigned int rgb_from_colormap(double gray, udvt_entry *colormap);
double map2gray(double z, udvt_entry *colormap);
void get_colormap_range( udvt_entry *colormap, double *cm_min, double *cm_max );

#endif /* COLOR_H */

/* eof color.h */
