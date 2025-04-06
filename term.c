/*
 *
 *    G N U P L O T  --  term.c
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

#include <stdio.h>
#include "plot.h"

#define NICE_LINE               0
#define POINT_TYPES             6

extern FILE *outfile;
extern BOOLEAN term_init;
extern int term;

extern char input_line[];
extern struct lexical_unit token[];
extern struct termentry term_tbl[];

#ifdef PC
static int pattern[] = {0xffff, 0x0f0f, 0xffff, 0x3333, 0x3f3f};

#ifdef CORONA
char screen[65535];
#endif /* CORONA */

int mask;
static int graphics_on = FALSE;
int startx, starty;

#define EGA_XMAX 640
#define EGA_YMAX 350

#define EGA_XLAST (EGA_XMAX - 1)
#define EGA_YLAST (EGA_YMAX - 1)

#define EGA_VCHAR 14
#define EGA_HCHAR 8
#define EGA_VTIC 5
#define EGA_HTIC 5


#define CGA_XMAX 640
#define CGA_YMAX 200

#define CGA_XLAST (CGA_XMAX - 1)
#define CGA_YLAST (CGA_YMAX - 1)

#define CGA_VCHAR 8
#define CGA_HCHAR 8
#define CGA_VTIC 3
#define CGA_HTIC 3

#ifdef CORONA
#define COR_XMAX 640
#define COR_YMAX 325

#define COR_XLAST (COR_XMAX - 1)
#define COR_YLAST (COR_YMAX - 1)

#define COR_VCHAR 13
#define COR_HCHAR 8
#define COR_VTIC 4
#define COR_HTIC 4
#endif /* CORONA */
#endif /* PC */

/*
 * general point routine
 */
line_and_point(x,y,number)
int x,y,number;
{
        /* temporary(?) kludge to allow terminals with bad linetypes
                to make nice marks */

        (*term_tbl[term].linetype)(NICE_LINE);
        do_point(x,y,number);
}

do_point(x,y,number)
int x,y;
int number;
{
register struct termentry *t;
register int htic,vtic;

        number %= POINT_TYPES;
        t = &term_tbl[term];
        htic = (t->h_tic/2);    /* should be in term_tbl[] in later version */
        vtic = (t->v_tic/2);

        if ( x < t->h_tic || y < t->v_tic || x >= t->xmax-t->h_tic ||
                y >= t->ymax-t->v_tic )
                return;                         /* add clipping in later version !!! */

        switch(number) {
                case 0: /* do diamond */
                                (*t->move)(x-htic,y);
                                (*t->vector)(x,y-vtic);
                                (*t->vector)(x+htic,y);
                                (*t->vector)(x,y+vtic);
                                (*t->vector)(x-htic,y);
                                (*t->move)(x,y);
                                (*t->vector)(x,y);
                                break;
                case 1: /* do plus */
                                (*t->move)(x-htic,y);
                                (*t->vector)(x+htic,y);
                                (*t->move)(x,y-vtic);
                                (*t->vector)(x,y+vtic);
                                break;
                case 2: /* do box */
                                (*t->move)(x-htic,y-vtic);
                                (*t->vector)(x+htic,y-vtic);
                                (*t->vector)(x+htic,y+vtic);
                                (*t->vector)(x-htic,y+vtic);
                                (*t->vector)(x-htic,y-vtic);
                                (*t->move)(x,y);
                                (*t->vector)(x,y);
                                break;
                case 3: /* do X */
                                (*t->move)(x-htic,y-vtic);
                                (*t->vector)(x+htic,y+vtic);
                                (*t->move)(x-htic,y+vtic);
                                (*t->vector)(x+htic,y-vtic);
                                break;
                case 4: /* do triangle */
                                (*t->move)(x,y+(4*vtic/3));
                                (*t->vector)(x-(4*htic/3),y-(2*vtic/3));
                                (*t->vector)(x+(4*htic/3),y-(2*vtic/3));
                                (*t->vector)(x,y+(4*vtic/3));
                                (*t->move)(x,y);
                                (*t->vector)(x,y);
                                break;
                case 5: /* do star */
                                (*t->move)(x-htic,y);
                                (*t->vector)(x+htic,y);
                                (*t->move)(x,y-vtic);
                                (*t->vector)(x,y+vtic);
                                (*t->move)(x-htic,y-vtic);
                                (*t->vector)(x+htic,y+vtic);
                                (*t->move)(x-htic,y+vtic);
                                (*t->vector)(x+htic,y-vtic);
                                break;
        }
}



