#ifndef PSYCHOMPEGONE_DEFINED
#define PSYCHOMPEGONE_DEFINED




#ifndef LOG_TWO
#define LOG_TWO             (0.69314718)
#endif

#ifndef FOR
#define FOR(a,b)    for(a=0;a<(b);a++)
#endif

#define DEBUG_LEVEL  (-1)
#define DEBUGL(l,s)          if((DEBUG_LEVEL)>=(l))printf(s);


#ifndef FALSE
#define FALSE               (1==0)
#endif 

#ifndef TRUE
#define TRUE                (1==1)
#endif 




/* --- from MPEG_Implemenattion: ----- */
/* Psychacoustic Model 1 Definitions */
#define CB_FRACTION     0.33
#define MAX_SNR         1000
#define NOISE           10
#define TONE            20
#define DBMIN           -200.0
#define LAST            -1
#define STOP            -100
#define POWERNORM       90.3090 /* = 20 * log10(32768) to normalize */
                                /* max output power to 96 dB per spec */

#define         SCALE                   32768
#define         MPEGTABENV      "MPEGTABLES"
#define         PATH_SEPARATOR  "/"        /* how to build paths */
#ifndef PI
#define         PI                      (3.14159265358979)
#endif
#define         FFT_SIZE                1024
#define         HAN_SIZE                512
#define         SBLIMIT                 32

/* --- end: from MPEG_Implemenattion -- */

#define PMO_POWERNORM   (POWERNORM)


#define LTG_HEAR        (-20.0)  /* constant threshold in quiet; should be 
				   changed to an analytic approximation 
				   KAF */
#define PMO_SUB_SIZE_MAX (2000)  /* used in define_freq_band_v2() */



typedef struct 
{
  int        line;
  double     bark, 
             hear, 
             x;
} g_thres, *g_ptr;


typedef struct 
{
  double     x;
  int        type, 
             next, 
             map;
} mask, *mask_ptr;


typedef struct {
    double real, imag;
} COMPLEX;


extern void        rfft(double *,COMPLEX *,long);
extern long 	   ld(unsigned long);

extern double      bark2hz (double );/* new */
extern double      hz2bark (double );/* new */

extern void        read_cbound(long, long);
extern void        define_cbound(long , long );/* new */

extern double      hearmask(double f);

extern void        read_freq_band(g_ptr*, long, long);
extern void        define_freq_band_v2(g_ptr*, long, long );/* new */
extern void        define_freq_band(g_ptr*, long);/* new */

extern void        calc_spektrum_eff(mask*, double *, long);/* new */
extern void        make_map(mask*, g_thres*);
extern double      add_db(double, double);
extern void        II_hann_win(double*);
extern void        II_pick_max(mask*, double*);
extern void        II_tonal_label(mask*, long*);
extern void        noise_label(mask*, long*, g_thres*);
extern void        subsampling(mask*, g_thres*, long*, long*);
extern void        threshold(mask*, g_thres*, long*, long*);
/* extern void        remap_mask(mask *, g_thres *, double *);*/
extern void        remap_mask(g_thres *, double *, long);/* new */
extern void        II_minimum_mask(g_thres*, double*, long);
extern void        adapt_mask_pegel(g_thres *);/* new */
extern void        calc_smr_eff(double*, double*, double*); /* new */
extern void        II_Psycho_One( short*, long, double*, double*, 
				  double*, long, double*, long, double*);

long PsychoMpegOne (short buffer[], long bufferlen, 
		    long fs, 
		    long fftsize, 
		    double *spektrum, double *maskierung,
		    double *spektrum_eff, long ,
		    double *, long,
		    double *); /* new */



#endif /* PSYCHOMPEGONE_DEFINED */
