/*
 * Bessel functions and related special functions.
 * The routines in this file are wrappers for implementations in an
 * external library detected at configuration/build time.
 *	AMOS - A Portable Package for Bessel Functions of a Complex Argument
 *	       and Nonnegative Order
 *             D. E. Amos, Sandia National Laboratories, SAND85-1018 (1985)
 * The amos routines have been packaged by various projects in slightly 
 * different ways.
 * The zairy_() routine (note Fortran naming and parameter passing conventions)
 * is found, for example, in libopenspecfun.  We use it to calculate Ai(z).
 * Similarly zbiry_() is used to calcualte Bi(z).
 * The cairy() routine (note C naming but Fortran parameter passing)
 * is from my own build of the AMOS routines.
 *
 * IERR codes from libamos
 *	0	no error
 *	1	input error, no computation
 *	2	overflow predicted from input value
 *	3	overflow from computation
 *	4	input value out of range, no computation
 *	5	other error
 *
 * Ethan A Merritt - March 2020
 */

#include "eval.h"
#include "stdfn.h"	/* for not_a_number() */

#ifdef HAVE_AMOS

extern void zairy_( double *zr, double *zi, int32_t *id, int32_t *kode,
		    double *air, double *aii, int32_t *underflow, int32_t *ierr );
extern void zbiry_( double *zr, double *zi, int32_t *id, int32_t *kode,
		    double *air, double *aii, int32_t *ierr );
extern void zbesk_( double *zr, double *zi, double *nu, int32_t *kode, int32_t *length,
		    double *zbr, double *zbi, int32_t *underflow, int32_t *ierr);

void
f_amos_Ai(union argument *arg)
{
    struct value a;
    struct cmplx z, ai;
    int32_t id, kode, underflow, ierr;

    (void) arg;                        /* avoid -Wunused warning */
    pop(&a);

    id = 0;	/* 0 = Ai  1 = delAi/delZ */
    kode = 1;	/* 1 = unscaled   2 = scaled */

    if (a.type == INTGR) {
	z.real = a.v.int_val;
	z.imag = 0;
    } else {
	z.real = a.v.cmplx_val.real;
	z.imag = a.v.cmplx_val.imag;
    }

    /* Fortran calling conventions! */
    zairy_( &z.real, &z.imag, &id, &kode, &ai.real, &ai.imag, &underflow, &ierr );

    if (underflow != 0 || ierr != 0) {
	FPRINTF((stderr,"zairy( {%.3f, %.3f} ): underflow = %d   ierr = %d\n",
		z.real, z.imag, underflow, ierr));
	Gcomplex(&a, not_a_number(), 0.0);
    } else {
	Gcomplex(&a, ai.real, ai.imag);
    }

    push(&a);
}

void
f_amos_Bi(union argument *arg)
{
    struct value a;
    struct cmplx z, bi;
    int32_t id, kode, ierr;

    (void) arg;                        /* avoid -Wunused warning */
    pop(&a);

    id = 0;	/* 0 = Bi  1 = delBi/delZ */
    kode = 1;	/* 1 = unscaled   2 = scaled */

    if (a.type == INTGR) {
	z.real = a.v.int_val;
	z.imag = 0;
    } else {
	z.real = a.v.cmplx_val.real;
	z.imag = a.v.cmplx_val.imag;
    }

    /* Fortran calling conventions! */
    zbiry_( &z.real, &z.imag, &id, &kode, &bi.real, &bi.imag, &ierr );

    if (ierr != 0) {
	FPRINTF((stderr,"zbiry( {%.3f, %.3f} ): ierr = %d\n",
		z.real, z.imag, ierr));
	Gcomplex(&a, not_a_number(), 0.0);
    } else {
	Gcomplex(&a, bi.real, bi.imag);
    }

    push(&a);
}

/*
 * Modified Bessel function of the second kind K_nu(z)
 *	val = BesselK( nu, z )
 * The underlying libamos routine besk fills in an array of values
 * val[j] corresponding to a sequence functions BesselK( nu+j, z )
 * but we ask for only the j=0 case.
 */
#define NJ 1
void
f_amos_BesselK(union argument *arg)
{
    struct value a;
    struct cmplx z;
    struct cmplx Bk[NJ];
    double nu;
    int32_t kode, length, underflow, ierr;

    (void) arg;                        /* avoid -Wunused warning */

    /* ... unpack arguments ... */
    pop(&a);
    if (a.type == INTGR) {
	z.real = a.v.int_val;
	z.imag = 0;
    } else {
	z.real = a.v.cmplx_val.real;
	z.imag = a.v.cmplx_val.imag;
    }
    nu = real(pop(&a));


    kode = 1;		/* 1 = unscaled   2 = scaled */
    length = NJ;	/* number of members in the returned sequence of functions */

    /* Fortran calling conventions! */
    zbesk_( &z.real, &z.imag, &nu, &kode, &length,
	    &Bk[0].real, &Bk[0].imag, &underflow, &ierr );

    if (ierr != 0) {
	FPRINTF((stderr,"zbesk( {%.3f, %.3f} ): ierr = %d\n",
		z.real, z.imag, ierr));
	Gcomplex(&a, not_a_number(), 0.0);
    } else {
	Gcomplex(&a, Bk[0].real, Bk[0].imag);
    }

    push(&a);
}
#undef NJ

#endif	/* HAVE_AMOS */