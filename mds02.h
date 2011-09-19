/*
 * header of main program 'mds02'
 * 
 * definitions and params
 *
 */ 
/*
    Copyright (C) 2010,2011  Meinolf Braeutigam

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

				Meinolf Braeutigam, Kannenbaeckerstr. 14
				53395 Rheinbach, GERMANY
				meinolf.braeutigam@arcor.de
*/
/*
 * ---------------------------------------------------------------------------
 *  --- alpha test version mbr20110829 ---
 * ---------------------------------------------------------------------------
 * */
					/* insert one time only */
#ifndef MDS_HDR
#define MDS_HDR 1

#define INTERROG 1
					/* compilation of interrogator chck */
#define BDS_DIRECT 1
					/* direct bds decoding in df20/21
					 * with try and error */


#define P_MAX 100
					/* max. number of usable plane 
					 *  datasets (first dataset with
					 *    index 0 is always empty and
					 *    filled with 'unknown' data */
#define L_TIME 120
					/* lifetime in sec of data in
					 *  shared memory with no update */
#define I_F_NAME	"/dev/ttyACM0"
					/* default path for input data */
#define I_FMT		"PGR"
					/* default input data format
					 *  select "PGR" or "AVR" */
#define I_INIT_PGR	"G-G+"
#define I_TERM_PGR	"G-"
#define I_INIT_AVR	"#43-02\r"
#define I_TERM_AVR	"#43-00\r"
					/* default initialization  and
					 *   termination string
					 *   overridden by commandline param */

#define LOG_F_NAME	"./mds02.log"
					/* log file for status and error
					 * reports */
#define DEF_SF_NAME	"./mds02.dat"
					/* default file for saving data */
#define O_POS_LAT	51.418631
#define O_POS_LON	-0.283287
					/* observer position in degr. */

/* flag for CPR ambiguity (global / local) */

//#define GLOBAL_R 1
					/* if defined ambiguity solving
					 *  uses odd/even position reports
					/* if undefined
					 *  observer position from
					 *  above is used */

/* data for solving surface position ambiguity
 * (surface positions are in the range of 0..90 degree only)
 * 	given data are added to the computed surface position lat lon
 * 	in subroutine 'CPT2LL'
 * */

#define SP_BASELAT	0.
					/* latitude,
					 * 	may be 0. or -90. (for
					 * 	nothern and southern
					 * 	hemisphere) */
#define SP_BASELON	0.
					/* longitude,
					 * 	may be 0., 90., 180., 270. */


/* 
 * normally do not edit anything below this line
 * =========================================================================
 * */
					  
/*
 * -------------------------------------------------------------------------
 * structure for misc data in shared memory
 * -------------------------------------------------------------------------
 * */
struct Misc 
{
   int P_MAX_C;				/* current highest index */
   time_t t_start;			/* time at program start */
   time_t t_flag;			/* marks update of any structure 
					 *  set to -1 on mds02 termination */
					/* for tests and statistics: */ 
   unsigned long int total_p;		/* total received data packets */
   unsigned long int used_p;		/* used data packets */
   unsigned long int bc_t;		/* bytecount total */
   unsigned long int bc_u;		/* bytecount of valid frames */
};
//} misc;

//struct Misc *p_mi;			/* pointer to misc structure in
//					 *   shared memory */
						



#ifdef BDS_DIRECT
/*
 * -------------------------------------------------------------------------
 * structure definition for 'ems' direct bds decoding
 *   only present if 'BDS_DIRECT' defined (see above)
 *   then part of each data block for each plane/icao-address
 * -------------------------------------------------------------------------
 * */
struct BDS
{
					/* from bds 4.0: */
   double fcu_alt_40;			/* mcp/fcu selected altitude [ft] */
   double fms_alt_40;			/* fms selected altitude [ft] */
   double baro_40;			/* baro setting [millibar] = [hpa] */
   int vnav_40;				/* vnav mode [0/1]*/
   int alt_h_40;			/* altitude hold [0/1]*/
   int app_40;				/* approach mode [0/1]*/
   int alt_s_40;			/* target altitude source
					 *   0 = unknown
					 *   1 = aircraft altitude
					 *   2 = FCU/MCP selected altitude
					 *   3 = FMS        "        "    */
   
   					/* from bds 5.0: */
   double roll_50;			/* roll angle [degr] */
   double track_50;			/* true track [0..360 degr] */
   double g_speed_50;			/* ground speed [kn] */
   double t_rate_50;			/* track angle rate [degr./sec] */
   double tas_50;			/* true airspeed [kn] */
					 
					/* from bds 6.0: */
   double hdg_60;			/* magnetic heading [0..360 degr.] */
   double ias_60;			/* indicated airspeed [kn] */
   double mach_60;			/* mach ??? */
   double vert_b_60;			/* vertical speed, baro based
					 *   [ft/min] */
   double vert_i_60;			/* vertical speed, ins based
					 *   [ft/min] */
   
