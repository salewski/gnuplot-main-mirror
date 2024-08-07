/* GNUPLOT - breaders.c */

/*[
 * Copyright 2004  Petr Mikulik
 *
 * As part of the program Gnuplot, which is
 *
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

/* AUTHOR : Petr Mikulik */

/*
 * Readers to set up binary data file information for particular formats.
 */

#include "breaders.h"
#include "datafile.h"
#include "alloc.h"
#include "misc.h"

/*
 * Local prototypes
 */

/*
 * Reader for the ESRF Header File format files (EDF / EHF).
 */

/* Inside datafile.c, but kept hidden. */
extern int df_no_bin_cols;	/* cols to read */
extern df_endianess_type df_bin_file_endianess;
extern TBOOLEAN df_matrix_file, df_binary_file;
extern void *df_pixeldata;

/* Reader for the ESRF Header File format files (EDF / EHF).
 */

/* gen_table4 */
struct gen_table4 {
    const char *key;
    int value;
    short signum; /* 0..unsigned, 1..signed, 2..float or double */
    short sajzof; /* sizeof on 32bit architecture */
};

/* Exactly like lookup_table_nth from tables.c, but for gen_table4 instead
 * of gen_table.
 */ 
static int
lookup_table4_nth(const struct gen_table4 *tbl, const char *search_str)
{
    int k = -1;
    while (tbl[++k].key)
	if (tbl[k].key && !strncmp(search_str, tbl[k].key, strlen(tbl[k].key)))
	    return k;
    return -1; /* not found */
}

static const struct gen_table4 edf_datatype_table[] =
{
    { "UnsignedByte",	DF_UCHAR,   0, 1 },
    { "SignedByte",	DF_CHAR,    1, 1 },
    { "UnsignedShort",	DF_USHORT,  0, 2 },
    { "SignedShort",	DF_SHORT,   1, 2 },
    { "UnsignedInteger",DF_UINT,    0, 4 },
    { "SignedInteger",	DF_INT,	    1, 4 },
    { "UnsignedLong",	DF_ULONG,   0, 8 },
    { "SignedLong",	DF_LONG,    1, 8 },
    { "FloatValue",	DF_FLOAT,   2, 4 },
    { "DoubleValue",	DF_DOUBLE,  2, 8 },
    { "Float",		DF_FLOAT,   2, 4 }, /* Float and FloatValue are synonyms */
    { "Double",		DF_DOUBLE,  2, 8 }, /* Double and DoubleValue are synonyms */
    { NULL, -1, -1, -1 }
};

static const struct gen_table edf_byteorder_table[] =
{
    { "LowByteFirst",	DF_LITTLE_ENDIAN }, /* little endian */
    { "HighByteFirst",	DF_BIG_ENDIAN },    /* big endian */
    { NULL, -1 }
};

/* Orientation of axes of the raster, as the binary matrix is saved in 
 * the file.
 */
enum EdfRasterAxes {
    EDF_RASTER_AXES_XrightYdown,	/* matricial format: rows, columns */
    EDF_RASTER_AXES_XrightYup		/* cartesian coordinate system */
    /* other 6 combinations not available (not needed until now) */
};

static const struct gen_table edf_rasteraxes_table[] =
{
    { "XrightYdown",	EDF_RASTER_AXES_XrightYdown },
    { "XrightYup",	EDF_RASTER_AXES_XrightYup },
    { NULL, -1 }
};


/* Find value_ptr as pointer to the parameter of the given key in the header.
 * Returns NULL on success.
 */
static char*
edf_findInHeader ( const char* header, const char* key )
{
    char *value_ptr = strstr( header, key );

    if (!value_ptr) 
	return NULL;
    /* an edf line is "key     = value ;" */
    value_ptr = 1 + strchr(value_ptr + strlen(key), '=');
    while (isspace((unsigned char)*value_ptr)) 
	value_ptr++;
    return value_ptr;
}
 
