/*********************************************************************
 *
 *
 * Module "PsychoMpegOne" 
 * 
 * Implemenattion of a adapted version of the MPEG Audio Encoder's 
 * Version of the "Psychoacoustic Model One" to be used in 
 * Layer II of the MPEG Audio Encoder as of Distribution 07. 
 * 
 * This Implementation is based on:
 *    ISO MPEG Audio Subgroup Software Simulation Group (1996)
 *    ISO 13818-3 MPEG-2 Audio Encoder - Lower Sampling Frequency Extension
 *
 *    $Id: tonal.c,v 1.1 1996/02/14 04:04:23 rowlands Exp $
 *
 *    $Log: tonal.c,v $
 *    Revision 1.1  1996/02/14 04:04:23  rowlands
 *    Initial revision
 *
 *    Received from Mike Coleman
 *
 * For a detailed list of all contributors to the Implementation of the
 * underlying MPEG Module, see the Implementation of "tonal.c"
 *
 * Author of this adapted version: 
 *   Kyrill A. Fischer, DT Berkom, March/April 1997
 *
 * 
 * Major changes made to the original implementation:
 *
 * o Processing for MONO-signals only (no [2][...]-subscripts
 *   for spike, buffer, etc; omitting outermost loop 
 *   "for(k=0; k<stereo; ...)" )
 *
 * o provided "calling function" "PsychoMpegOne()", that prepares
 *   processing and calls the (adapted) function "II_Psycho_One()"
 *
 * o Using formulas to derive values of MPEG-Parameter tables 
 *   cbound[] and ltg[].bark, ltg[].line, ltg[].hear.  These values are 
 *   therefore no longer read at runtime (initialization using specific 
 *   ascii-files provided as part of the MPEG distribution) but can be 
 *   calculated. This is the major step to make this tool working with
 *   parameters not provided by MPEG (i.e. new sampling frequencies, etc.)
 *   (Old functions: "read_cbound()" and "read_freq_band()"
 *       (still part of this package; #define PMODEF_USE_TABLE to use 
 *       them instead of the new functions); 
 *    New functions: "define_cbound()" and "define_freq_band_v2()"
 *   )
 *
 * o introduced #define-switches for several variations of the algorithm; 
 *   see PMODEF_... -Macros. 
 * 
 * o introduced variable sizes for FFT_SIZE (prev. #def'd to 1024)
 *   and subsequent variables (HAN_SIZE, some vars in "II_f_f_t()")
 *   by making them local vars to this module ("PMO_*")
 *
 * o introduced concept of local variables; names start with "PMO_"
 *
 * o provided result (mask) in the same resolution as internally 
 *   used for FFT (rather than "compressing" it to fit the 32 Filterbank-
 *   outputs used in MPEG) (see function "remap_mask()")
 *
 * o input samples are always centered (maybe zero-padded or cutted to 
 *   fit fft_size)
 *
 * o using function `rfft()' instead of MPEG-function 'II_f_f_t()'
 *
 * o first removed "spike"-variable (it is not needed), but 
 *   then I re-introduced it for comparision with MPEG
 *
 * o used abbreviated FOR(i, MAX) notation (on some places). 
 *
 * o changed "int" to "long" (on most places). 
 *
 * o the algorithm assumes that max. input (i.e. 32768) == 96 dB. 
 *   (see POWERNORM in PsychoMpegOne.h). This is important for comparison
 *   to absolute hearing threshold in quiet, which is defined in absolute 
 *   dB (SPL)-Values in a MPEG table (and stored in ltg[].hear). 
 *
 * o The Function "noise_label()" used to prefer the same positions for 
 *   the resulting noise-component's position within each Bark. The 
 *   determination of this position has therefore been changed; now, this
 *   position is determined by the position of the maximum power[].x
 *   -value within each Bark. #define PMODEF_NOISE_MAXPOS for this new
 *   method. Tho have MPEG's original method re-established, do not define 
 *   this switch.  
 *
 * To be done:
 *
 * + change to REAL_MATH -Mechanism everywhere; 
 **********************************************************************/
#define PMO_VERSION_STRING "1.0"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* #include "common.h" */
/* #include "kyrill.h" */

/* #include "encoder.h" */
#include "PsychoMpegOne.h"
/* #include "real_math.h" */
/* #include "dft.h" */



/* 
 * write some intermediate signals to file (*_WRITE*) or stdout (*_PRINT*)
 */ 

/* LTG: global MT, internal resolution (PMO_sub_size values): */
/* #define PMODEF_WRITELTG */
/* #define PMODEF_WRITELTMIN */         /* global MT, warped to fftsize/2 */
/* #define PMODEF_WRITELTMIN_OLD      /* minimum MT, MPEG's 32-freq.band resolution */
/* #define PMODEF_WRITESPIKE          /* "signal" := sum(); MPEG's 32-fb resol. */
#define PMODEF_WRITESPECTRUM_EFF   /* resolution: fftsize/2 */
#define PMODEF_WRITEMASKING_EFF /* resolution: fftsize/2 */

/* #define PMODEF_PRINTCBOUNDVALUES */ /* for info; for debugging */
/* #define PMODEF_PRINTLTGVALUES */ /* for info; defines it's own table */
/* #define PMODEF_PRINTSAMPLE */ /* for debugging purposes */
/* #define PMODEF_READSAMPLE   /* for test purposes */


/*
 * Important switches: 
 * These switches define the exact algorithm of the psychoacoustic
 * calculation to be carried out by this generalized implementation.
 * Choose wisely.
 *
 * KAF
 */
#define PMODEF_CONSIDER_HEAR     /* (or do not consider the Abs.HearThr. at all) */ 
/* #define PMODEF_USE_TABLE */   /* (or use analytic approx. otherwise) */
/* #define PMODEF_USE_MPEG_TABLERANGE /* or use full range of spectrum for def of tables */
/* #define PMODEF_USE_CONSTHEAR */ /* use LTG_HEAR=const for all frequencies */
/* #define PMODEF_NORM_MT */     /* (or use un-norme'd sums as in orig. MPEG) */
#define PMODEF_NOISE_MAXPOS     /* noise-components position := pos(max(power)) within 1 bark)


/*
 * switches used in the MPEG implementation:
 */
#define LONDON                  /* enable "LONDON" modification */
#define MAKE_SENSE              /* enable "MAKE_SENSE" modification */
#define MI_OPTION               /* enable "MI_OPTION" modification */
/**********************************************************************
*
*        This module implements the psychoacoustic model I for the
* MPEG encoder layer II. It uses simplified tonal and noise masking
* threshold analysis to generate SMR for the encoder bit allocation
* routine.
*
**********************************************************************/


/* changed to static; KAF */
static long *cbound;
static long PMO_crit_band;   /* used to be `crit_band' */

static long PMO_fft_size;    /* used to be FFT_SIZE */
static long PMO_han_size;    /* used to be HAN_SIZE */
static long PMO_sub_size;    /* used to be sub_size */
static long PMO_fs;          /* used to be given via info->lay */
static long PMO_fs_idx;      /* used to be given via info->lay */
static long PMO_sblimit;     /* used to be `sblimit' */
static long PMO_sblimit_old; /* same value as MPEG's `sblimit' */

static long PMO_12, PMO_2, PMO_3, PMO_6;  /* generalizedd ranges 
					     in `tonal_label()' */




/**************************************************************************
 *
 * some supplementary functions; not part of MPEG/Audio
 *
**************************************************************************/

/*********************************************************************
ld - base 2 logarithm

Returns base 2 log such that i = 2**ans where ans = ld(i).
if ld(i) is between two values, the larger is returned.

long ld(unsigned long x)
*********************************************************************/

extern long ld(unsigned long x)
{
    unsigned long bitmask,i;

    if(x == 0) return(-1);      /* zero is an error, return -1 */

    x--;                        /* get the max index, x-1 */

    for(bitmask = 1 , i = 0 ; ; bitmask *= 2 , i++) {
        if(x == 0) return(i);      /* return log2 if all zero */
        x = x & (~bitmask);        /* AND off a bit */
    }
}


/*********************************************************************
fft - In-place radix 2 decimation in time FFT

Requires pointer to complex array, x and power of 2 size of FFT, m
(size of FFT = 2**m).  Places FFT output on top of input COMPLEX array.

void fft(COMPLEX *x, long m)
*********************************************************************/