#define AED_XMAX 768
#define AED_YMAX 575

#define AED_XLAST (AED_XMAX - 1)
#define AED_YLAST (AED_YMAX - 1)

#define AED_VCHAR       13
#define AED_HCHAR       8
#define AED_VTIC        8
#define AED_HTIC        7




#define HP75_XMAX 6000
#define HP75_YMAX 6000

#define HP75_XLAST (HP75_XMAX - 1)
#define HP75_YLAST (HP75_XMAX - 1)

/* HP75_VCHAR, HP75_HCHAR  are not used */
#define HP75_VCHAR      (HP75_YMAX/20)
#define HP75_HCHAR      (HP75_XMAX/20)
#define HP75_VTIC       (HP75_YMAX/70)
#define HP75_HTIC       (HP75_XMAX/75)




#define REGISXMAX 800
#define REGISYMAX 440

#define REGISXLAST (REGISXMAX - 1)
#define REGISYLAST (REGISYMAX - 1)

#define REGISVCHAR              20
#define REGISHCHAR              8
#define REGISVTIC               8
#define REGISHTIC               6



#define QMS_XMAX 9000
#define QMS_YMAX 6000

#define QMS_XLAST (QMS_XMAX - 1)
#define QMS_YLAST (QMS_YMAX - 1)

#define QMS_VCHAR               120
#define QMS_HCHAR               75
#define QMS_VTIC                70
#define QMS_HTIC                70




#define TEK40XMAX 1024
#define TEK40YMAX 780

#define TEK40XLAST (TEK40XMAX - 1)
#define TEK40YLAST (TEK40YMAX - 1)

#define TEK40VCHAR      25
#define TEK40HCHAR              14
#define TEK40VTIC               11
#define TEK40HTIC               11




#ifdef UNIXPLOT

#define UP_XMAX 4096
#define UP_YMAX 4096

#define UP_XLAST (UP_XMAX - 1)
#define UP_YLAST (UP_YMAX - 1)

#define UP_VCHAR (UP_YMAX/30)
#define UP_HCHAR (UP_XMAX/72)   /* just a guess--no way to know this! */
#define UP_VTIC (UP_YMAX/80)
#define UP_HTIC (UP_XMAX/80)

#endif /* UNIXPLOT */



#define TERMCOUNT (sizeof(term_tbl)/sizeof(struct termentry))

#ifdef PC

PC_lrput_text(row,str)
int row;
char str[];
{
        PC_curloc(24-row,78-strlen(str));
        PC_puts(str);
}

PC_ulput_text(row,str)
int row;
char str[];
{
        PC_curloc(row+1,2);
        PC_puts(str);
}

#ifdef CORONA
COR_init()
{
}

COR_graphics()
{
        graphics_on = TRUE;
        Vmode(3);
        grinit(screen);
        grandtx();
}

COR_text()
{
        if (graphics_on) {
                graphics_on = FALSE;
                while (!kbhit())
                        ;
        }
        grreset();
        txonly();
        Vmode(3);
}

COR_linetype(linetype)
{
        if (linetype > 2)
                linetype %= 3;
        mask = pattern[linetype+2];
}

COR_move(x,y)
{
        if (x < 0)
                startx = 0;
        else if (x > COR_XLAST)
                startx = COR_XLAST;
        else
                startx = x;

        if (y < 0)
                starty = 0;
        else if (y > COR_YLAST)
                starty = COR_YLAST;
        else
                starty = y;
}

COR_vector(x,y)
{
        if (x < 0)
                x = 0;
        else if (x > COR_XLAST)
                x = COR_XLAST;
        if (y < 0)
                y = 0;
        else if (y > COR_YLAST)
                y = COR_YLAST;

        Cor_line(startx,COR_YLAST-starty,x,COR_YLAST-y);
        startx = x;
        starty = y;
}

#define COR_lrput_text PC_lrput_text
#define COR_ulput_text PC_ulput_text

COR_reset()
{
}
#endif /* CORONA */


CGA_init()
{
        PC_color(1);            /* monochrome */
}

CGA_graphics()
{
        graphics_on = TRUE;
        Vmode(6);
}

CGA_text()
{
        if (graphics_on) {
                graphics_on = FALSE;
                while (!kbhit())
                        ;
                Vmode(3);
        }
}