void
edf_filetype_function(void)
{
    FILE *fp;
    char *header = NULL;
    int header_size = 0;
    char *p;
    int k;
    /* open (header) file */
    fp = loadpath_fopen(df_filename, "rb");
    if (!fp)
	os_error(NO_CARET, "Can't open data file \"%s\"", df_filename);
    /* read header: it is a multiple of 512 B ending by "}\n" */
    while (header_size == 0 || strncmp(&header[header_size-2],"}\n",2)) {
	int header_size_prev = header_size;
	header_size += 512;
	if (!header)
	    header = gp_alloc(header_size+1, "EDF header");
	else
	    header = gp_realloc(header, header_size+1, "EDF header");
	header[header_size_prev] = 0; /* protection against empty file */
	k = fread(header+header_size_prev, 512, 1, fp);
	if (k == 0) { /* protection against indefinite loop */
	    free(header);
	    os_error(NO_CARET, "Damaged EDF header of %s: not multiple of 512 B.\n", df_filename);
	}
	header[header_size] = 0; /* end of string: protection against strstr later on */
    }
    fclose(fp);
    /* make sure there is a binary record structure for each image */
    if (df_num_bin_records < 1)
	df_add_binary_records(1-df_num_bin_records, DF_CURRENT_RECORDS); /* otherwise put here: number of images (records) from this file */
    if ((p = edf_findInHeader(header, "EDF_BinaryFileName"))) {
	int plen = strcspn(p, " ;\n");
	df_filename = gp_realloc(df_filename, plen+1, "datafile name");
	safe_strncpy(df_filename, p, plen);
	if ((p = edf_findInHeader(header, "EDF_BinaryFilePosition")))
	    df_bin_record[0].scan_skip[0] = atoi(p);
	else
	    df_bin_record[0].scan_skip[0] = 0;
    } else
	df_bin_record[0].scan_skip[0] = header_size; /* skip header */
    /* set default values */
    df_bin_record[0].scan_dir[0] = 1;
    df_bin_record[0].scan_dir[1] = -1;
    df_bin_record[0].scan_generate_coord = TRUE;
    df_bin_record[0].cart_scan[0] = DF_SCAN_POINT;
    df_bin_record[0].cart_scan[1] = DF_SCAN_LINE;
    df_extend_binary_columns(1);
    df_set_skip_before(1,0);
    df_set_skip_after(1,0);
    df_no_use_specs = 1;
    use_spec[0].column = 1;
    /* now parse the header */
    if ((p = edf_findInHeader(header, "Dim_1")))
	df_bin_record[0].scan_dim[0] = atoi(p);
    if ((p = edf_findInHeader(header, "Dim_2")))
	df_bin_record[0].scan_dim[1] = atoi(p);
    if ((p = edf_findInHeader(header, "DataType"))) {
	k = lookup_table4_nth(edf_datatype_table, p);
	if (k >= 0) { /* known EDF DataType */
	    int s = edf_datatype_table[k].sajzof; 
	    switch (edf_datatype_table[k].signum) {
		case 0: df_set_read_type(1,SIGNED_TEST(s)); break;
		case 1: df_set_read_type(1,UNSIGNED_TEST(s)); break;
		case 2: df_set_read_type(1,FLOAT_TEST(s)); break;
	    }
	}
    }
    if ((p = edf_findInHeader(header, "ByteOrder"))) {
	k = lookup_table_nth(edf_byteorder_table, p);
	if (k >= 0)
	    df_bin_file_endianess = edf_byteorder_table[k].value;
    }
    /* Origin vs center: EDF specs allows only Center, but it does not hurt if
       Origin is supported as well; however, Center rules if both specified.
    */
    if ((p = edf_findInHeader(header, "Origin_1"))) {
	df_bin_record[0].scan_cen_or_ori[0] = atof(p);
	df_bin_record[0].scan_trans = DF_TRANSLATE_VIA_ORIGIN;
    }
    if ((p = edf_findInHeader(header, "Origin_2"))) {
	df_bin_record[0].scan_cen_or_ori[1] = atof(p);
	df_bin_record[0].scan_trans = DF_TRANSLATE_VIA_ORIGIN;
    }
    if ((p = edf_findInHeader(header, "Center_1"))) {
	df_bin_record[0].scan_cen_or_ori[0] = atof(p);
	df_bin_record[0].scan_trans = DF_TRANSLATE_VIA_CENTER;
    }
    if ((p = edf_findInHeader(header, "Center_2"))) {
	df_bin_record[0].scan_cen_or_ori[1] = atof(p);
	df_bin_record[0].scan_trans = DF_TRANSLATE_VIA_CENTER;
    }
    /* now pixel sizes and raster orientation */
    if ((p = edf_findInHeader(header, "PSize_1")))
	df_bin_record[0].scan_delta[0] = atof(p);
    if ((p = edf_findInHeader(header, "PSize_2")))
	df_bin_record[0].scan_delta[1] = atof(p);
    if ((p = edf_findInHeader(header, "RasterAxes"))) {
	k = lookup_table_nth(edf_rasteraxes_table, p);
	switch (k) {
	    case EDF_RASTER_AXES_XrightYup:
		df_bin_record[0].scan_dir[0] = 1;
		df_bin_record[0].scan_dir[1] = 1;
		df_bin_record[0].cart_scan[0] = DF_SCAN_POINT;
		df_bin_record[0].cart_scan[1] = DF_SCAN_LINE;
		break;
	    default: /* also EDF_RASTER_AXES_XrightYdown */
		df_bin_record[0].scan_dir[0] = 1;
		df_bin_record[0].scan_dir[1] = -1;
		df_bin_record[0].cart_scan[0] = DF_SCAN_POINT;
		df_bin_record[0].cart_scan[1] = DF_SCAN_LINE;
	}
    }

    free(header);

}