void fft(x,m)
    COMPLEX *x;
    long m;
{
    static COMPLEX *w;           /* used to store the w complex array */
    static long mstore = 0;       /* stores m for future reference */
    static long n = 1;            /* length of fft stored for future */

    COMPLEX u,temp,tm;
    COMPLEX *xi,*xip,*xj,*wptr;

    long i,j,k,l,le,windex;

    double arg,w_real,w_imag,wrecur_real,wrecur_imag,wtemp_real;

    if(m != mstore) {

/* free previously allocated storage and set new m */

        if(mstore != 0) free(w);
        mstore = m;
        if(m == 0) return;       /* if m=0 then done */

/* n = 2**m = fft length */

        n = 1 << m;
        le = n/2;

/* allocate the storage for w */

        w = (COMPLEX *) calloc(le-1,sizeof(COMPLEX));
        if(!w) {
            printf("\nUnable to allocate complex W array\n");
            exit(1);
        }

/* calculate the w values recursively */

        arg = 4.0*atan(1.0)/le;         /* PI/le calculation */
        wrecur_real = w_real = cos(arg);
        wrecur_imag = w_imag = -sin(arg);
        xj = w;
        for (j = 1 ; j < le ; j++) {
            xj->real = (double)wrecur_real;
            xj->imag = (double)wrecur_imag;
            xj++;
            wtemp_real = wrecur_real*w_real - wrecur_imag*w_imag;
            wrecur_imag = wrecur_real*w_imag + wrecur_imag*w_real;
            wrecur_real = wtemp_real;
        }
    }

/* start fft */

    le = n;
    windex = 1;
    for (l = 0 ; l < m ; l++) {
        le = le/2;

/* first iteration with no multiplies */

        for(i = 0 ; i < n ; i = i + 2*le) {
            xi = x + i;
            xip = xi + le;
            temp.real = xi->real + xip->real;
            temp.imag = xi->imag + xip->imag;
            xip->real = xi->real - xip->real;
            xip->imag = xi->imag - xip->imag;
            *xi = temp;
        }

/* remaining iterations use stored w */

        wptr = w + windex - 1;
        for (j = 1 ; j < le ; j++) {
            u = *wptr;
            for (i = j ; i < n ; i = i + 2*le) {
                xi = x + i;
                xip = xi + le;
                temp.real = xi->real + xip->real;
                temp.imag = xi->imag + xip->imag;
                tm.real = xi->real - xip->real;
                tm.imag = xi->imag - xip->imag;             
                xip->real = tm.real*u.real - tm.imag*u.imag;
                xip->imag = tm.real*u.imag + tm.imag*u.real;
                *xi = temp;
            }
            wptr = wptr + windex;
        }
        windex = 2*windex;
    }            

/* rearrange data by bit reversing */

    j = 0;
    for (i = 1 ; i < (n-1) ; i++) {
        k = n/2;
        while(k <= j) {
            j = j - k;
            k = k/2;
        }
        j = j + k;
        if (i < j) {
            xi = x + i;
            xj = x + j;
            temp = *xj;
            *xj = *xi;
            *xi = temp;
        }
    }
}




/************************************************************
rfft - trig recombination real input FFT

Requires real array pointed to by x, pointer to complex
output array, y and the size of real FFT in power of
2 notation, m (size of input array and FFT, N = 2**m).
On completion, the COMPLEX array pointed to by y 
contains the lower N/2 + 1 elements of the spectrum.

void rfft(double *x, COMPLEX *y, long m)
***************************************************************/

void rfft(x,y,m)
    double      *x;
    COMPLEX    *y;
    long        m;
{
    static    COMPLEX  *cf;
    static    long      mstore = 0;
    long       p,num,k,index;
    double     Realsum, Realdif, Imagsum, Imagdif;
    double    factor, arg;
    COMPLEX   *ck, *xk, *xnk, *cx;

/* First call the fft routine using the x array but with
   half the size of the real fft */

    p = m - 1;
    cx = (COMPLEX *) x;
    fft(cx,p);

/* Next create the coefficients for recombination, if required */

    num = 1 << p;    /* num is half the real sequence length.  */

    if (m!=mstore){
      if (mstore != 0) free(cf);
      cf = (COMPLEX *) calloc(num - 1,sizeof(COMPLEX));
      if(!cf){
        printf("\nUnable to allocate trig recomb coefficients.");
        exit(1);
      }

      mstore = m;
      factor = 4.0*atan(1.0)/num;
      for (k = 1; k < num; k++){
        arg = factor*k;
        cf[k-1].real = (double)cos(arg);
        cf[k-1].imag = (double)sin(arg);
      }
    }  

/* DC component, no multiplies */
    y[0].real = cx[0].real + cx[0].imag;
    y[0].imag = 0.0;

/* other frequencies by trig recombination */
    ck = cf;
    xk = cx + 1;
    xnk = cx + num - 1;
    for (k = 1; k < num; k++){
      Realsum = ( xk->real + xnk->real ) / 2;
      Imagsum = ( xk->imag + xnk->imag ) / 2;
      Realdif = ( xk->real - xnk->real ) / 2;
      Imagdif = ( xk->imag - xnk->imag ) / 2;

      y[k].real = Realsum + ck->real * Imagsum
                          - ck->imag * Realdif ;

      y[k].imag = Imagdif - ck->imag * Imagsum
                          - ck->real * Realdif ;
      ck++;
      xk++;
      xnk--;
    }
}





/**************************************************************************
 *
 * End of the supplementary functions
 *
**************************************************************************/



/*******************************************************************************
*
*  Allocate number of bytes of memory equal to "block".
*
*******************************************************************************/

void  *mem_alloc(unsigned long block, char *item)
{

    void    *ptr;

#if ! defined (MACINTOSH) && ! defined (MSC60)
    ptr = (void  *) malloc(block);
#endif

    if (ptr == NULL){
        printf("Unable to allocate %s\n", item);
        exit(0);
    }
    return(ptr);
}


/****************************************************************************
*
*  Free memory pointed to by "*ptr_addr".
*
*****************************************************************************/

void    mem_free(ptr_addr)
    void    **ptr_addr;
{

    if (*ptr_addr != NULL){
        free(*ptr_addr);
        *ptr_addr = NULL;
    }

}


FILE *OpenTableFile(name)
    char *name;
{
    char fulname[80];
    char *envdir;
    FILE *f;
    
    fulname[0] = '\0';

#define TABLES_PATH "/u/kyrill/PD-Codecs/MPEG/dist07/lsf/encoder/tables/"
    
#ifdef TABLES_PATH
    strcpy(fulname, TABLES_PATH);   /* default relative path for tables */
#endif /* TABLES_PATH */          /* (includes terminal path seperator */
    
/* #ifdef UNIX                        */
/* { */
/*     char *getenv(); */
    
/*     envdir = getenv(MPEGTABENV);    check for environment */
/*     if(envdir != NULL) */
/* 	strcpy(fulname, envdir); */
/*     strcat(fulname, PATH_SEPARATOR);   add a "/" on the end */
/* } */
/* #endif  UNIX */

    strcat(fulname, name);
    if( (f=fopen(fulname,"r"))==NULL ) {
	fprintf(stderr,"OpenTable: could not find %s\n", fulname);
    
#ifdef UNIX
	if(envdir != NULL)
	    fprintf(stderr,"Check %s directory '%s'\n",MPEGTABENV, envdir);
	else
            fprintf(stderr,"Check local directory './%s' or setenv %s\n",
                    TABLES_PATH, MPEGTABENV);
#else /* not unix : no environment variables */

#ifdef TABLES_PATH
	fprintf(stderr,"Check local directory './%s'\n",TABLES_PATH);
#endif /* TABLES_PATH */

#endif /* UNIX */

    }
    return f;
}


/*
 * This Function calculates the Frequency in Hertz given a 
 * Bark-value. It uses the Traunmueller-formula for bark>2
 * and a linear inerpolation below. 
 * KAF
 */
double bark2hz (double bark)
{
    double hz;

    if(bark>2.0)
	hz = 1960 * (bark + 0.53) / (26.28 - bark);
    else
	hz = bark * 102.9;

    return (hz);
}



/*
 * This Function calculates the Frequency in Bark given a 
 * Frequency-value in Hertz. It uses the Traunmueller-formula 
 * for frequency>200Hz and a linear inerpolation below. 
 * KAF
 */
double hz2bark (double hz)
{
    double bark;

    if(hz>200.0)
	bark = 26.81 * hz / (1960 + hz) - 0.53; 
    else
	bark = hz / 102.9;

    return (bark);
}




/***********************************************************************
 * 
 * This function defines the absolute hearing threshold L in quiet
 * (in dB SPL) given a frequency f (in Hertz). 
 *
 * The values are lineary interpolated in the (f [Hz], L [dB])-domain 
 * using a table.
 *
 * KAF
 ***********************************************************************/ 
double hearmask( double f)
{

    static double freq[]={20.0, 25.0, 30.0, 35.0, 40.0, 45.0, 50.0, 55.0, 60.0, 70.0,
			80.0, 90.0, 100.0, 125.0, 150.0, 177.0, 200.0, 250.0, 300.0, 350.0,
			400.0, 450.0, 500.0, 550.0, 600.0, 700.0, 800.0, 900.0, 1000.0, 1500.0,
			2000.0, 2500.0, 2828.0, 3000.0, 3500.0, 4000.0, 4500.0, 5000.0, 5500.0, 6000.0,
			7000.0, 8000.0, 9000.0, 10000.0, 12750.0, 15000, 20000}; 

    static double thres[]={73.4, 65.2, 57.9, 52.7, 48.0, 45.0, 41.9, 39.3, 36.8, 33.0,
			 29.7, 27.1, 25.0, 22.0, 18.2, 16.0, 14.0, 11.4, 9.2, 8.0,
			 6.9, 6.2, 5.7, 5.1, 5.0, 5.0, 4.4, 4.3, 3.9, 2.7,
			 0.9, -1.3, -2.5, -3.2, -4.4, -4.1, -2.5, -0.5, 2.0, 2.5,
			 3.0, 5.0, 8.0, 10.5, 20.0, 40.0, 60.0};

    /* adapted according to Zwicker "Psychoacoustics" and to the tables given 
       in MPEG */

    static long tablen = 47;
    long i;

    if( f<=freq[0]) return (thres[0]);
    if( f>=freq[tablen-1]) return (thres[tablen-1]);
    
    i=0;
    while (f > freq[i]) i++;
    i--;
    
    return ( (f-freq[i])*(thres[i+1]-thres[i])/(freq[i+1]-freq[i]) + thres[i] );
}