CGA_linetype(linetype)
{
        if (linetype > 2)
                linetype %= 3;
        PC_mask(pattern[linetype+2]);
}

CGA_move(x,y)
{
        startx = x;
        starty = y;
}


CGA_vector(x,y)
{
        PC_line(startx,CGA_YLAST-starty,x,CGA_YLAST-y);
        startx = x;
        starty = y;
}

#define CGA_lrput_text PC_lrput_text
#define CGA_ulput_text PC_ulput_text


CGA_reset()
{
}


EGA_init()
{
        PC_mask(0xffff);
}

EGA_graphics()
{
        graphics_on = TRUE;
        Vmode(16);
}

EGA_text()
{
        PC_curloc(24,0);
        if (graphics_on) {
                graphics_on = FALSE;
                while (!kbhit())
                        ;
        }
}

EGA_linetype(linetype)
{
        static int c[] = {9, 8, 10, 11, 12, 13, 14, 15, 7, 5, 4, 3, 2, 6};
        PC_color(c[linetype+2]);
}

EGA_move(x,y)
{
        startx = x;
        starty = y;
}


EGA_vector(x,y)
{
        PC_line(startx,EGA_YLAST-starty,x,EGA_YLAST-y);
        startx = x;
        starty = y;
}

#define EGA_lrput_text PC_lrput_text
#define EGA_ulput_text PC_ulput_text


EGA_reset()
{
        Vmode(3);
}
#endif /* PC */


#ifdef AED
AED_init()
{
        fprintf(outfile,
        "\033SEN3DDDN.SEC.7.SCT.0.1.80.80.90.SBC.0.AAV2.MOV.0.9.CHR.0.FFD");
/*   2            3     4                5     7    6       1
        1. Clear Screen
        2. Set Encoding
        3. Set Default Color
        4. Set Backround Color Table Entry
        5. Set Backround Color
        6. Move to Bottom Lefthand Corner
        7. Anti-Alias Vectors
*/
}


AED_graphics()
{
        fprintf(outfile,"\033FFD\033");
}


AED_text()
{
        fprintf(outfile,"\033MOV.0.9.SEC.7.XXX");
}



AED_linetype(linetype)
int linetype;
{
static int color[9+2] = { 7, 1, 6, 2, 3, 5, 1, 6, 2, 3, 5 };
static int type[9+2] = { 85, 85, 255, 255, 255, 255, 255,
                                        85, 85, 85, 85 };

        fprintf(outfile,"\033SLS%d.255.",type[linetype+2]);
        fprintf(outfile,"\033SEC%d.",color[linetype+2]);

}



AED_move(x,y)
int x,y;
{
        fprintf(outfile,"\033MOV%d.%d.",x,y);
}


AED_vector(x,y)
int x,y;
{
        fprintf(outfile,"\033DVA%d.%d.",x,y);
}


AED_lrput_text(row,str) /* write text to screen while still in graphics mode */
int row;
char str[];
{
        AED_move(AED_XMAX-((strlen(str)+2)*AED_HCHAR),AED_VTIC+AED_VCHAR*(row+1));
        fprintf(outfile,"\033XXX%s\033",str);
}


AED_ulput_text(row,str) /* write text to screen while still in graphics mode */
int row;
char str[];
{
        AED_move(AED_HTIC*2,AED_YMAX-AED_VTIC-AED_VCHAR*(row+1));
        fprintf(outfile,"\033XXX%s\033",str);
}


#define hxt (AED_HTIC/2)
#define hyt (AED_VTIC/2)

AED_reset()
{
        fprintf(outfile,"\033SCT0.1.0.0.0.SBC.0.FFD");
}

#endif /* AED */




#ifdef HP75

HP75_init()
{
        fprintf(outfile,
        "\033.Y;IN;\033.P1:SC0,%d,0,%d;\nRO90;IP;CS20;SI0.2137,0.2812;\n",
                HP75_XMAX,HP75_YMAX);
/*       1      2  3       4             5    6  7
        1. turn on eavesdropping
        2. reset to power-up defaults
        3. enable XON/XOFF flow control
        4. set SCaling to 2000 x 2000
        5. rotate page 90 degrees
        6. ???
        7. set some character set stuff
*/
}


HP75_graphics()
{
        fputs("\033.Y",outfile);
/*         1
        1. enable eavesdropping
*/
}