#define FILETYPE_PNG 1
#define FILETYPE_GIF 2
#define FILETYPE_JPEG 3

#ifdef HAVE_GD_PNG

/*
 *	Image reading routines using libgd
 *	    png_filetype_function
 *	    jpeg_filetype_function
 *	    gif_filetype_function
 *	    df_image_get_pixel
 *
 *	Use libgd for input of binary images in PNG GIF JPEG formats
 *	Ethan A Merritt - August 2009
 */

#include <gd.h>
static gdImagePtr im = NULL;

/* local prototypes for libgd */
static void gd_filetype_function(int filetype, char *filename);

void
png_filetype_function(void)
{
    gd_filetype_function(FILETYPE_PNG, df_filename);
}

void
gif_filetype_function(void)
{
    gd_filetype_function(FILETYPE_GIF, df_filename);
}

void
jpeg_filetype_function(void)
{
    gd_filetype_function(FILETYPE_JPEG, df_filename);
}

static void
gd_filetype_function(int filetype, char *filename)
{
    FILE *fp;
    unsigned int M, N;

    /* free previous image, if any */
    if (im) {
	gdImageDestroy(im);
	im = NULL;
    }

    /* read image into memory */
    fp = loadpath_fopen(filename, "rb");
    if (!fp)
	int_error(NO_CARET, "Can't open data file \"%s\"", filename);

    switch(filetype) {
	case FILETYPE_PNG:	im = gdImageCreateFromPng(fp); break;
	case FILETYPE_GIF:
#ifdef HAVE_GD_GIF
			im = gdImageCreateFromGif(fp);
#endif
			break;
	case FILETYPE_JPEG:
#ifdef HAVE_GD_JPEG
			im = gdImageCreateFromJpeg(fp);
#endif
	default:	break;
    }
    fclose(fp);

    if (!im)
	int_error(NO_CARET, "libgd doesn't recognize the format of \"%s\"", filename);

    /* check on image properties and complain if we can't handle them */
    M = im->sx;
    N = im->sy;
    FPRINTF((stderr,"This is a %u X %u %s image\n",M,N,
		im->trueColor ? "TrueColor" : "palette"));

    df_pixeldata = im->trueColor ? (void *)im->tpixels : (void *)im->pixels;
    df_matrix_file = FALSE;
    df_binary_file = TRUE;

    df_bin_record[0].scan_skip[0] = 0;
    df_bin_record[0].scan_dim[0] = M;
    df_bin_record[0].scan_dim[1] = N;

    df_bin_record[0].scan_dir[0] = 1;
    df_bin_record[0].scan_dir[1] = -1;
    df_bin_record[0].scan_generate_coord = TRUE;
    df_bin_record[0].cart_scan[0] = DF_SCAN_POINT;
    df_bin_record[0].cart_scan[1] = DF_SCAN_LINE;

    df_extend_binary_columns(4);
    df_set_read_type(1, DF_UCHAR);
    df_set_read_type(2, DF_UCHAR);
    df_set_read_type(3, DF_UCHAR);
    df_set_read_type(4, DF_UCHAR);
    df_set_skip_before(1,0);

    df_no_use_specs = 4;
    use_spec[0].column = 1;
    use_spec[1].column = 2;
    use_spec[2].column = 3;
    use_spec[3].column = 4;
}