/* this function reads in critical */
/* band boundaries                 */
void read_cbound(long lay, long freq)
{
    long i,j,k;
    FILE *fp;
    char r[16], t[80];

    strcpy(r, "2cb1");
    r[0] = (char) lay + '0';
    r[3] = (char) freq + '0';

    printf("read_cbound(): reading tablefile '%s'\n", r); /* KAF */

    if( !(fp = OpenTableFile(r)) ){       /* check boundary values */
	printf("Please check %s boundary table\n",r);
	exit(1);
    }
    fgets(t,80,fp);               /* read input for critical bands */
    sscanf(t,"%ld\n",&PMO_crit_band);
    cbound = (long  *) calloc(PMO_crit_band, sizeof(long));
    /* mem_alloc(sizeof(int) * PMO_crit_band, "cbound"); */

    for(i=0;i<PMO_crit_band;i++){   /* continue to read input for */
	fgets(t,80,fp);            /* critical band boundaries   */
	sscanf(t,"%ld %ld\n",&j, &k);
	if(i==j) cbound[j] = k;
	else {                     /* error */
	    printf("Please check index %ld in cbound table %s\n",i,r);
	    exit(1);
	}
    }
    fclose(fp);
}        


/* this function defines the critical   */
/* band boundaries from equation rather */
/* than from a table.                   */
/* KAF                                  */
void define_cbound(long fs, long last_line)
{
    long i;

    /*
     * define `PMO_crit_band' (number of crit_bands used
     * for covering the given frequency range)
     */
    PMO_crit_band = (long) hz2bark( ((double)fs)/2.0 ) + 1;
 

    /*
     * now define array cbound[]:
     */
    cbound = (long *) calloc(PMO_crit_band, sizeof(long));
    cbound [0] = 1;
   
    for(i=1; i<PMO_crit_band; i++)
    {  
	cbound[i] = bark2hz( (double)i ) * (double)PMO_han_size * 2.0 / (double)fs; 
	if(cbound[i] == cbound[i-1]) cbound[i]++;
    }

    if (cbound[PMO_crit_band-1] > last_line)
    {
	cbound[PMO_crit_band-1] = last_line;
	if (cbound[PMO_crit_band-1]  <= cbound[PMO_crit_band-2])
	{   
	    cbound[PMO_crit_band-1] = cbound[PMO_crit_band-2]+1;
	    printf("define_cbound(): warning: cbounds nearly overlap!\n");
	}
    }
#ifdef PMODEF_PRINTCBOUNDVALUES
    FOR(i, PMO_crit_band)
	printf("cbound[%3d] = %4d\n", i, (int)cbound[i]);
#endif

}


 

void read_freq_band(g_ptr *ltg, long lay, long freq)  /* this function reads in   */
/* long lay, freq;  */                   /* frequency bands and bark */
/* g_ptr  *ltg; */               /* values                   */
{
    long i,j, k;
    double b,c;
    FILE *fp;
    char r[16], t[80];

    strcpy(r, "2th1");
    r[0] = (char) lay + '0';
    r[3] = (char) freq + '0';

    printf("read_freq_band(): reading cband tablefile '%s'\n", r); /* KAF */

    if( !(fp = OpenTableFile(r)) ){   /* check freq. values  */
	printf("Please check frequency and cband table %s\n",r);
	exit(1);
    }
    fgets(t,80,fp);              /* read input for freq. subbands */
    sscanf(t,"%ld\n",&PMO_sub_size);
    *ltg = (g_ptr  ) malloc(sizeof(g_thres) * PMO_sub_size);
    /* mem_alloc(sizeof(g_thres) * PMO_sub_size, "ltg"); */

    (*ltg)[0].line = 0;          /* initialize global masking threshold */
    (*ltg)[0].bark = 0;
    (*ltg)[0].hear = 0;
    for(i=1;i<PMO_sub_size;i++){    /* continue to read freq. subband */
	fgets(t,80,fp);          /* and assign                     */
	sscanf(t,"%ld %ld %lf %lf\n",&j, &k, &b, &c);
	if(i == j){
	    (*ltg)[j].line = k;
	    (*ltg)[j].bark = b;
	    (*ltg)[j].hear = c;
	}
	else {                   /* error */
	    printf("Please check index %ld in freq-cb table %s\n",i,r);
	    exit(1);
	}
    }
    fclose(fp);
}




/*
 * This function calculates the values of `ltg[]' rather than reading
 * it from a table. 
 *
 * The "rule" used in the tables seems to be a piecewise constant stepsize
 * for ltg[].line-Array: 
 *
 * Delta-line = {1, 2, 4, 8} in the following way:
 *
 *   Delta-line =1 for line <=48; 
 *              =2 for line <=96;
 *              =4 for line <=192; 
 *              =8      until 480 (or 464; depending on fs). 
 *
 * This "rule" is used in this function. 
 * However, in order to be able to deal with power[]-Arrays of size other 
 * than 512 (which is the constant value used in MPEG), this "rule" has 
 * been extended to cover array-sizes being integer multiples or 
 * subdivisions of 512. 
 *
 * For power[]-array-sizes of 128 or below, the resolution has been increased
 * by using only Delta-line = {1, 2}. 
 *
 * The argument "last_line" indicates the value of the last ltg[].line beeing 
 * allowed and usually should be (PMO_han_size-1).
 * In order to compare this function with the current MPEG-implementation,
 * a value of last_line=480 (instaed of 512) has to be used, so it has
 * been provided as a variable. 
 *
 * KAF
 */
void define_freq_band_v2(g_ptr *ltg, long fs, long last_line) 
{
   
    long i, j, k, l;
    double deltaline, faktor;

    static long swidx_1[4] = { 48, 24, 24, 100};
    static long swidx_2[4] = { 48, 100};
   
    *ltg = (g_ptr) malloc(sizeof(g_thres) * PMO_SUB_SIZE_MAX);
   
    (*ltg)[0].line = 0;          /* initialize global masking threshold */
    (*ltg)[0].bark = 0;
    (*ltg)[0].hear = 0;

    if(PMO_han_size >= 256)
    {
	faktor = (double)(PMO_han_size) /512.0;
	l = 1;
	deltaline = 1;
	for(k=0; k<4; k++)
	{  
	    for(j=1; j<= (swidx_1[k]*faktor); j++)
	    {  
		(*ltg)[l].line = (*ltg)[l-1].line + deltaline;
		if( (*ltg)[l].line > last_line)
		{  
		    PMO_sub_size = l;
		    k=4; 
		    break;
		}
		l++;
		if(l == PMO_SUB_SIZE_MAX)
		{   
		    printf("define_freq_band_v2(): array ltg[] too short!\n");
		    exit(-1);
		}
	    }
	    deltaline *= 2;
	}
    }
    else /* use finer resolution for ltg[].line: */
    {
	faktor = (double)(PMO_han_size) /48.0;
	l = 1;
	deltaline = 1;
	for(k=0; k<2; k++)
	{  
	    for(j=0; j<swidx_2[k]*faktor; j++)
	    {  
		(*ltg)[l].line = (*ltg)[l-1].line + deltaline;
		if( (*ltg)[l].line >= PMO_han_size)
		{  
		    PMO_sub_size = l;
		    k=2; 
		    break;
		}
		l++;
		if(l == PMO_SUB_SIZE_MAX)
		{   
		    printf("define_freq_band_v2(): array ltg[] too short!\n");
		    exit(-1);
		}
	    }
	    deltaline *= 2;
	}
    }
    /* printf("define_freq_band_v2(): PMO_sub_size = %ld\n", PMO_sub_size);  */ 

    /*
     * Now define the corresponding values of 
     *     ltg[].bark and 
     *     ltg[].hear: 
     */ 
    for(i=1; i< PMO_sub_size; i++)
    {   
	(*ltg)[i].bark = hz2bark ( (double)(*ltg)[i].line * fs / (2.0 * PMO_han_size) );
#ifndef PMODEF_USE_CONSTHEAR	
	(*ltg)[i].hear = hearmask( (*ltg)[i].line * fs / (2.0 * PMO_han_size) );
#else
	(*ltg)[i].hear = LTG_HEAR;
#endif  /* PMODEF_USE_CONSTHEAR	*/

#ifdef PMODEF_PRINTLTGVALUES
	printf("%ld %ld %lf %lf\n", 
	       i, (*ltg)[i].line, (*ltg)[i].bark, (*ltg)[i].hear);
#endif /* PMODEF_PRINTLTGVALUES */
    }
}


