void multiplot_start(void);
void multiplot_end(void);
void multiplot_next(void);
void multiplot_reset(void);
void append_multiplot_line(char *line);
void replay_multiplot(void);
void multiplot_reset_after_error(void);
void multiplot_use_size_and_origin(void);
int multiplot_current_panel(void);

extern TBOOLEAN multiplot_playback;	/* TRUE while inside "remultiplot" playback */
extern TBOOLEAN suppress_multiplot_save;/* TRUE while inside a for/while loop */
extern int multiplot_last_panel;	/* only this panel will support pan/zoom */
extern int multiplot_highest_panel;	/* the highest panel number actually used */
extern int multiplot_event_panel;	/* most recent event attributed to this */
extern int queued_zoom_panel;		/* pan or zoom requested for this panel */
extern TBOOLEAN in_multiplot_zoom;

/* Some commands (pause, reset, ...) would be problematic if executed during
 * multiplot playback.  Invoke this from the command that should be filtered.
 */
#define filter_multiplot_playback() \
	if (multiplot_playback) { \
	    while (!END_OF_COMMAND) c_token++; \
	    return; \
	}

/*
 * multiplot mousing structures
 */
#define MAX_PANELS 16
extern BoundingBox panel_bounds[MAX_PANELS];	/* terminal coords of next panel */
extern unsigned int panel_flags[MAX_PANELS];	/* bit settings, e.g. PANEL_3D */
#ifdef USE_MOUSE
extern axis_mapping x_mapping[], x2_mapping[], y_mapping[], y2_mapping[];
extern axis_mapping r_mapping[], theta_mapping[];
#endif

#define PANEL_3D	0x01	/* Bit settings in panel_flags[] */
#define PANEL_SPLOT	0x02
#define PANEL_2D	0x04
#define PANEL_POLAR	0x08

