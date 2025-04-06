/*
 *
 *    G N U P L O T  --  misc.c
 *
 *  Copyright (C) 1986 Thomas Williams, Colin Kelley
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

extern BOOLEAN screen_ok;

extern struct curve_points plot[];
extern int c_token,next_value,next_function;
extern struct udft_entry udft[];
extern struct at_type *curr_at;
extern struct ft_entry ft[];
extern struct vt_entry vt[];

char *malloc();


pointmem(samples)
int samples;
{
register int i;
        for (i = MAX_PLOTS-1; i >= 0; i--)
                if (plot[i].points != NULL) {
                        free(plot[i].points);
                        plot[i].points = NULL;
                }
        for (i = 0; i < MAX_PLOTS; i++)
                if ((plot[i].points = (struct coordinate *)
                        malloc((samples+1) * sizeof(struct coordinate))) == NULL) {
                                fprintf(stderr,"only space for %d plots\n",i);
                                screen_ok = FALSE;
                                break;
                        }
}


save_functions(fp)
FILE *fp;
{
int i;

        if (fp == 0)
                os_error("Cannot open save file",c_token);
        else {
                for (i=0; i < next_function; i++)
                        fprintf(fp,"%s\n",udft[i].definition);
        }
        (void) fclose(fp);
}


save_variables(fp)
FILE *fp;
{
int i;

        if (fp == 0)
                os_error("Cannot open save file",c_token);
        else {
                for (i=0; i < next_value; i++) {
                        fprintf(fp,"%s = ",vt[i].vt_name);
                        show_value(fp,&vt[i].vt_value);
                        (void) putc('\n',fp);
                }
        }
        (void) fclose(fp);
}


save_all(fp)
FILE *fp;
{
int i;

        if (fp == 0)
                os_error("Cannot open save file",c_token);
        else {
                for (i=0; i < next_function; i++)
                        fprintf(fp,"%s\n",udft[i].definition);
                for (i=0; i < next_value; i++) {
                        fprintf(fp,"%s = ",vt[i].vt_name);
                        show_value(fp,&vt[i].vt_value);
                        (void) putc('\n',fp);
                }
        }
        (void) fclose(fp);
}


load_file(fp)
FILE *fp;
{
register int len;
extern char input_line[];

        if ( fp == 0 )
                os_error("Cannot open load file",c_token);
        else {
                while (fgets(input_line,MAX_LINE_LEN,fp)) {
                        len = strlen(input_line) - 1;
                        if (input_line[len] == '\n')
                                input_line[len] = '\0';

                        screen_ok = FALSE;      /* make sure command line is
                                           echoed on error */
                        do_line();
                }
        }
        (void) fclose(fp);
}



view_variables()
{
int i;

        screen_ok = FALSE;
        fprintf(stderr,"\nVariables:\n");
        for (i=0; i < next_value; i++) {
                fprintf(stderr,"%-*s ",MAX_ID_LEN,vt[i].vt_name);
                if (vt[i].vt_undef)
                        fputs("is undefined\n",stderr);
                else {
                        fputs("= ",stderr);
                        show_value(stderr,&vt[i].vt_value);
                        (void) putc('\n',stderr);
                }
        }
        (void) putc('\n',stderr);
}


view_functions()
{
int i;
        screen_ok = FALSE;
        fprintf(stderr,"\nUser-Defined Functions:\n");
        for (i=0; i < next_function; i++)
                if (udft[i].at.count == 0)
                        fprintf(stderr,"%s is undefined\n",udft[i].udft_name);
                else
                        fprintf(stderr,"%s\n",udft[i].definition);
        (void) putc('\n',stderr);
}


view_at()
{
static struct at_type at;
        screen_ok = FALSE;
        build_at(&at);          /* build action table in at */
        (void) putc('\n',stderr);
        show_at(0);
        (void) putc('\n',stderr);
}


show_at(level)
int level;
{
struct at_type *at_address;
int i, j;
struct value *arg;

        at_address = curr_at;
        for (i = 0; i < at_address->count; i++) {
                for (j = 0; j < level; j++)
                        (void) putc(' ',stderr);        /* indent */

                        /* print name of action instruction */
                fputs(ft[at_address->actions[i].index].ft_name,stderr);
                arg = &(at_address->actions[i].arg);
                        /* now print optional argument */

                switch(at_address->actions[i].index) {
                  case (int)PUSH:       fprintf(stderr," (%s)\n",
                                          vt[arg->v.int_val].vt_name);
                                        break;
                  case (int)PUSHC:      (void) putc('(',stderr);
                                        show_value(stderr,arg);
                                        fputs(")\n",stderr);
                                        break;
                  case (int)PUSHD:      fprintf(stderr," (%s dummy)\n",
                                          udft[arg->v.int_val].udft_name);
                                        break;
                  case (int)CALL:       fprintf(stderr," (%s)\n",
                                          udft[arg->v.int_val].udft_name);
                                        curr_at = &udft[arg->v.int_val].at;
                                        show_at(level+2); /* recurse! */
                                        curr_at = at_address;
                                        break;
                  default:
                                        (void) putc('\n',stderr);
                }
        }
}

#ifdef vms
#define OS "vms"
#endif

#ifdef unix
#define OS "unix"
#endif

#ifdef MSDOS
#define OS "MS-DOS"
#endif

#ifndef OS
#define OS ""
#endif

show_version()
{
        extern char version[];
        extern char date[];
        screen_ok = FALSE;
        fprintf(stderr,"%s v%s (%s);  %s\n\n", PROGRAM, version, OS, date);
}