void define_freq_band(g_ptr *ltg, long fs) 
{
    long i, last_line_eff;
    double deltabark, bark_tmp; 
   
    /*
     * determine PMO_sub_size
     * (this value can be changed when reasonable behaviour of 
     * this function is ensured) 
     */
    PMO_sub_size = 133;       /* can have other values, too */
 
    /*
     * define ltg[]-Array now:
     */
    *ltg = (g_ptr) malloc(sizeof(g_thres) * PMO_sub_size);

    (*ltg)[0].line = 0;          /* initialize global masking threshold */
    (*ltg)[0].bark = 0;
    (*ltg)[0].hear = 0;

    /* FIXME: 
       Frage:   warum ltg[].line in den tabellen immer nur bis 480 von 512 ? 
       Antwort: wegen der Delta-line = const-regelung ist man mit sub_size=133
       dann erst bei 480 (bzw. 494) 
       FRAGE: Warum aber dann sub_size=133 ?
	       
	      
       bessere
       Antwort: Die Tabellen sollen nur den Bereich bis zu sblimit/SBLIMIT, 
       also 30/32 der Frequenzachse abdecken. Das entspricht dann nur dem
       Wert von ungefaehr 480/512. Und der genaue Wert 480 folgt aus der
       Tatsache, dass ltg[132].line>>4 == sblimit, also gerade 480>>4 == 30
       sein sollte (in II_minimum_mask()). 

       Gesamtantwort: 
       Vorgegeben ist: 
       1:  Abdecken von 30/32 der Frequenzachse  (durch sblimit)
       2:  Konstruktionsverfahren fuer ltg[].line (mit abschnittsweise
       konstanter Schrittweite). 
       Daraus folgt dann:
       1: sub_size = 133 Abschnitte fuer ltg[]; 
       2: der letzte wert ltg[132].line = 480; 

       Problem: dieser Ansatz ist nur bedingt verallgemeinerbar. 


       ---> eventuell hier auch beruecksichtigen! han_size - 10% oder so. */


    /* in tables: last_line = 480 for HAN_SIZE=512: */
    /* now, ltg[PMO_sub_size-1] gets line(fs*480/512): */
    deltabark = hz2bark(((double)fs *(480.0/512.0))/2.0)  / (double)(PMO_sub_size-1);
 

    for(i=1;i<PMO_sub_size;i++)
    {  
	(*ltg)[i].bark = bark_tmp = i * deltabark; 
	(*ltg)[i].line = 2.0 * bark2hz(bark_tmp) * (double)PMO_han_size / (double)fs;
	if((*ltg)[i].line == (*ltg)[i-1].line) (*ltg)[i].line++;
	(*ltg)[i].hear = LTG_HEAR;  /* may be replaced by an analytic expression for 
				       the hearing threshold in quiet. 
				       KAF */
#ifdef PMODEF_PRINTLTGVALUES
	printf("%ld %ld %lf %lf\n", 
	       i, (*ltg)[i].line, (*ltg)[i].bark, (*ltg)[i].hear);
#endif
    }
}




/*******************************************************************
*
*        This function finds the maximum spectral component in each
* subband and return them to the encoder for time-domain threshold
* determination.
*
*******************************************************************/
#ifndef LONDON
void II_pick_max(power, spike)
    double  spike[SBLIMIT];
    mask  power[HAN_SIZE];
{
    double max;
    int i,j;

    for(i=0;i<HAN_SIZE;spike[i>>4] = max, i+=16)      /* calculate the      */
	for(j=0, max = DBMIN;j<16;j++)                    /* maximum spectral   */
	    max = (max>power[i+j].x) ? max : power[i+j].x; /* component in each  */
}                                                  /* subband from bound */
/* 4-16               */
#else
void II_pick_max(mask power[], double spike[])
{
    double sum;
    int i,j;

    for(i=0;i<PMO_han_size;spike[i>>4] = 10.0*log10(sum), i+=16)
	/* calculate the      */
	for(j=0, sum = pow(10.0,0.1*DBMIN);j<16;j++)      /* sum of spectral   */
	    sum += pow(10.0,0.1*power[i+j].x);              /* component in each  */
}                                                  /* subband from bound */
                                                   /* 4-16               */
#endif



/***********************************************************************
 *
 * Calculate spectrum_eff, the effective "signal" for SMR calculation. 
 * This is done by calculating the mean() over a range of spectrum_range 
 * values of power[].x
 * PMO_han_size/spekrum_range has to be (int). 
 *
 * KAF
 **********************************************************************/
void calc_spectrum_eff(mask power[], double spectrum_eff[], long spectrum_range)
{
    long i, j;
    double sum_tmp, meanval;

    for(i=0; i<PMO_han_size; i+= spectrum_range)
    {
	sum_tmp = 0.0;
	for(j=0; j<spectrum_range; j++)  
	    sum_tmp += pow(10.0, power[i+j].x/10.0);
	meanval = 10.0 * log10 (sum_tmp/(double)spectrum_range);
	for(j=0; j<spectrum_range; j++)
	    spectrum_eff[i+j] = meanval;
    }
}





/***********************************************************************
 *
 * Define power[].map from ltg[].line:
 *
 **********************************************************************/
void make_map(mask *power, g_thres *ltg)
{
    long i,j, k;

    for(i=1;i<PMO_sub_size;i++) 
    {  for(j=ltg[i-1].line;j<=ltg[i].line;j++)
    {  
	power[j].map = i;
    }
    }
    /* fill end too: (necessary for remap_mask())  KAF */
    for(k=ltg[PMO_sub_size-1].line; k<PMO_han_size; k++)
    {  
	power[k].map = PMO_sub_size-1;
    }
/*
    FOR(i,PMO_han_size)  printf(" %ld %ld\n", i, power[i].map);
*/
}

sh 

/***********************************************************************
 *
 * Addition im linearen Leistungsbereich
 *
 **********************************************************************/
double add_db(a,b)  
    double a,b;
{
    a = pow(10.0,a/10.0);
    b = pow(10.0,b/10.0);
    return 10 * log10(a+b);
}






/****************************************************************
*
*         Window the incoming audio signal.
*
****************************************************************/

void II_hann_win(double sample[])          /* this function calculates a  */
    /* Hann window for PCM (input) */
{                                 /* samples for a 1024-pt. FFT  */
    register long i;
    register double sqrt_8_over_3;
    static long init = 0;
    static double *window;

    if(!init){  /* calculate window function for the Fourier transform */
	window = (double *) malloc(sizeof(double) * PMO_fft_size);
	/*mem_alloc(sizeof(double) * PMO_fft_size, "window"); */

	sqrt_8_over_3 = pow(8.0/3.0, 0.5);
	FOR(i, PMO_fft_size){
	    /* Hann window formula */
	    window[i]=sqrt_8_over_3*0.5*(1-cos(2.0*PI*i/((double)PMO_fft_size)))/(double)PMO_fft_size;
	}
	init = 1;
    }
    FOR(i, PMO_fft_size)
	sample[i] *= window[i];
}


/****************************************************************
*
*        This function labels the tonal component in the power
* spectrum.
*
* adapted. KAF
****************************************************************/

void II_tonal_label(mask *power, long *tone)  
    /* this function extracts (tonal) */
    /* sinusoidals from the spectrum  */
    /* int *tone;  */
{
    long i,j, last = LAST, first, run, last_but_one = LAST; /* dpwe */
    double max;

    *tone = LAST;
    for(i=2;i<PMO_han_size-PMO_12;i++){    /* "-12" fuer PMO_han_size=512; mitskalieren */
	if(power[i].x>power[i-1].x && power[i].x>=power[i+1].x){
	    power[i].type = TONE;
	    power[i].next = LAST;
	    if(last != LAST) power[last].next = i;
	    else first = *tone = i;
	    last = i;
	}
    }
    last = LAST;
    first = *tone;
    *tone = LAST;
    while(first != LAST){               /* the conditions for the tonal          */
	if(first<3 || first>(PMO_han_size-PMO_12))
	    run = 0;                      /* otherwise k+/-j will be out of bounds */
	else if(first<(PMO_han_size/8)) run = PMO_2;      /* components in layer II, which         */
	else if(first<(PMO_han_size/4)) run = PMO_3;      /* are the boundaries for calc.          */
	else if(first<(PMO_han_size/2)) run = PMO_6;      /* the tonal components                  */
	else run = PMO_12;

	max = power[first].x - 7;        /* after calculation of tonal   */
	for(j=2;j<=run;j++)              /* components, set to local max */
	    if(max < power[first-j].x || max < power[first+j].x){
		power[first].type = FALSE;
		break;
	    }
	if(power[first].type == TONE){   /* extract tonal components */
	    long help=first;
	    if(*tone==LAST) *tone = first;
	    while((power[help].next!=LAST)&&(power[help].next-first)<=run)
		help=power[help].next;
	    help=power[help].next;
	    power[first].next=help;
	    if((first-last)<=run){
		if(last_but_one != LAST) power[last_but_one].next=first;
	    }
#ifndef PMODEF_NORM_MT
	    if(first>1 && first<(PMO_han_size-PMO_12)){     /* calculate the sum of the */
		double tmp;                /* powers of the components */
		tmp = add_db(power[first-1].x, power[first+1].x);
		power[first].x = add_db(power[first].x, tmp);
	    }
#else
	    if(first>1 && first<(PMO_han_size-PMO_12)){     /* calculate the __mean__ of the */
		double tmp;                              /* powers of the components */
		tmp  = pow(10.0,power[first-1].x/10.0);
		tmp += pow(10.0,power[first].x/10.0);
		tmp += pow(10.0,power[first+1].x/10.0);	  
		power[first].x = 10.0 * log10(tmp/3.0);
	    }
#endif /* PMODEF_NORM_MT */

	    for(j=1;j<=run;j++){
		power[first-j].x = power[first+j].x = DBMIN;
		power[first-j].next = power[first+j].next = STOP;
		power[first-j].type = power[first+j].type = FALSE;
	    }
	    last_but_one=last;
	    last = first;
	    first = power[first].next;
	}
	else {
	    long ll;
	    if(last == LAST); /* *tone = power[first].next; dpwe */
	    else power[last].next = power[first].next;
	    ll = first;
	    first = power[first].next;
	    power[ll].next = STOP;
	}
    }
}