int
df_image_get_pixel(int i, int j, int component)
{
    static int pixel;
    int alpha;

    switch(component) {
    case 0:	pixel = gdImageGetTrueColorPixel(im, i,j);
    		return gdTrueColorGetRed(pixel);
    case 1:	return gdTrueColorGetGreen(pixel);
    case 2:	return gdTrueColorGetBlue(pixel);
    case 3:	/* gdlib alpha runs from 0-127 rather than 0-255 */
		alpha = 2 * gdTrueColorGetAlpha(pixel);
		return (255-alpha);
    default:	return 0; /* shouldn't happen */
    }
}


#elif HAVE_CAIROPDF

/*
 *	Image reading routines using cairo
 *	    png_filetype_function
 *	    jpeg_filetype_function
 *	    gif_filetype_function
 *	    df_image_get_pixel
 *
 *	Ethan A Merritt - June 2024
 */

#include <cairo.h>
static unsigned char *im = NULL;	/* Image data */
static int stride;			/* length in bytes of a single image row */

/* local prototypes for cairo */
static void cairo_filetype_function(int filetype, char *filename);

void
png_filetype_function(void)
{
    cairo_filetype_function(FILETYPE_PNG, df_filename);
}

void
gif_filetype_function(void)
{
    cairo_filetype_function(FILETYPE_GIF, df_filename);
}

void
jpeg_filetype_function(void)
{
    cairo_filetype_function(FILETYPE_JPEG, df_filename);
}

static void
cairo_filetype_function(int filetype, char *filename)
{
    static cairo_surface_t *image = NULL;
    FILE *imagefile = NULL;
    int height, width;

    /* free previous image, if any */
    if (image != NULL) {
	cairo_surface_destroy(image);
	image = NULL;
	im = NULL;
    }

    if (filetype != FILETYPE_PNG)
	int_error(NO_CARET, "this copy of gnuplot can only read png images");

    /* Search for the named file in current load path.
     * If the search is successful, the full path name is in global loadpath_fontname.
     */
    imagefile = loadpath_fopen(filename, "r");
    if (imagefile) {
	cairo_format_t image_format;
	char *fullname = loadpath_fontname ? loadpath_fontname : filename;

	fclose(imagefile);
	image = cairo_image_surface_create_from_png(fullname);
	if (cairo_surface_status(image) != CAIRO_STATUS_SUCCESS) {
	    cairo_surface_destroy(image);
	    int_error(NO_CARET, "cannot read png image from %s\n", fullname);
	}
	im = cairo_image_surface_get_data(image);
	height = cairo_image_surface_get_height(image);
	width = cairo_image_surface_get_width(image);
	stride = cairo_image_surface_get_stride(image);
	image_format = cairo_image_surface_get_format(image);
	FPRINTF((stderr, "Read %dx%d (stride %d) image from %s\n",
		width, height, stride, fullname));
	if (image_format != CAIRO_FORMAT_RGB24 && image_format != CAIRO_FORMAT_ARGB32)
	    int_warn(NO_CARET, "Unsupported format in png file");
    } else {
	int_error(NO_CARET, "cannot find or open %s\n", filename);
    }

    /*
     * Fill in binary record structure for gnuplot's bookkeeping
     */
    df_pixeldata = im;
    df_matrix_file = FALSE;
    df_binary_file = TRUE;

    df_bin_record[0].scan_skip[0] = 0;
    df_bin_record[0].scan_dim[0] = width;
    df_bin_record[0].scan_dim[1] = height;

    df_bin_record[0].scan_dir[0] = 1;
    df_bin_record[0].scan_dir[1] = -1;
    df_bin_record[0].scan_generate_coord = TRUE;
    df_bin_record[0].cart_scan[0] = DF_SCAN_POINT;
    df_bin_record[0].cart_scan[1] = DF_SCAN_LINE;

    df_extend_binary_columns(4);
    df_set_read_type(1, DF_UCHAR);
    df_set_read_type(2, DF_UCHAR);
    df_set_read_type(3, DF_UCHAR);
    df_set_read_type(4, DF_UCHAR);
    df_set_skip_before(1,0);

    df_no_use_specs = 4;
    use_spec[0].column = 1;
    use_spec[1].column = 2;
    use_spec[2].column = 3;
    use_spec[3].column = 4;
}