   int l_bds;				/* last bds decoded */
   time_t t_bds;			/* time of last bds decoding */
};
#endif

/*
 * -------------------------------------------------------------------------
 * structure definition for 'Globally Unambiguous Position'
 *      time in unix seconds
 *      yz, xz in bins
 *      lat, lon in degree
 * 
 * used in each data block for each plane/icao-address
 * -------------------------------------------------------------------------
 * */
struct CPR
{
   int cprf;                            /* last cpr format, even=0, odd=1 */
   time_t t[2];                         /* time of position data, even/odd */
   long int yz[2];                      /* downlink position data, even/odd */
   long int xz[2];                      /* downlink position data, even/odd */
   double rlat[2];                      /* computed position */
   double rlon[2];
   time_t t_s;                          /* time of reference/last position */

   double lat_s;                        /* lat             "               */
   double lon_s;                        /* lon             "               */
};

/*
 * -------------------------------------------------------------------------
 * plane data structure
 *  (placed P_MAX times in shared memory)
 * -------------------------------------------------------------------------
 * */
struct Plane
{
   unsigned long int icao;		/* icao address */
   time_t utms;				/* time of any last update 
					 *  in UNIX sec 
					 *  0 if dataset is in update */
   unsigned long int total;		/* total updates */
   unsigned long int id;		/* identity (squawk) (DF05 DF21) */
   int acc;				/* aircraft category (DF17) */
   int acc_ftc;				/* FTC of CAT (01 ..04) */
   char acident[9];			/* aircraft ident (DF17 FTC01..04)
					 *  (flight # ) */
   double lon;				/* longitude position data */
   double lat;				/*  latitude */
   int pos_ftc;				/* FTC from DF17 of last position */
   time_t ll_t;				/*  time of update */
   double alt;				/* altitude [ft] DF04 DF00 */
   int alt_s;				/* altitude source (DFnn) */
   int alt_ftc;				/* altitude source FTC
					 *  FTC if DF17 or DF18, 0 otherwise */
   int alt_c;				/* altitude source coding 
					 *  0=GILLHAM, 1=binary
					 *  -1=not avail. 
					 *  -2=metric (scaling unknown !!!) */
   double alt_d;			/* difference GNSSheight - Baro 
					 *  (DF17 FTC19) */
   time_t alt_t;			/* time of last alt update */

   double veast;			/* speed over ground in east dir. .. */
   double vnorth;			/*  ... in north dir.
					 *  (DF17 FTC19/1/2) */

   double hdg;				/* heading ... */
   double ias;				/*  ... airspeed indicated ... */
   double tas;				/*  ... true  
					 *  in kn (DF17 FTC19/3/4 */
   
   double sogc;				/* speed over ground in kn ... */
   double cogc;				/* ... course over ground
					 *  if 'g_flag' = 0  computed
					 *  from DF17 FTC19/1/2
					 *  if 'g_flag' = 1 from
					 *  DF17 Surface position */
   
   double vert;				/* vertical speed (ft/min) */
   int verts;				/* source of vertical speed
					 *  0=GNSS, 1=Baro, -1=unknown */
   time_t cs_t;				/* time of last course and speed
					 *  update (DF17 FTC19) */
   int clic;				/* interrogator (DF11 DF17 DF18) */
   int ca;				/* capability CA (DF11 DF17) */
   int cf;				/* control CF (DF18) */  
   int af;				/* application AF (DF19) */
   int fs;				/* flight status (DF04) */
   int dr;				/* downlink request (DF04) */
   int um;				/* utility message (DF04) */
   int vs;				/* vertical status (DF00) */
   int cc;				/* cross-link cap. (DF00) */
   int ri;				/* reply info (DF00) */
   int sstat;				/* surveillance status (DF17) */
   int nicsb;				/* NIC supplement B (DF17) */
   int tbit;				/* time sync bit (DF17) */
   int g_flag;				/* set if on ground */
   int ems;				/* emergency state (FTC 28) */
   struct CPR pos;			/* position data (for decoding) */
#ifdef BDS_DIRECT
   struct BDS bds;			/* direct bds decoding (DF20 DF21) */
#endif   
   int lastdf;				/* DF of last update */
   unsigned long int dstat;		/* data status (DF bitmask)
					 *  each bit set represents a rec. DF
					 *  bit 0 (LSB) == DF00 */
};

/*
 * -------------------------------------------------------------------------
 * function prototypes from 'shmsub02'
 * -------------------------------------------------------------------------
 */

int get_shm_id(void);

int shm_att(int);

struct Misc *p_2_misc(void);

struct Plane *p_2_0(void);

struct Plane *pptr(int i);

struct Plane *p_icao(unsigned int addr);

#endif