/****************************************************************
*
*        This function groups all the remaining non-tonal
* spectral lines into critical band where they are replaced by
* one single line.
*
* adapted. KAF
****************************************************************/
        
void noise_label(mask *power, long *noise, g_thres *ltg)
    /* g_thres  *ltg; */
    /* mask  *power; */
    /* int *noise; */
{
    long i,j, centre, last = LAST;
    double index, weight, sum;
    double tmp_kaf;                       /* used for PMODEF_NORM_MT */
    long   anz_kaf;                       /* used for PMODEF_NORM_MT */
    long   max_pos_kaf;			  /* used for PMODEF_NOISE_MAXPOS */
    double max_val_kaf;			  /* used for PMODEF_NOISE_MAXPOS */

    /* calculate the remaining spectral */
    for(i=0;i<PMO_crit_band-1;i++){  /* lines for non-tonal components   */
	for(j=cbound[i],
	      weight = 0.0,
	      sum = DBMIN,
	      tmp_kaf=0.0,
	      anz_kaf=0,
	      max_val_kaf=DBMIN-1,
	      max_pos_kaf=cbound[i]; j<cbound[i+1]; j++){
	    if(power[j].type != TONE){
		if(power[j].x != DBMIN){
#ifdef PMODEF_NOISE_MAXPOS
		  if(power[j].x > max_val_kaf) {
		    max_val_kaf = power[j].x; 
		    max_pos_kaf = j; 
		  }
#endif /* PMODEF_NOISE_MAXPOS */
#ifndef PMODEF_NORM_MT
		    sum = add_db(power[j].x,sum);
#else
		    tmp_kaf +=  pow(10.0, power[j].x/10.0);
		    anz_kaf++;
		    sum      = 10.0 * log10(tmp_kaf/(double)anz_kaf);
#endif /* PMODEF_NORM_MT */

		    /* the line below and others under the "MAKE_SENSE" condition are an alternate
		       interpretation of "geometric mean". This approach may make more sense but
		       it has not been tested with hardware. */
#ifdef MAKE_SENSE
		    /* weight += pow(10.0, power[j].x/10.0) * (ltg[power[j].map].bark-i);
		       bad code [SS] 21-1-93
		       */
		    weight += pow(10.0,power[j].x/10.0) * (double) (j-cbound[i]) /
			(double) (cbound[i+1]-cbound[i]);  /* correction */
#endif
		    power[j].x = DBMIN;
		}
	    }   /*  check to see if the spectral line is low dB, and if  */
	}      /* so replace the center of the critical band, which is */
	/* the center freq. of the noise component              */

#ifdef MAKE_SENSE
	if(sum <= DBMIN){
	  centre = (cbound[i+1]+cbound[i]) /2;
	  /* printf("noise_label(): (sum <= DBMIN) ---> centre = %ld\n", centre); */
	  /* This condition is true about one (or two) times per spectrum. KAF */
	}
	else {
#ifndef PMODEF_NORM_MT
	    index = weight/pow(10.0,sum/10.0);
#else
	    index = weight/(pow(10.0,sum/10.0) * (double)(cbound[i+1]-cbound[i]));
#endif /* PMODEF_NORM_MT */
	    centre = cbound[i] + (long) (index * (double) (cbound[i+1]-cbound[i]) );
#ifdef PMODEF_NOISE_MAXPOS
	    centre = max_pos_kaf; 
#endif /* PMODEF_NOISE_MAXPOS */
	} 
#else
	index = (double)( ((double)cbound[i]) * ((double)(cbound[i+1]-1)) );
	centre = (long)(pow(index,0.5)+0.5);
#endif /* MAKE_SENSE */

	/* locate next non-tonal component until finished; */
	/* add to list of non-tonal components             */
#ifdef MI_OPTION
	/* Masahiro Iwadare's fix for infinite looping problem? */
	if(power[centre].type == TONE) 
	    if (power[centre+1].type == TONE) centre++; else centre--;
#else
	/* Mike Li's fix for infinite looping problem */
	if(power[centre].type == FALSE) centre++;

#ifdef PMODEF_CONSIDER_HEAR
	/* Consider: this code is exec'd only iff MI_OPTION is OFF (!). 
	   Since the index power[i] (not j) seems strange, this code does not 
	   seem to be ok. Be happy that MI_OPTION usually is turned ON. 
	   KAF */
	if(power[centre].type == NOISE){
	    if(power[centre].x >= ltg[power[i].map].hear){
		if(sum >= ltg[power[i].map].hear) sum = add_db(power[j].x,sum);
		else
		    sum = power[centre].x;
	    }
	}
#else
	if(power[centre].type == NOISE){ sum = add_db(power[j].x,sum); }
#endif /* PMODEF_CONSIDER_HEAR */
#endif /* MI_OPTION */

	if(last == LAST) *noise = centre;
	else {
	    power[centre].next = LAST;
	    power[last].next = centre;
	}
	power[centre].x = sum;
	power[centre].type = NOISE;        
	last = centre;
    }        
}

/****************************************************************
*
*        This function reduces the number of noise and tonal
* component for further threshold analysis.
*
* adapted. KAF
****************************************************************/

void subsampling(mask *power, g_thres *ltg, long *tone, long *noise)
    /* mask  power[HAN_SIZE]; */
    /* g_thres  *ltg; */
    /* int *tone, *noise; */
{
 
    long i, old;


#ifdef PMODEF_CONSIDER_HEAR
    i = *tone; old = STOP;    /* calculate tonal components for */
    while(i!=LAST){           /* reduction of spectral lines    */
	if(power[i].x < ltg[power[i].map].hear){
	    power[i].type = FALSE;
	    power[i].x = DBMIN;
	    if(old == STOP) {
		*tone = (i = power[i].next);
		if (i==STOP) *tone= (i=LAST);         /* KAF */
	    }
	    else  
		power[old].next = (i = power[i].next);
	}
	else 
	{
	    old = i;
	    i = power[i].next;

	}
    }

    i = *noise; old = STOP;    /* calculate non-tonal components for */
    while(i!=LAST){            /* reduction of spectral lines        */
	if(power[i].x < ltg[power[i].map].hear){
	    power[i].type = FALSE;
	    power[i].x = DBMIN;
	    if(old == STOP){
		*noise = (i = power[i].next);
		if (i==STOP) *noise= (i=LAST);         /* KAF */
	    }
	    else   power[old].next = (i = power[i].next);
	}
	else 
	{
	    old = i;
	    i = power[i].next;
	}
    }
#endif /* PMODEF_CONSIDER_HEAR */



    i = *tone; old = STOP;
    while(i != LAST){                              /* if more than one */
	if(power[i].next == LAST)break;             /* tonal component  */
	if(ltg[power[power[i].next].map].bark -     /* is less than .5  */
	   ltg[power[i].map].bark < 0.5) {          /* bark, take the   */
	    if(power[power[i].next].x > power[i].x ){/* maximum          */
		if(old == STOP) *tone = power[i].next;
		else power[old].next = power[i].next;
		power[i].type = FALSE;
		power[i].x = DBMIN;
		i = power[i].next;
	    }
	    else {
		power[power[i].next].type = FALSE;
		power[power[i].next].x = DBMIN;
		power[i].next = power[power[i].next].next;
		old = i;
	    }
	}
	else {
	    old = i;
	    i = power[i].next;
	}
    }
}

/****************************************************************
*
*        This function calculates the individual threshold and
* sum with the quiet threshold to find the global threshold.
*
* adapted. KAF
****************************************************************/