int
df_image_get_pixel(int i, int j, int component)
{
    /* CAIRO_FORMAT_ARGB32
     *	each pixel is a 32-bit quantity, with alpha in the upper 8 bits,
     *	then red, then green, then blue. Alpha is premultiplied;
     *  that is, 50% transparent red is 0x80800000, not 0x80ff0000.
     * CAIRO_FORMAT_RGB24
     *  each pixel is a 32-bit quantity with the upper 8 bits unused.
     */
    int pixel;

    switch (component) {
	case 0:	/* Red */
		pixel = *(im + stride*j + 4*i + 2);
		break;
	case 1: /* Green */
		pixel = *(im + stride*j + 4*i + 1);
		break;
	case 2: /* Blue */
		pixel = *(im + stride*j + 4*i + 0);
		break;
	case 3: /* Alpha */
		pixel = *(im + stride*j + 4*i + 3);
		break;
	default:
		pixel = 0;
		break;
    }

    return pixel;
}


#elif HAVE_STBI		/* no libgd no libcairo */

/*
 *	Image reading routines using libstbi
 *	    png_filetype_function
 *	    jpeg_filetype_function
 *	    gif_filetype_function
 *	    df_image_get_pixel
 *
 *
 *	Ethan A Merritt - May 2024
 */
#include <stb_image.h>

/* local prototypes for libstbi */
static void stbi_filetype_function(char *filename);

/* Buffer that the image will be read into */
static unsigned char *im = NULL;
static int stbi_rows;
static int stbi_cols;

void
png_filetype_function(void)
{
    stbi_filetype_function(df_filename);
}

void
gif_filetype_function(void)
{
    stbi_filetype_function(df_filename);
}

void
jpeg_filetype_function(void)
{
    stbi_filetype_function(df_filename);
}

static void
stbi_filetype_function(char *filename)
{
    int width, height, channels;
    int ok;
    FILE *fp;

    /* Free any previous image */
    if (im)
	stbi_image_free(im);
    im = NULL;

    /* Check readability and size of requested image file */
    /* Note: the stbi routines may consume up to 92 bytes trying to identify
     * the file type and segfault if this fails.
     */
    if ((fp = loadpath_fopen(filename, "rb")) == NULL)
	int_error(NO_CARET, "Cannot open file %s\n", filename);
    if ((fseek(fp, 0L, SEEK_END) < 0)
    ||  (ftell(fp) < 128L)
    ||  (fseek(fp, 0L, SEEK_SET) < 0))
	int_error(NO_CARET, "file %s not usable as a pixmap\n", filename);

    /* Always request RGBA (4 channel) representation)
     * It would be possible to check file suitability without allocating image data
     *     ok = stbi_info_from_file(fp, &width, &height, &channels);
     *     if (!ok) ...
     */
    im = stbi_load_from_file(fp, &width, &height, &channels, 4);
    if (!im) {
	fclose(fp);
	int_error(NO_CARET, "libstbi failed to open or read from %s\n", filename);
    }
    stbi_rows = height;
    stbi_cols = width;
    fclose(fp);

    /*
     * Fill in binary record structure for gnuplot's bookkeeping
     */
    df_pixeldata = im;
    df_matrix_file = FALSE;
    df_binary_file = TRUE;

    df_bin_record[0].scan_skip[0] = 0;
    df_bin_record[0].scan_dim[0] = width;
    df_bin_record[0].scan_dim[1] = height;

    df_bin_record[0].scan_dir[0] = 1;
    df_bin_record[0].scan_dir[1] = -1;
    df_bin_record[0].scan_generate_coord = TRUE;
    df_bin_record[0].cart_scan[0] = DF_SCAN_POINT;
    df_bin_record[0].cart_scan[1] = DF_SCAN_LINE;

    df_extend_binary_columns(4);
    df_set_read_type(1, DF_UCHAR);
    df_set_read_type(2, DF_UCHAR);
    df_set_read_type(3, DF_UCHAR);
    df_set_read_type(4, DF_UCHAR);
    df_set_skip_before(1,0);

    df_no_use_specs = 4;
    use_spec[0].column = 1;
    use_spec[1].column = 2;
    use_spec[2].column = 3;
    use_spec[3].column = 4;
}