HP75_text()
{
        fputs("NR;\033.Z",outfile);
/*         1  2
        1. go into 'view' mode
        2. disable plotter eavesdropping
*/
}


HP75_linetype(linetype)
int linetype;
{
        fprintf(outfile,"SP%d;\n",3+(linetype%8));
}


HP75_move(x,y)
int x,y;
{
        fprintf(outfile,"PU%d,%d;\n",x,y);
}


HP75_vector(x,y)
int x,y;
{
        fprintf(outfile,"PD%d,%d;\n",x,y);
}


HP75_lrput_text(row,str)
int row;
char str[];
{
        HP75_move(HP75_XMAX-HP75_HTIC*2,HP75_VTIC*2+HP75_VCHAR*row);
        fprintf(outfile,"LO17;LB%s\003\n",str);
}

HP75_ulput_text(row,str)
int row;
char str[];
{
        HP75_move(HP75_HTIC*2,HP75_YMAX-HP75_VTIC*2-HP75_VCHAR*row);
        fprintf(outfile,"LO13;LB%s\003\n",str);
}

HP75_reset()
{
}

#endif /* HP75 */



#ifdef REGIS

REGISinit()
{
        fprintf(outfile,"\033[r\033[24;1H");
/*                   1     2
        1. reset scrolling region
        2. locate cursor on bottom line
*/
}


REGISgraphics()
{
        fprintf(outfile,"\033[2J\033P1pS(C0)");
/*                   1      2      3
        1. clear screen
        2. enter ReGIS graphics
        3. turn off graphics diamond cursor
*/
}


REGIStext()
{
        fprintf(outfile,"\033[24;1H");
/*                       1
        1. locate cursor on last line of screen (and leave ReGIS)
*/
}


REGISlinetype(linetype)
int linetype;
{
        static int in_map[9+2] = {2,2,3,2,3,2,3,2,1,1,1};
        static int lt_map[9+2] = {1,4,1,1,4,4,6,6,1,4,6};
        fprintf(outfile,"W(I%d)",in_map[linetype+2]);
        fprintf(outfile,"W(P%d)",lt_map[linetype+2]);
}


REGISmove(x,y)
int x,y;
{
        fprintf(outfile,"P[%d,%d]v[]",x,REGISYLAST-y,x,REGISYLAST-y);
}


REGISvector(x,y)
int x,y;
{
        fprintf(outfile,"v[%d,%d]",x,REGISYLAST - y);
}


REGISlrput_text(row,str)
int row;
char *str;
{
        REGISmove(REGISXMAX-REGISHTIC-REGISHCHAR*(strlen(str)+3),
                REGISVTIC+REGISVCHAR*(row+1));
        (void) putc('T',outfile); (void) putc('\'',outfile);
        while (*str) {
                (void) putc(*str,outfile);
                if (*str == '\'')
                                (void) putc('\'',outfile);      /* send out another one */
                str++;
        }
        (void) putc('\'',outfile);
}


REGISulput_text(row,str)
int row;
char *str;
{
        REGISmove(REGISVTIC,REGISYMAX-REGISVTIC*2-REGISVCHAR*row);
        (void) putc('T',outfile); (void) putc('\'',outfile);
        while (*str) {
                (void) putc(*str,outfile);
                if (*str == '\'')
                                (void) putc('\'',outfile);      /* send out another one */
                str++;
        }
        (void) putc('\'',outfile);
}


REGISreset()
{
        fprintf(outfile,"\033[2J\033[24;1H");
}

#endif /* REGIS */




#ifdef QMS

QMS_init()
{
        fprintf(outfile,"^IOL\n");
}


QMS_graphics()
{
        fprintf(outfile,"^IGV\n");
}



QMS_text()
{
        fprintf(outfile,"^IGE\n^,");
}


QMS_linetype(linetype)
int linetype;
{
static int width[9+2] = {7, 3, 3, 3, 3, 5, 5, 5, 7, 7, 7};
static int type[9+2] =  {0, 0, 0, 2, 5, 0, 2, 5, 0, 2, 5};
        fprintf(outfile,"^PW%02d\n^V%x\n",width[linetype+2], type[linetype+2]);
}


QMS_move(x,y)
int x,y;
{
        fprintf(outfile,"^U%05d:%05d\n", 1000 + x, QMS_YLAST + 1000 - y);
}