void threshold(mask *power, g_thres *ltg, long *tone, long *noise)
    /*mask  power[HAN_SIZE]; */
    /* g_thres  *ltg; */
    /*int *tone, *noise, bit_rate; */
{
    long k, t;
    double dz, tmps, vf;

    for(k=1;k<PMO_sub_size;k++){
	ltg[k].x = DBMIN;
	t = *tone;          /* calculate individual masking threshold for */
	while(t != LAST){   /* components in order to find the global     */
	    if(ltg[k].bark-ltg[power[t].map].bark >= -3.0 && /*threshold (LTG)*/
	       ltg[k].bark-ltg[power[t].map].bark <8.0){

		dz = ltg[k].bark-ltg[power[t].map].bark; /* distance of bark value*/

		tmps = -1.525-0.275*ltg[power[t].map].bark - 4.5 + power[t].x;

		/* masking function for lower & upper slopes */
		if(-3<=dz && dz<-1) vf = 17*(dz+1)-(0.4*power[t].x +6);
		else if(-1<=dz && dz<0) vf = (0.4 *power[t].x + 6) * dz;
		else if(0<=dz && dz<1) vf = (-17*dz);
		else if(1<=dz && dz<8) vf = -(dz-1) * (17-0.15 *power[t].x) - 17;
		tmps += vf;        
		ltg[k].x = add_db(ltg[k].x, tmps);
	    }
	    t = power[t].next;
	}
   
	t = *noise;        /* calculate individual masking threshold  */
	while(t != LAST){  /* for non-tonal components to find LTG    */
	    if(ltg[k].bark-ltg[power[t].map].bark >= -3.0 &&
	       ltg[k].bark-ltg[power[t].map].bark <8.0){

		dz = ltg[k].bark-ltg[power[t].map].bark; /* distance of bark value */

		tmps = -1.525-0.175*ltg[power[t].map].bark -0.5 + power[t].x;

		/* masking function for lower & upper slopes */
		if(-3<=dz && dz<-1) vf = 17*(dz+1)-(0.4*power[t].x +6);
		else if(-1<=dz && dz<0) vf = (0.4 *power[t].x + 6) * dz;
		else if(0<=dz && dz<1) vf = (-17*dz);
		else if(1<=dz && dz<8) vf = -(dz-1) * (17-0.15 *power[t].x) - 17;
		tmps += vf;
		ltg[k].x = add_db(ltg[k].x, tmps);
	    }
	    t = power[t].next;
	}
   
#ifdef PMODEF_CONSIDER_HEAR   
	/* consider abs. threshold. 
	   use MPEGs low bit-rate approximation:
          (this is "nearly" MAX(abs, mask)): KAF */ 
	ltg[k].x = add_db(ltg[k].hear, ltg[k].x);

	/* MPEG's bit-rate dependency omitted for simplicity:  KAF
	 *     if(bit_rate<96)
	 *          ltg[k].x = add_db(ltg[k].hear, ltg[k].x);
	 *     else 
	 *          ltg[k].x = add_db(ltg[k].hear-12.0, ltg[k].x);
	 **/
#endif /* PMODEF_CONSIDER_HEAR */

   
    }
}

/****************************************************************
*
*        This function fills the output masking array according 
* to the mapping described by ltg[].line using a 
* "subsampling"-factor given by `range'.
* Therefore, for range==1, the output array has same size and 
* resolution as power[].x -spectrum, or has the MPEG-resolution
* for range==16.
*
* PMO_han_size/range must be (int). 
*
**KAF***********************************************************/
void  remap_mask(g_thres *ltg, double *zielfeld, long range)
{
    long     i, j, k, lu, lo; 
    double   min_tmp;

    j =1;
    for(i=0; i< PMO_han_size/range; i++)
    {
	min_tmp = ltg[j].x;
	lu = i * range;
	lo = (i+1) * range -1;
	/* printf("\n\n zielfeld[%4ld..%4ld] = ", lu, lo); */

	while( (lu <= ltg[j].line) && (ltg[j].line <= lo) )
	{
	    if(min_tmp > ltg[j].x) min_tmp = ltg[j].x;
	    /* printf("%ld ", j); */
	    j++;
	}

	for(k=lu; k<=lo; k++) zielfeld[k] = min_tmp;
    }
}




/****************************************************************
*
*        This function finds the minimum masking threshold and
* return the value to the encoder.
* 
* nicht angepasst fuer PMODEF_CONSIDER_HEAR !
*
****************************************************************/

void II_minimum_mask(g_thres *ltg, double *ltmin, long sblimit)
{
    double min;
    long i,j;

    j=1;
    for(i=0;i<sblimit;i++)
	if(j>=PMO_sub_size-1)                    /* check subband limit, and       */
	    ltmin[i] = ltg[PMO_sub_size-1].hear; /* calculate the minimum masking  */
	else 
	{                              /* level of LTMIN for each subband*/
	    min = ltg[j].x;
	    while(ltg[j].line>>4 == i && j < PMO_sub_size)
	    {
		if(min>ltg[j].x)  min = ltg[j].x;
		j++;
	    }
	    ltmin[i] = min;
	}
}



/***********************************************************************
 * 
 * The MPEG-relation between Signal and Mask is valid for 
 *   S:=  sum(power[].x)  ... (sum over 16 values)
 *   M:=  min(ltg[].x)    ... (min of 16 values)
 *
 * Now, if we want to use
 *   S:=  mean(power[].x) ... (mean over <spectrum_range> values), 
 * we have to consider the transformation 
 *    sum()[16 values] ----> mean() 
 * for M:= ltg[].x, too. So we have to change the level of ltg[].x
 * accordingly, to ensure the same result for   S - M.
 *
 * Since in this case
 *   dB(mean({x}))  = dB(sum({x})/16)
 *                  = dB(sum({x})) - dB(16) 
 *                  = dB(sum({x})) - 12.04dB, 
 *
 * we have to subtract 12.04 dB from each value of ltg[].x in order 
 * to have the same relation between 
 *
 *   mean(power[].x)      <---> ltg[].x_new
 *
 * as before (in MPEG), where we had
 *
 *   sum(power[].x)[16]   <---> ltg[].x_old
 *
 * This is valid for PMO_han_size=512 (MPEG's orginal value). For values
 * other than this, a corresponding dB-offset has to be used (eg. only 
 * 9.0dB for PMO_han_size=256, etc.)
 *
 * KAF 
 **********************************************************************/
void adapt_mask_pegel(g_thres *ltg)
{
    long i;
    double dB_offset;

    /* start with ltg[1].x, because ltg[0].x is 
       intentionally not defined (i.e. left 0)
       in `threshold()' 
       (for(k=1;...) .. ltg[k].x =...) */

    dB_offset = 10.0 * log10 ((double)PMO_han_size / 32.0); 

    for(i=1; i<PMO_sub_size; i++)
	ltg[i].x -= dB_offset; 
}





/***********************************************************************
 *
 * Calculate the _eff SMR using 
 *  spectrum_eff and masking_eff. 
 *
 * KAF
 **********************************************************************/
void calc_smr_eff(double spectrum_eff[], double masking_eff[], double smr_eff[]) 
{
    long i;

    for(i=0; i<PMO_han_size; i++)
	smr_eff[i] = spectrum_eff[i] - masking_eff[i];

}





/****************************************************************
*
*        This procedure calls all the necessary functions to
* complete the psychoacoustic analysis.
*
* adapted. KAF
****************************************************************/