int
df_image_get_pixel(int i, int j, int component)
{
    int channel = im[ 4 * (j * stbi_cols + i) + component ];
    return channel;
}

#else /* No libgd no libcairo no libstbi */

/*
 * No image reading support is present (libgd or libcairo or libstbi)
 * Print error message without opening any files
 *	    png_filetype_function
 *	    jpeg_filetype_function
 *	    gif_filetype_function
 *	    df_image_get_pixel
 *
 */

void png_filetype_function()
{
    int_error(NO_CARET, "This copy of gnuplot was built without support for reading an image file");
}
void jpeg_filetype_function() { png_filetype_function(); }
void gif_filetype_function() { png_filetype_function(); }
int df_image_get_pixel(int i, int j, int component) { return 0; }

#endif /* HAVE_GD_PNG */

TBOOLEAN
df_read_pixmap( t_pixmap *pixmap )
{
    int filetype;
    int i, j;
    coordval *pixel;
    char *file_ext = strrchr(pixmap->filename, '.');

    /* Parse file name */
    if (!file_ext++) {
	int_warn(NO_CARET, "unrecognized pixmap type: %s", pixmap->filename);
	return FALSE;
    }
    if (!strcasecmp(file_ext, "png"))
	filetype = FILETYPE_PNG;
    else if (!strcasecmp(file_ext, "gif"))
	filetype = FILETYPE_GIF;
    else if (!strcasecmp(file_ext, "jpeg") || !strcasecmp(file_ext, "jpg"))
	filetype = FILETYPE_JPEG;
    else {
	/* Clear anything that was there before */
	pixmap->nrows = pixmap->ncols = 0;
	int_warn(NO_CARET, "unrecognized pixmap type: %s", pixmap->filename);
	return FALSE;
    }

    /* Create a blank record that gd_filetype_function can write into */
    df_add_binary_records(1, DF_CURRENT_RECORDS);

#ifdef HAVE_GD_PNG
    gd_filetype_function(filetype, pixmap->filename);
#elif CAIRO_HAS_IMAGE_SURFACE
    cairo_filetype_function(filetype, pixmap->filename);
#elif HAVE_STBI
    stbi_filetype_function(pixmap->filename);
#else
    png_filetype_function(); /* Will emit error */
#endif

    pixmap->ncols = df_bin_record[0].scan_dim[0];
    pixmap->nrows = df_bin_record[0].scan_dim[1];
    pixmap->image_data = gp_realloc( pixmap->image_data,
			4. * sizeof(coordval) * pixmap->ncols * pixmap->nrows, "pixmap");

    /* Fill in image data */
    pixel = pixmap->image_data;
    for (i=0; i<pixmap->nrows; i++)
    for (j=0; j<pixmap->ncols; j++)
    {
	*pixel++ = (coordval)df_image_get_pixel(j,i,0) / 255.;
	*pixel++ = (coordval)df_image_get_pixel(j,i,1) / 255.;
	*pixel++ = (coordval)df_image_get_pixel(j,i,2) / 255.;
	*pixel++ = (coordval)df_image_get_pixel(j,i,3);
    }

    return TRUE;
}