QMS_vector(x2,y2)
int x2,y2;
{
        fprintf(outfile,"^D%05d:%05d\n", 1000 + x2, QMS_YLAST + 1000 - y2);
}


QMS_lrput_text(row,str)
int row;
char str[];
{
        QMS_move(QMS_XMAX-QMS_HTIC-QMS_HCHAR*(strlen(str)+1),
                QMS_VTIC+QMS_VCHAR*(row+1));
        fprintf(outfile,"^IGE\n%s\n^IGV\n",str);
}

QMS_ulput_text(row,str)
int row;
char str[];
{
        QMS_move(QMS_HTIC*2,QMS_YMAX-QMS_VTIC-QMS_VCHAR*(row+1));
        fprintf(outfile,"^IGE\n%s\n^IGV\n",str);
}


QMS_reset()
{
        fprintf(outfile,"^,\n");
}

#endif /* QMS */



#ifdef TEK

#define HX 0x20         /* bit pattern to OR over 5-bit data */
#define HY 0x20
#define LX 0x40
#define LY 0x60

#define LOWER5 31
#define UPPER5 (31<<5)


TEK40init()
{
}


TEK40graphics()
{
        fprintf(outfile,"\033\014");
/*                   1
        1. clear screen
*/
}


TEK40text()
{
        TEK40move(0,12);
        fprintf(outfile,"\037");
/*                   1
        1. into alphanumerics
*/
}


TEK40linetype(linetype)
int linetype;
{
}



TEK40move(x,y)
unsigned int x,y;
{
        (void) putc('\035', outfile);   /* into graphics */
        TEK40vector(x,y);
}


TEK40vector(x,y)
unsigned int x,y;
{
        (void) putc((HY | (y & UPPER5)>>5), outfile);
        (void) putc((LY | (y & LOWER5)), outfile);
        (void) putc((HX | (x & UPPER5)>>5), outfile);
        (void) putc((LX | (x & LOWER5)), outfile);
}


TEK40lrput_text(row,str)
unsigned int row;
char str[];
{
        TEK40move(TEK40XMAX - TEK40HTIC - TEK40HCHAR*(strlen(str)+1),
                TEK40VTIC + TEK40VCHAR*(row+1));
        fprintf(outfile,"\037%s\n",str);
}


TEK40ulput_text(row,str)
unsigned int row;
char str[];
{
        TEK40move(TEK40HTIC, TEK40YMAX - TEK40VTIC - TEK40VCHAR*(row+1));
        fprintf(outfile,"\037%s\n",str);
}


TEK40reset()
{
}

#endif /* TEK */


#ifdef UNIXPLOT
UP_init()
{
        openpl();
        space(0, 0, UP_XMAX, UP_YMAX);
}


UP_graphics()
{
        erase();
}


UP_text()
{
}


UP_linetype(linetype)
int linetype;
{
static char *lt[] = {"solid", "longdashed", "solid", "dotted", "shortdashed",
        "dotdashed", "longdashed"};

        if (linetype >= 5)
                linetype %= 5;
        linemod(lt[linetype+2]);
}


UP_move(x,y)
unsigned int x,y;
{
        move(x,y);
}


UP_vector(x,y)
unsigned int x,y;
{
        cont(x,y);
}


UP_lrput_text(row,str)
unsigned int row;
char str[];
{
        move(UP_XMAX - UP_HTIC - UP_HCHAR*(strlen(str)+1),
                UP_VTIC + UP_VCHAR*(row+1));
        label(str);
}


UP_ulput_text(row,str)
unsigned int row;
char str[];
{
        UP_move(UP_HTIC, UP_YMAX - UP_VTIC - UP_VCHAR*(row+1));
        label(str);
}

UP_reset()
{
        closepl();
}

#endif /* UNIXPLOT */



UNKNOWN_null()
{
        int_error("you must set your terminal type before plotting!",NO_CARET);
}


/*
 * term_tbl[] contains an entry for each terminal.  "unknown" must be the
 *   first, since term is initialized to 0.
 */