void II_Psycho_One( short *buffer, 
		    long buf_size, 
		    double *ltmin, 
		    double *spectrum, 
		    double *masking_eff, 
		    long masking_range,
		    double *spectrum_eff,
		    long spectrum_range,
		    double *smr_eff)
{
    static char       init = 0;
    static mask_ptr   power;
    static g_ptr      ltg;

    static double    *sample;
    static double    *ltmin_old;
    static double    *spike;
    static COMPLEX   *rfftout;      /* defined in dft.h */
    static long      fft_order;

    double           energy_i; 


    long k,
	i, 
	offs, 
	tone=0, 
	noise=0;
  
    long anz_tonal, anz_noise;




#ifdef PMODEF_WRITELTMIN
#define KAF_LTMINFILENAME "kaf_PMO_ltmin.bin"
    static long kaf_ltmin_write_ft = TRUE;
    static FILE *kaf_ltmin_fp;
#endif

#ifdef PMODEF_WRITELTMIN_OLD
#define KAF_LTMIN_OLDFILENAME "kaf_PMO_ltmin_old.bin"
    static long kaf_ltmin_old_write_ft = TRUE;
    static FILE *kaf_ltmin_old_fp;
#endif

#ifdef PMODEF_WRITELTG
#define KAF_LTGFILENAME "kaf_PMO_ltg.bin"
    static long kaf_ltg_write_ft = TRUE;
    static FILE *kaf_ltg_fp;
#endif

#ifdef PMODEF_WRITESPIKE
#define KAF_SPIKEFILENAME "kaf_PMO_spike.bin"
    static long kaf_spike_write_ft = TRUE;
    static FILE *kaf_spike_fp;
#endif

#ifdef PMODEF_WRITESPECTRUM_EFF
#define KAF_SPECTRUM_EFFFILENAME "kaf_PMO_spectrum_eff.bin"
    static long kaf_spectrum_eff_write_ft = TRUE;
    static FILE *kaf_spectrum_eff_fp;
#endif


#ifdef PMODEF_WRITEMASKING_EFF
#define KAF_MASKING_EFFFILENAME "kaf_PMO_masking_eff.bin"
    static long kaf_masking_eff_write_ft = TRUE;
    static FILE *kaf_masking_eff_fp;
#endif



#ifdef PMODEF_READSAMPLE
    FILE *f;
#endif


    DEBUGL(0,"II_Psycho_One(): start...\n");

    /* call functions for critical boundaries, freq. */
    if(!init){  /* bands, bark values, and mapping */
	DEBUGL(0,"II_Psycho_One(): init: start...\n");

	sample = (double *) malloc(sizeof(double) * PMO_fft_size);
	ltmin_old= (double *) malloc(sizeof(double) * SBLIMIT);     
	spike = (double *) malloc(sizeof(double) * SBLIMIT);     
	power = (mask_ptr) malloc(sizeof(mask) * PMO_han_size);

	rfftout = (COMPLEX *) malloc(sizeof(COMPLEX) * PMO_fft_size);
	fft_order = (long) ld( PMO_fft_size);        /* from dft.c */

#ifdef PMODEF_USE_TABLE     
	read_cbound( 2     /* info->lay */, 6 /* info->sampling_frequency + 4*/ );
	read_freq_band(&ltg, 2 /* info->lay*/ , 6 /*info->sampling_frequency + 4*/ );
#else
#ifdef PMODEF_USE_MPEG_TABLERANGE
	define_cbound( PMO_fs, 480 );  
	define_freq_band_v2( &ltg, PMO_fs, 480 ); 
#else
	define_cbound( PMO_fs, PMO_han_size );  
	define_freq_band_v2( &ltg, PMO_fs, PMO_han_size ); 
#endif /* PMODEF_USE_MPEG_TABLERANGE */
#endif

#ifdef PMODEF_USE_CONSTHEAR
	FOR(i,PMO_sub_size)
	    ltg[i].hear = LTG_HEAR; 
	printf("II_Psycho_One(): using ltg[i].hear = LTG_HEAR (=%f); \n", 
	       (double)LTG_HEAR);
#endif

	make_map(power,ltg);  /* OK, since  ltg[].line is well-defined. */

	init = 1;
	DEBUGL(0,"II_Psycho_One(): init: ...end.\n");
    }



 
    /* zero sample[]: */
    FOR(i, PMO_fft_size) 
	sample[i]=0; 

    DEBUGL(0,"II_Psycho_One(): define centered range of analysis...\n");
    /* define centered range of analysis: */
    if (buf_size > PMO_fft_size) {
	offs = (buf_size-PMO_fft_size) / 2; 
	FOR(i, PMO_fft_size) 
	    sample[i] = (double)buffer[i+offs]/SCALE;
    } else {
	offs = (PMO_fft_size-buf_size)/2; 
	FOR(i, buf_size) 
	    sample[i+offs] = (double)buffer[i]/SCALE;
    }


#ifdef PMODEF_PRINTSAMPLE
    FOR(i,PMO_fft_size)
	printf("%g\n", sample[i]);
#endif

#ifdef PMODEF_READSAMPLE
    if( (f=fopen("./werte.h", "r"))==NULL )
    {   
	printf("II_Psycho_One(): could not open `werte.h'\n");
	exit(-1);
    }
    if(PMO_fft_size != 1024) 
    {
	printf("II_Psycho_One(): error: size of `werte.h' is wrong!");
    }
    printf("II_Psycho_One(): reading sample[] from file `werte.h'... \n");
    FOR(i,PMO_fft_size)
	fscanf(f, "%lg \n", &sample[i]);
    printf("II_Psycho_One(): ... end reading sample[] from file `werte.h'. \n");
    printf("II_Psycho_One(): read sample[PMO_fft_size/2=%d] = %g\n", 
	   PMO_fft_size/2, sample[PMO_fft_size/2]);
    fclose(f);
#endif

    DEBUGL(0,"II_Psycho_One(): call II_hann_win...\n");
    /* call functions for windowing PCM samples,*/
    II_hann_win(sample);        /* location of spectral components in each  */
    for(i=0;i<PMO_han_size;i++) power[i].x = DBMIN;  /*subband with labeling*/


    /*
     * DEBUGL(0,"\n\nII_Psycho_One(): call II_f_f_t_new...\n");
     * II_f_f_t_new(sample, power);                   
     */

    DEBUGL(0,"call rfft()...\n");
    rfft(sample, rfftout, fft_order);	  
   
    FOR(i, PMO_han_size)
	{ 
	    energy_i     = rfftout[i].real * rfftout[i].real 
		+ rfftout[i].imag * rfftout[i].imag;
	    if (energy_i < 1E-20) energy_i = 1E-20;
	    power[i].x = 
		spectrum[i] = 10 * log10( energy_i) + POWERNORM;
	    power[i].next = STOP;
	    power[i].type = FALSE;
	}

    calc_spectrum_eff (power, spectrum_eff, spectrum_range);

#ifdef PMODEF_WRITESPECTRUM_EFF 
    if(kaf_spectrum_eff_write_ft==TRUE) 
    { 
	if ((kaf_spectrum_eff_fp = fopen(KAF_SPECTRUM_EFFFILENAME, "wb")) == NULL)  
	{ 
	    printf("Could not open \"%s\".\n", KAF_SPECTRUM_EFFFILENAME); 
	    exit(1); 
	} 
	kaf_spectrum_eff_write_ft = FALSE; 
    } 
    fwrite (spectrum_eff, sizeof(double), PMO_han_size, kaf_spectrum_eff_fp);
    fflush(kaf_spectrum_eff_fp);
#endif


 

#ifdef PMODEF_WRITESPIKE 
    if(PMO_han_size == HAN_SIZE)
    {  
	DEBUGL(0,"II_Psycho_One(): start II_pick_max() ...\n");
	II_pick_max(power, spike);

	if(kaf_spike_write_ft==TRUE) 
	{ 
	    if ((kaf_spike_fp = fopen(KAF_SPIKEFILENAME, "wb")) == NULL)  
	    { 
		printf("Could not open \"%s\".\n", KAF_SPIKEFILENAME); 
		exit(1); 
	    } 
	    kaf_spike_write_ft = FALSE; 
	} 
	fwrite (spike, sizeof(double), PMO_sblimit_old, kaf_spike_fp);
	fflush(kaf_spike_fp);
    }
    else
    {
	printf("Psycho_One(): can not calc spike[] because PMO_han_size !=512\n");
    }
#endif

    DEBUGL(0,"\nII_Psycho_One(): start II_tonal_label()...\n");
    II_tonal_label( power, &tone);                /*reduce noise & tonal */

    DEBUGL(0,"\nII_Psycho_One(): start II_noise_label()...\n");
    noise_label( power, &noise, ltg);             /*components, find     */

    DEBUGL(0,"\nII_Psycho_One(): start subsampling()...\n");
    subsampling( power, ltg, &tone, &noise);      /*global & minimal     */

    /* 
    printf("nach subsampling: (tonal), <noise> sind:\n");
    FOR(i,PMO_han_size)
	{  if(power[i].type == TONE) printf("(%ld) ", i);
	if(power[i].type == NOISE) printf("<%ld> ", i);
	}
    printf("\n\n");
    */
 	
    DEBUGL(0,"II_Psycho_One(): start threshold()...\n");
    threshold( power, ltg, &tone, &noise);        /*threshold, and sgnl- */
                                                  /*to-mask ratio        */

    DEBUGL(0,"II_Psycho_One(): start adapt_mask_pegel()...\n");
    adapt_mask_pegel(ltg);

#ifdef PMODEF_WRITELTG
    if(kaf_ltg_write_ft==TRUE)
    {
	printf("II_Psycho_One(): PMO_sub_size = %ld\n", PMO_sub_size);		
	if ((kaf_ltg_fp = fopen(KAF_LTGFILENAME, "wb")) == NULL) 
	{
	    printf("Could not open \"%s\".\n", KAF_LTGFILENAME);
	    exit(1);
	}
	kaf_ltg_write_ft = FALSE;
    }
    for(i=0; i<PMO_sub_size; i++)
    {
	fwrite (&(ltg[i].x), sizeof(double), 1, kaf_ltg_fp);
    }
    fflush(kaf_ltg_fp);
#endif

    

    DEBUGL(0,"II_Psycho_One(): start remap_mask...\n");
    /* generalisierung von "II_minimum_mask()" */

    /* range==1: warped copy of ltg[].x: */
    remap_mask ( ltg, &(ltmin[0]), 1);  

    remap_mask ( ltg, masking_eff, masking_range);


#ifdef PMODEF_WRITELTMIN 
    if(kaf_ltmin_write_ft==TRUE) 
    { 
	if ((kaf_ltmin_fp = fopen(KAF_LTMINFILENAME, "wb")) == NULL)  
	{ 
	    printf("Could not open \"%s\".\n", KAF_LTMINFILENAME); 
	    exit(1); 
	} 
	kaf_ltmin_write_ft = FALSE; 
    } 
    fwrite (ltmin, sizeof(double), PMO_han_size, kaf_ltmin_fp);
    fflush(kaf_ltmin_fp);
#endif

#ifdef PMODEF_WRITEMASKING_EFF 
    if(kaf_masking_eff_write_ft==TRUE) 
    { 
	if ((kaf_masking_eff_fp = fopen(KAF_MASKING_EFFFILENAME, "wb")) == NULL)  
	{ 
	    printf("Could not open \"%s\".\n", KAF_MASKING_EFFFILENAME); 
	    exit(1); 
	} 
	kaf_masking_eff_write_ft = FALSE; 
    } 
    fwrite (masking_eff, sizeof(double), PMO_han_size, kaf_masking_eff_fp);
    fflush(kaf_masking_eff_fp);
#endif


    DEBUGL(0,"II_Psycho_One(): start calc_smr_eff()...\n");
    calc_smr_eff(spectrum_eff, masking_eff, smr_eff);


#ifdef PMODEF_WRITELTMIN_OLD 
    if(PMO_han_size == HAN_SIZE)
    {  
	DEBUGL(0,"II_Psycho_One(): start II_minimum_mask ...\n");
	II_minimum_mask(ltg, &ltmin_old[0], PMO_sblimit_old);
	if(kaf_ltmin_old_write_ft==TRUE) 
	{ 
	    if ((kaf_ltmin_old_fp = fopen(KAF_LTMIN_OLDFILENAME, "wb")) == NULL)  
	    { 
		printf("Could not open \"%s\".\n", KAF_LTMIN_OLDFILENAME); 
		exit(1); 
	    } 
	    kaf_ltmin_old_write_ft = FALSE; 
	} 
	fwrite (ltmin_old, sizeof(double), PMO_sblimit_old, kaf_ltmin_old_fp);
	fflush(kaf_ltmin_old_fp);
    } else
    {
	printf("Psycho_One(): can not calc ltmin_old[] because PMO_han_size !=512\n");
    }

#endif

    DEBUGL(0,"II_Psycho_One(): ...end.\n");

} /* ----- end of II_Psycho_One() ------- */













/****************************************************************
 *
 * This function calls the generalized version of the 
 * MPEG Audio Encoder function `II_Psycho_One()'
 *
 * KAF
 ***************************************************************/

long PsychoMpegOne (short buffer[], long bufferlen, 
		    long fs, 
		    long fftsize, 
		    double *spectrum, double *masking,
		    double *spectrum_eff, long spectrum_range, 
		    double *masking_eff, long masking_range,
		    double *smr_eff)
{
    long status, 
	i, 
	fft_order, ld2_tmp;
   
    static long init =0;
    char name[80];


    DEBUGL(0,"PsychoMpegOne(): start...\n");
    status = 0; /* FIXME: change to a reasonable value */

    if(!init)
    {
	/*
	** check input-data for reasonable values:
	**/

	fft_order = ld (fftsize);
	if (pow( 2.0, (double)fft_order) != fftsize)
	{  
	    fftsize = (long)pow( 2.0, (double)fft_order); 
	    printf("PsychoMpegOne(): changing fftsize to fftsize=%ld\n", fftsize);
	}
     
	if( (buffer==NULL) ||
	    (spectrum==NULL) || 
	    (masking==NULL) || 
	    (masking_eff==NULL) ||
	    (spectrum_eff==NULL) )
	{  
	    printf("PsychoMpegOne(): uninitialized pointer in (one of the) input array(s)! \n");
	    exit(-1);
	}
	if ((fs<4000) || (fs>48000))
	{
	    printf("PsychoMpegOne(): illegal value (=%ld) for sampling frequeny fs [Hz]\n",
		   fs);
	    exit(-1);
	}
	ld2_tmp = ld (spectrum_range);
	spectrum_range = pow(2.0, (double)ld2_tmp);
	if( (spectrum_range<1) || (spectrum_range>fftsize/4))
	{
	    printf("PsychoMpegOne(): illegal value (=%ld) for spectrum_range.\n",
		   spectrum_range);
	    exit(-1);
	}
	
	ld2_tmp = ld (masking_range);
	masking_range = pow(2.0, (double)ld2_tmp);
	if( (masking_range<1) || (masking_range>fftsize/4))
	{
	    printf("PsychoMpegOne(): illegal value (=%ld) for masking_range.\n",
		   masking_range);
	    exit(-1);
	}


	/****
	 * report parameters:
	 */
	sprintf(name, "%s", "PsychoMpegOne()");
	printf("%s: \n", name);
	printf("%s: Version %s; compiled at %s, %s \n", 
	       name, PMO_VERSION_STRING, __DATE__, __TIME__);
	printf("%s: Parameter used: \n", name);
	printf("%s:   sampling frequency [Hz]               %6ld\n", name, fs);
	printf("%s:   fft_size [smp]                        %6ld\n", name, fftsize);
	printf("%s:   lines in the Power density spectrum   %6ld\n", name, fftsize/2);
	printf("%s:   spek_range [smp] for mean()-calc      %6ld\n", name, spectrum_range);
	printf("%s:   mask_range [smp] for min()-calc       %6ld\n", name, masking_range);

	/*
	 * report #define's:
	 */
#ifdef PMODEF_CONSIDER_HEAR
	printf("%s:   hearing threshold in quiet will be considered.\n", name);
#ifdef PMODEF_USE_CONSTHEAR
	printf("%s:   hearing threshold in quiet is constant: LTG_HEAR=%f\n", 
	       name, (double)LTG_HEAR);
#else	
	printf("%s:   hearing threshold in quiet is not a constant.\n", name);
#endif
#else	
	printf("%s:   hearing threshold in quiet will NOT be considered.\n", name);
#endif /* PMODEF_CONSIDER_HEAR */	
#ifdef PMODEF_NOISE_MAXPOS
	printf("%s:   pos(noise-component) := pos(max(power)); unlike MPEG! \n", name);
#endif 
#ifdef PMODEF_READSAMPLE
	printf("%s:   input frame samples will be defined using separate file.\n", name);
#endif	

#ifdef PMODEF_USE_TABLE
	printf("%s:   parameter arrays are read from table-file. \n", name);
#else
        printf("%s:   parameter arrays are calculated using formula. \n", name);
#endif	
#ifdef PMODEF_NORM_MT
	printf("%s:   global MT wird mit mean() statt sum() berechnet; Ergebnis ist falsch!\n", name);
#endif   

#ifdef PMODEF_WRITELTG
	printf("%s:   Global MaskThr (ltg[].x) is written to binary file.\n", name);
#endif

#ifdef PMODEF_WRITELTMIN
	printf("%s:   Global MaskThr (warped to fftsize/2) is written to binary file.\n", name);
#endif

#ifdef PMODEF_WRITELTMIN_OLD
	printf("%s:   Min. MaskThr (MPEG 32-chn resol.) is written to binary file.\n", name);
#endif

#ifdef PMODEF_WRITESPIKE
	printf("%s:   signal:=sum(spek) (MPEGs sign est.) is written to binary file.\n", name);
#endif

#ifdef PMODEF_WRITESPECTRUM_EFF
	printf("%s:   spectrum_eff is written to binary file.\n", name);
#endif

#ifdef PMODEF_WRITEMASKING_EFF
	printf("%s:   masking_eff is written to binary file.\n", name);
#endif


	printf("%s: \n", name);
	init++;
    }
    /*
    ** define some variables local to this module "PsychoMpegOne"; 
    ** these variables will be used in several functions; 
    ** the names of these variables start with "PMO_":
    **/
    PMO_fft_size    = fftsize;         /* was  #define FFT_SIZE */
    PMO_han_size    = fftsize/2;       /* was  #define HAN_SIZE */
    PMO_fs          = fs;              /* was  fr_ps->sampling_frequency */
    PMO_sblimit     = PMO_han_size;    /* was  sblimit (read from "alloc_*") */ 
    PMO_sblimit_old = 30;              /* was  sblimit (read from "alloc_*") */ 

    PMO_12 = 12 * PMO_han_size/512;
    PMO_2  =  2 * PMO_han_size/512;
    PMO_3  =  3 * PMO_han_size/512;
    PMO_6  =  6 * PMO_han_size/512;




    /*
    ** Calculate the Masking threshold:
    **/
    DEBUGL(0,"PsychoMpegOne(): calling II_Psycho_One()...\n");
    II_Psycho_One( &(buffer[0]), 
		   bufferlen, 
		   masking, 
		   spectrum, 
		   masking_eff, masking_range, 
		   spectrum_eff, spectrum_range,
		   smr_eff
	); 
    DEBUGL(0,"PsychoMpegOne():...back from II_Psycho_One()...\n");
 

    DEBUGL(0,"PsychoMpegOne(): ...end.\n");
    return status;
}