struct termentry term_tbl[] = {
        {"unknown", 100, 100, 1, 1, 1, 1, UNKNOWN_null, UNKNOWN_null, UNKNOWN_null,
        UNKNOWN_null, UNKNOWN_null, UNKNOWN_null, UNKNOWN_null, UNKNOWN_null,
        UNKNOWN_null, UNKNOWN_null}
#ifdef PC
        ,{"cga", CGA_XMAX, CGA_YMAX, CGA_VCHAR, CGA_HCHAR,
                CGA_VTIC, CGA_HTIC, CGA_init, CGA_reset,
                CGA_text, CGA_graphics, CGA_move, CGA_vector,
                CGA_linetype, CGA_lrput_text, CGA_ulput_text, line_and_point}
        ,{"ega", EGA_XMAX, EGA_YMAX, EGA_VCHAR, EGA_HCHAR,
                EGA_VTIC, EGA_HTIC, EGA_init, EGA_reset,
                EGA_text, EGA_graphics, EGA_move, EGA_vector,
                EGA_linetype, EGA_lrput_text, EGA_ulput_text, do_point}

#ifdef CORONA
        ,{"corona", COR_XMAX, COR_YMAX, COR_VCHAR, COR_HCHAR,
                COR_VTIC, COR_HTIC, COR_init, COR_reset,
                COR_text, COR_graphics, COR_move, COR_vector,
                COR_linetype, COR_lrput_text, COR_ulput_text, line_and_point}
#endif /* CORONA */
#endif /* PC */

#ifdef AED
        ,{"aed767", AED_XMAX, AED_YMAX, AED_VCHAR, AED_HCHAR,
                AED_VTIC, AED_HTIC, AED_init, AED_reset,
                AED_text, AED_graphics, AED_move, AED_vector,
                AED_linetype, AED_lrput_text, AED_ulput_text, do_point}
#endif

#ifdef HP75
        ,{"hp75xx",HP75_XMAX,HP75_YMAX, HP75_VCHAR, HP75_HCHAR,HP75_VTIC,HP75_HTIC,
                HP75_init,HP75_reset,HP75_text, HP75_graphics, HP75_move, HP75_vector,
                HP75_linetype, HP75_lrput_text, HP75_ulput_text, do_point}
#endif

#ifdef QMS
        ,{"qms",QMS_XMAX,QMS_YMAX, QMS_VCHAR, QMS_HCHAR, QMS_VTIC, QMS_HTIC,
                QMS_init,QMS_reset, QMS_text, QMS_graphics, QMS_move, QMS_vector,
                QMS_linetype,QMS_lrput_text,QMS_ulput_text,line_and_point}
#endif

#ifdef REGIS
        ,{"regis", REGISXMAX, REGISYMAX, REGISVCHAR, REGISHCHAR, REGISVTIC,
                REGISHTIC, REGISinit, REGISreset, REGIStext, REGISgraphics,
                REGISmove,REGISvector,REGISlinetype, REGISlrput_text, REGISulput_text,
                line_and_point}
#endif

#ifdef TEK
        ,{"tek40xx",TEK40XMAX,TEK40YMAX,TEK40VCHAR, TEK40HCHAR, TEK40VTIC,
                TEK40HTIC, TEK40init,TEK40reset, TEK40text, TEK40graphics,
                TEK40move, TEK40vector,TEK40linetype,TEK40lrput_text,
                TEK40ulput_text, line_and_point}
#endif

#ifdef UNIXPLOT
        ,{"unixplot", UP_XMAX, UP_YMAX, UP_VCHAR, UP_HCHAR, UP_VTIC, UP_HTIC,
                UP_init, UP_reset, UP_text, UP_graphics, UP_move, UP_vector,
                UP_linetype, UP_lrput_text, UP_ulput_text, line_and_point}
#endif
        };


list_terms()
{
register int i;

        (void) putc('\n',stderr);
        fprintf(stderr,"available terminals types: \n");
        for (i = 0; i < TERMCOUNT; i++)
                fprintf(stderr,"\t%s\n",term_tbl[i].name);
        (void) putc('\n',stderr);
}


set_term(c_token)
int c_token;
{
register int i,t;

        if (!token[c_token].is_token)
                int_error("terminal name expected",c_token);
        t = -1;
        for (i = 0; i < TERMCOUNT; i++) {
                if (!strncmp(input_line + token[c_token].start_index,term_tbl[i].name,
                        token[c_token].length)) {
                        if (t != -1)
                                int_error("ambiguous terminal name",c_token);
                        t = i;
                }
        }
        if (t == -1)
                int_error("unknown terminal type; type just 'set terminal' for a list",
                        c_token);
        term_init = FALSE;
        return(t);
}
