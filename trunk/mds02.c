/* 'Mode S' data assembling
 * 
 * handling of ADS-B data from
 *   - PlaneGadgetRadar (PGR)
 *   - Receiver with AVR format without timestamp (AVR)
 * places data in shared memory
 *
 * */
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
 * ------------------------------------------------------------------------
 *  --- alpha test version ---
 * ------------------------------------------------------------------------
 * */

#define version "mbr20110902"

//#define DEBUG_AVRin 1
					/* AVR raw data printout */
//#define DEBUG_AVRfr 1
					/* AVR frame error text to log file */
//#define INTER_Stat 1
					/* interrogator status counts to log */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <getopt.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "mds02.h"
#include "decsub02.h"
#include "decbds.h"

char Str[80];				/* for error report (and other) */

unsigned char Dbuf[15];			/* raw data buffer (14 data, 1 sync) */
unsigned char Dbuf_o[15];		/* buffer for comparison */
					
					/* sync character in save file */
#define STX 0x02
#define FF  0xff
#define Sbufl 32
   					/* save file output buffer length:
					 * 	2 sync bytes +
					 * 	time data +
					 * 	Dbuf length
					 * at least */
unsigned char Sbuf[Sbufl];

char init_str[80] = {""};		/* initialization string */
char term_str[80] = {""};		/* termination string */

int RW_flag;				/* read/write save file marker */
int In_fd;				/* device or saved file for reading */
int Save_f;				/* for save or replay */
int i_oldest;				/* oldest structure */
int I_fmt = -1;				/* input format key 
					 *     -1 = unknown
					 * 	0 = save file
					 *	1 = PGR
					 *	2 = AVR */
int fr_err_c = -1;			/* corrected framing error */
int fr_err_u = -1;			/* uncorrectable framing error */


int Shm_id;

int clic;				/* checksum / interrogator */

#ifdef INTERROG
					/* interrogator check */
# define CLIC_MASK 0x3f

#define L_CNT 16384
int m_cnt = 16;				/* initial maximum count */
#define I_CNT 128
int known[I_CNT];			/* 7 bit clic */

#endif

FILE *Log_f;				/* for logging and errors */
FILE *i_fl;				/* input */


time_t D_tim;				/* data time 
					 * receive time of date in realtime
					 * time from save file on replay */

struct Plane *p_2_empty;		/* pointer to empty data structure */
struct Misc *p_mi;

extern void *Shm_ptr;



/* ------------------------------------------------------------------------
 * initialize empty plane structure
 * unknown values are (mostly) set to:
 *	0			if 0 not a valid data
 *	-1			if 0 is a valid data and -1 is not
 *				  and data is a signed value
 * 	<out of range value>	otherwise
 * ------------------------------------------------------------------------ */
void pl_empty_init (struct Plane *p) 
{
   p->icao = 0;				/* icao address */
   p->utms = 0;				/* time of last update in UNIX sec */
   p->total = 0;			/* total update count */
   p->id = 0;				/* identity (squawk) DF05 DF21 */
   p->acc = 0;				/* aircraft category DF17 FTC01..04 */
   p->acc_ftc = -1;			/* aircraft category FTC01..04 */
   p->acident[0] = '\000';		/* aircraft ident DF17 FTC01..04 */
   p->lat = -9999.;			/* actual position data */
   p->lon = -9999.;
   p->pos_ftc = -1;			/* ftc of position */
   p->ll_t = 0;
   p->alt = -9999;			/* altitude [ft] DF04 DF00 */
   p->alt_s = -1;			/* altitude source (DFnn) */
   p->alt_ftc = -1;			/* altitude source subtype (FTC) */
   p->alt_c = -1;			/* altitude source coding */
   p->alt_d = -9999.;			/* altitude / height difference */
   p->alt_t = 0;			/* time of last alt update */
   p->veast = -9999.;			/* speed in east dir. */
   p->vnorth = -9999.;			/* speed in north dir. */
   p->sogc = -9999.;			/* speed over ground computed */
   p->cogc = -9999.;			/* course over ground computed */
   p->hdg = -9999.;			/* heading */
   p->ias = -9999.;			/* indicated airspeed */
   p->tas = -9999.;			/* true airspeed */
   p->vert = -9999;			/* vertical speed */
   p->verts = -1;			/* source of vertical speed */
   p->cs_t = 0;				/* time of last course and speed */
   p->clic = -1;			/* interrogator */
   p->ca = -1;				/* capability DF11 DF17 */
   p->cf = -1;				/* control DF18 */
   p->af = -1;				/* application DF19 */
   p->fs = -1;				/* flight status DF04*/
   p->dr = -1;				/* downlink request DF04 */
   p->um = -1;				/* utility message DF04 */
   p->vs = -1;				/* vertical status DF00 */
   p->cc = -1;				/* cross-link cap. */
   p->ri = -1;				/* reply info */
   p->sstat = -1;			/* surveillance status */
   p->nicsb = -1;			/* NIC supplement B */
   p->tbit = -1;			/* time sync bit */
   p->g_flag = -1;			/* set if on ground */
   p->ems = -1;				/* emergency state */
   					/* struct CPR position data 
					 *   used for decoding */
   p->pos.cprf = -1;
   p->pos.t[0] = 0;
   p->pos.yz[0] = 0;
   p->pos.xz[0] = 0;
   p->pos.rlat[0] = 0.;
   p->pos.rlon[0] = 0.;
   p->pos.t[1] = 0;
   p->pos.yz[1] = 0;
   p->pos.xz[1] = 0;
   p->pos.rlat[1] = 0.;
   p->pos.rlon[1] = 0.;
   p->pos.t_s = 0;			/* ! 0 here (used as flag too) */
   p->pos.lat_s = -9999.;
   p->pos.lon_s = -9999.;
   
#ifdef BDS_DIRECT
   p->bds.fcu_alt_40 = -9999.;		/* mcp/fcu selected altitude [ft] */
   p->bds.fms_alt_40 = -9999.;		/* fms selected altitude [ft] */
   p->bds.baro_40 = 0;			/* baro setting [millibar] */
   p->bds.vnav_40 = -1;			/* vnav mode [0/1]*/
   p->bds.alt_h_40 = -1;		/* altitude hold [0/1]*/
   p->bds.app_40 = -1;			/* approach mode [0/1]*/
   p->bds.alt_s_40 = -1;		/* target altitude source [0..3] */
   
   					/* from bds 5.0: */
   p->bds.roll_50 = -9999.;		/* roll angle [degr] */
   p->bds.track_50 = -9999.;		/* true track [0..360 degr] */
   p->bds.g_speed_50 = -9999.;		/* ground speed [kn] */
   p->bds.t_rate_50 = -9999.;		/* track angle rate [degr./sec] */
   p->bds.tas_50 = -9999.;		/* true airspeed [kn] */
					 
					/* from bds 6.0: */
   p->bds.hdg_60 = -9999.;		/* magnetic heading [0..360 degr.] */
   p->bds.ias_60 = -9999.;		/* indicated airspeed [kn] */
   p->bds.mach_60 = -9999.;		/* mach ??? */
   p->bds.vert_b_60 = -9999.;		/* vertical speed, baro based
					 *   [ft/min] */
   p->bds.vert_i_60 = -9999.;		/* vertical speed, ins based
					 *   [ft/min] */
   
   p->bds.l_bds = -1.;			/* last bds decoded */
   p->bds.t_bds = 0.;			/* time of last bds decoding */
#endif
   
   p->lastdf = -1;			/* DF of last update */
   p->dstat = 0;			/* data status (bitmask) */

   return;
}

/* ------------------------------------------------------------------------
 * send string pointed to by s to log file with leading time data
 * ------------------------------------------------------------------------ */
void log_to_file(const char *s)
{
   static int count;
#define total_c 1000
   
   struct tm *syst;
   time_t t;


   t = time(NULL);
   syst = localtime(&t);
   fprintf(Log_f,"%.2d:%.2d:%.2d   ", 
	  syst->tm_hour, syst->tm_min, syst->tm_sec);
   count++;
   if (count > total_c)
     {
	fprintf(Log_f,"to many ERRORS accumulated --- FATAL\n",s);
	fprintf(stderr,
		"\nmds02 -- terminated, to many errors in log file\n\n");
	exit(1);
     }
   else
     {
   fprintf(Log_f,"%s\n",s); 
   fflush(Log_f);
     }
}

   
/* ------------------------------------------------------------------------
 * find pointer to plane structure of an empty icao address
 * 	if not found --
 * 		find oldest dataset
 * 		delete it
 * 		log to file
 * 		use this now empty place
 * ------------------------------------------------------------------------ */
struct Plane *p_empty(void)
{
   int i;
   struct Plane *p;
   struct Plane *p_oldest;
   
   time_t t_oldest;
 
   p = pptr(1);				/* initialize age */
   t_oldest = p->utms;
   p_oldest = p;
	
   for (i=1; i<=P_MAX; i++)
     {
	p = pptr(i);
	if (p->icao == 0)  	/* empty structure found - use it */
	  {
	     if (i > p_mi->P_MAX_C) p_mi->P_MAX_C = i;
	     return p;
	  }
	     
	if (p->utms < t_oldest)	/* remember oldest */
	  {
	     t_oldest = p->utms;
	     p_oldest = p;
	  }
     }
   
   p = pptr(0);			/* delete the oldest and use it */
   *p_oldest = *p;
   log_to_file("shared memory full -- oldest dataset deleted and space used");
   return p_oldest;
}
   
/* ------------------------------------------------------------------------
 * send command to PGR in realtime mode
 *  	return 0 on success
 *             1 on error
 * ------------------------------------------------------------------------ */
int com_to_pgr(const char *s)
{

   extern char Str[sizeof Str]; 
   extern int RW_flag;
   
   size_t n;
   
   if (RW_flag < 0) return(0);		/* nothing to do for replay */

   if ((n = strlen(s)) == 0) return 0;
 
   
   if (write(In_fd, s, n) != n)
     {
	snprintf(Str,sizeof Str,"PGR command '%s' output error:\n\t\t%s",
		 s,strerror(errno));
	log_to_file(Str);
	return 1;
     }
   return 0 ;
}

/* ------------------------------------------------------------------------
 * terminate prog on fatal error 
 * ------------------------------------------------------------------------ */

void p_term_fatal(void)
{
   fprintf(stderr,"\nmds02 -- terminated, fatal, see log file\n");
   exit(1);
}

/* ------------------------------------------------------------------------
 * stop data and terminate prog
 * 	(assumes shared memory initialized and in use)
 * ------------------------------------------------------------------------ */

void p_term(int n)
{ 
   extern char Str[sizeof Str];
   extern char term_str[sizeof term_str];
   extern int I_fmt;
   extern FILE *i_fl;
   extern int Save_f;
   extern int fr_err_c, fr_err_u;
   
   printf("\nmds02 -- terminating ...\n");

					/* send termination string
   					 * (use default if not defined
					 *  in commandline) */
   if (I_fmt == 1) 
     {
	if (term_str[0] == '\000')
	  strncpy(term_str,I_TERM_PGR,sizeof term_str);
	if (term_str[0] != '\000') com_to_pgr(term_str);
	fsync(In_fd);
	close(In_fd);
     }
   
   if (I_fmt == 2) 
     {
	if (term_str[0] == '\000')
	  strncpy(term_str,I_TERM_AVR,sizeof term_str);
	if (term_str[0] != '\000') fprintf(i_fl,"%s",term_str);
	fclose(i_fl);
     }
   
   if (I_fmt > 0)
     {
	snprintf(Str,sizeof Str,
		 "termination string send to input device: %s",term_str);
	log_to_file(Str);
     }
   
   if (Save_f == 0) close(Save_f);
   
   sleep (2);
      
   if (Shm_id != 0)
     {
	p_mi->t_flag = -1;		/* mark shared memory removes */

	snprintf(Str,sizeof Str,
		 "total packets received: %ld, used: %ld (%.1f%%)",
		 p_mi->total_p, p_mi->used_p,
		 ((double)p_mi->used_p/(double)p_mi->total_p)*100.);
	log_to_file(Str);
		
   					/* report framing errors */
	if ((fr_err_c >= 0) || (fr_err_u >= 0))
	  {
	     snprintf(Str, sizeof Str,
		      "frame errors: %d uncorrectable, %d corrected",
		      ++fr_err_u, ++fr_err_c);	/* count starts with 0 */
	     log_to_file(Str);
	  }

	if (shmctl(Shm_id, IPC_RMID, NULL) < 0)
	  {
	     log_to_file("error -- shared memory  n o t  removed");
	  }
	else
	  {
	     log_to_file("shared memory removed");
	  }
     }
   
   log_to_file("... terminated");
   exit(0);

}

/* ------------------------------------------------------------------------
 * time from global 'D_tim' and data from global 'Dbuf' to save file 
 *	with leading flag 0x02 and 0xff seperating time and data
 * 	format: 0x02<time>0xff<data>
 * 
 * 	also used global buffer 'Sbuf'
 * ------------------------------------------------------------------------ */
void save_data(void)
{
   extern unsigned char Dbuf[sizeof Dbuf];
   extern char Str[sizeof Str];

   static unsigned char Sbufs;		/* real buffer length (computed) */
   int n,i;

/* buffer fill */

   i = 0;
   Sbuf[i++] = STX;			/* start of buffer sync */

   for (n=0; n<sizeof(time_t); n++) 	/* time data */
     Sbuf[i++] = (D_tim>>(n*8)) & 0xff;
   
   Sbuf[i++] = FF;			/* separator sync */
   
   for (n=0; n<((sizeof Dbuf)-1); n++)	/* buffer without last sync byte */
     {
	Sbuf[i++] = Dbuf[n]; /* the data */
	if (i > sizeof Sbuf)
	  {
	     fprintf(stderr,"mds02 -- fatal save buffer overflow !!!\n\n");
	     exit(1);
	  }
     }

/* write real buffer length to file at begin */
   
   if (Sbufs == 0)
     {
	Sbufs = i;
	write(Save_f,&Sbufs,1);		/* buffer size to file */
     }
   
/* buffer output */
   
   if (write(Save_f,Sbuf,i) != Sbufs)
     {
	snprintf(Str,sizeof Str,"error on write to save file:\n\t%s",
		 strerror(errno));
	log_to_file(Str);
     }
}

/* ------------------------------------------------------------------------
 * from save file time to 'D_tim' and data to 'D_buf' 
 *	with leading sync char 0x02 and 0xff seperating time
 *	and data checked
 * 	report df on success and -2 on eof
 * 
 * 	used global buffer 'Sbuf'
 * ------------------------------------------------------------------------ */
int rest_data(void)
{
   
   extern unsigned char Dbuf[sizeof Dbuf];
   extern time_t D_tim;
   extern unsigned char Sbuf[sizeof Sbuf];
     

   static unsigned char ibufs;		/* real buffer length */

   int n,i;
   
/* get length of data block */
   
   if (ibufs == 0)
     {
	if ((n = read(Save_f,&ibufs,1)) != 1)
	  {
	     perror("mds02 -- fatal error on first read from save file:\n\t");
	     exit(1);
	  }
     }
   
/* read the data */
   
   n = read(Save_f,Sbuf,ibufs);
   if (n == 0) return -2;
   if (n != ibufs)
     {
	perror("mds02 -- fatal error on data read from save file:\n\t");
	exit(1);
     }
   
/* sync check */
   
   i = sizeof(time_t) + 1;		/* index of 'FF' sync char */
   if ((Sbuf[0] != STX) || (Sbuf[i] != FF))
     {
	fprintf(stderr,"mds02 -- fatal sync error reading save file\n\n");
	exit(1);
     }
   
/* restore time */
   
   D_tim = 0;
   for (n=sizeof(time_t); n>0; n--) 
     {
	D_tim <<= 8;
	D_tim |= Sbuf[n];
     }

/* fill 'dbuf' with data */   
   
   for (n=0; n<((sizeof Dbuf)-1); n++)
     {
	i++;
	if (i > ibufs)
	  {
	     fprintf(stderr,"mds02 -- fatal short read from save file\n\n");
	     exit(1);
	  }
	Dbuf[n] = Sbuf[i];
     }
   return ((Dbuf[0])>>3)&0x1f;	/* df, isolated */
}
/* ------------------------------------------------------------------------
 * convert one ascii hex character c to	integer returned
 * 	return >15 if error
 * ------------------------------------------------------------------------ */

int hex2b(char c)
{
   if (c > 0x60) c -= 0x20;   		/* convert character to upper */
   if ((c > 0x46) || (c < 0x30)) return 0x10; /* > 'F' or < '0' */
   if (c > 0x40) return c-0x37;		/* A..F -> 1010 .. 1111 */
   if (c < 0x3a) return c-0x30;	       	/* 0..9 -> 0000 .. 1001 */
   return 0x10;				/* > 9 to < A */ 
}
/* ------------------------------------------------------------------------
 * convert two ascii hex characters pointed to by cp to	integer returned
 * 	return >0xff if error
 * ------------------------------------------------------------------------ */

int twoh2b(char *cp)
{
   int kh, kl;
   
   kh = hex2b(*cp);
   kl = hex2b(*(cp+1));
   if ((kh > 0xf) || (kl > 0xf)) return(0x100); /* error */
   kh = kh<<4;
   return kh|kl;
}

/* ------------------------------------------------------------------------
 * read frame from AVR input device
 * 	raw frame starts with '*' and ends with ';\n\r'
 * 	  checks format (length, terminator, ...)
 * 	  tries to reconstruct frame on error
 * 
 * return
 * 	df on success
 *      -1 on error in read with '*err' set to 'errno'
 *      -2 on eof (with '*err' undefined)
 *      -3 on (hex) decoding error
 * (requires at least 15 byte long 'dbuf')
 * ------------------------------------------------------------------------ */

int NextDataAVR(int *err)
{

   extern unsigned char Dbuf[sizeof Dbuf];
   extern char Str[sizeof Str];
   extern int fr_err_u, fr_err_c;
   
#define llength 48
   char line[llength];			/* input line buffer */
   
   char *cp;
   char *cpe;
   unsigned char *cpd;
   int i,k,n;
   int bcnt;
   int check;
   
/* read one (new) line with propper format from device */

   do 					/* get one formatted line 
					 * ('\r' terminated) */
     {
	cp = fgets(&line[0],llength,i_fl);
	if (feof(i_fl)) return -2; 	/* report eof */
	if (cp == NULL)
	  {
	     *err = errno;
	     return -1;			/* report other error */
	  }

#ifdef DEBUG_AVRin
	printf("%s",line);
#endif	

	i = strlen(line);
	
	bcnt = i+1;			/* count received bytes incl. cr-lf */
	p_mi->bc_t += bcnt;		/* statistics in shared memory */

	check = 0;			/* preset to false */

	i--;				/* last byte index */
	     				/* line must end with ';\n' */
	if ((line[i] != '\n')
	    || (line[i-1] != ';')) 
	  {
	     fr_err_u++;

#ifdef DEBUG_AVRfr
	     log_to_file("AVR frame termination error");
#endif
	     continue;   /* line must end with '\n' */
	  }
	
	i--;     
	cpe = &line[i];			/* pointer to frame terminator ';' */

	cp = strrchr(line,'*');		/* find frame start */

	n = cpe - cp;			/* length is fixed */
	if (((n != 15) && (n != 29)) || (cp == NULL))
	  {
	     fr_err_u++;		/* count error */

#ifdef DEBUG_AVRfr
	     snprintf(Str,sizeof Str,"AVR uncorrectable frame error:\n\t%s",
		      line);
	     log_to_file(Str);
#endif

	     continue;			/* ignore */
	  }
	check = 1;			/* frame ok */

	if (cp != &line[0])		/* '*' not at start of line */
	  {
	     fr_err_c++;		/* count error */

#ifdef DEBUG_AVRfr
	     snprintf(Str,sizeof Str,"AVR frame error corrected:\n\t%s",
		      line);
	     log_to_file(Str);
#endif
	  }
     }
   while (check != 1);

/* decode hex to bit/byte buffer and store in Dbuf */
   
   cp++;				/* pointer to first hex */
   
   n = 0;
   do
     {
	if ((k = twoh2b(cp)) > 0xff)
	  {
	     log_to_file("AVR hex decoding error");
	     strcpy(Str,"     ");
	     strcat(Str,line);
	     log_to_file(Str);
	     return  -3;
	  }
	Dbuf[n++] = k&0xff;		/* to buffer and next*/
	cp += 2;			/* next hex to convert */
     }
   while(cp < cpe);
   
   p_mi->bc_u += bcnt;			/* count used bytes */
   return ((Dbuf[0])>>3)&0x1f;		/* df, isolated */
}

/* ------------------------------------------------------------------------ */

#define NextDataPGR_R 1
#if NextDataPGR_R == 1

/* ------------------------------------------------------------------------
 * read frame from PGR input device
 * last version:
 * 	check for sync marker in buffer byte #14
 * 	if there, read next 15 bytes (14 data + next sync)
 * 	if not there read single byte in buffer byte #14 until
 * 		sync found - then read 15 byte
 * 	if last byte read is not sync start again
 * 	if sync at buffer end
 * 		return downlink format number 
 * return -1 on error in read with '*err' set to 'errno'
 * return -2 on eof single byte input (with '*err' undefined)
 * return -3 on eof 15 byte input (with '*err' undefined)
 * (requires at least 15 byte long 'dbuf')
 * ------------------------------------------------------------------------ */

int NextDataPGR(unsigned char *dbuf, int *err)
{
					/* length of ring buffer */
   static int ibuf_i;
   static unsigned char sync;
   unsigned char marker = 0xff;
   int n;
   int i;
   int ii;
   int ecnt;				/* conter for error report */
   int ibuf_l;				/* last byte of prev. frame */
   int loop = 0;

   do
     {
	ecnt = 0;
	while (sync != marker)
	  {
	     while ((n = read(In_fd, &sync, 1)) != 1)
	       {
		  if (n<0)
		    {
		       *err = errno;
		       return -1;
		    }
		  if (++ecnt > 100) return -2;
	       }
	     ++p_mi->bc_t;
	  }
   
	i = 15;
	ii = 0;
	
	ecnt = 0;
	do 
	  {
	     n = read(In_fd, dbuf+ii, i);
	     if (n < 0)
	       {
		  *err = errno;
		  return -1;
	       }
	     if (++ecnt > 200) return -3;
	     ii = ii+n;
	     i = i-n;
	  }
	while (i>0);
	sync = *(dbuf+14);
	
	p_mi->bc_t += 15;
	
     }
   while (sync != marker);
   
   p_mi->bc_u +=15;
   
   return (((*dbuf)>>3)&0x1f);		/* df, isolated */
}

/* ------------------------------------------------------------------------ */

#elif NextDataPGR_R == 2

/* ------------------------------------------------------------------------
 * read frame from input device
 * first version:
 * read bytes into ring-buffer at sequential position until marker found
 *	if byte at next position is also marker assume valid buffer
 * 	and transfer 14 bytes
 * return downlink format number on success
 * return -1 on error with '*err' set to 'errno'
 * return -2 on eof with '*err' set to 0
 * ------------------------------------------------------------------------ */

int NextDataPGR(unsigned char *dbuf, int *err)
{
   static unsigned char ibuf[16];	/* input ring buffer, fixed length */
   static int ibuf_i;
   
   unsigned char c;
   unsigned char marker = 0xff;
   int n;
   int i;
   int ecnt;				/* conter for error report */

   int bc = 0;
   
   *err = 0;
   
   do
     {
	ecnt = 0;
	do 
	  {
	     n = read(In_fd, &ibuf[ibuf_i], 1);
	     if (n<0)
	       {
		  *err = errno;
		  return -1;
	       }
	     ecnt++;
	     if (ecnt > 20) return -2;
	  }
	while (n!=1);

	++p_mi->bc_t;
	
	c = ibuf[ibuf_i];
	ibuf_i = (ibuf_i+1)&0xf;	/* 4 bit index */
	
//	if (++bc > 15) printf("%d\n",bc);
     }
   while ( (c != marker) || (ibuf[ibuf_i] != marker) );
   
   for (n=0; n<14; n++)			/* make data buffer */
     {
	i = (ibuf_i+n+1)&0xf;
	*(dbuf+n) = ibuf[i];
     }

   p_mi->bc_u +=15;

   return ((*dbuf)>>3)&0x1f;
}

/* ------------------------------------------------------------------------ */
   
#endif

/* ------------------------------------------------------------------------
 * complete handling of a valid DF
 *   insert current time in airplane structure pointed to by p
 * 	at propper place
 *   set DF marker in structure
 *   mark opdate of any data structure
 * ------------------------------------------------------------------------ */
void df_fin(struct Plane *p, int dfx)
{
   extern int RW_flag;

   int i;
   static time_t d_tim_o;
   time_t sec;

   					/* insert in data structure ... */
   p->utms = D_tim;			/* ... time of last update */
   p->dstat |= 0x1<<dfx;		/* ... mask of received DF */
   p->lastdf = dfx;			/* ... last DF received */
   p->total++;				/* count */

   p_mi->t_flag = D_tim;		/* mark any update */
   p_mi->used_p++;			/* count used packets */
   
   if (RW_flag == 2) save_data();	/* save used data */

/* every second ... */
   
   if (D_tim != d_tim_o)	
     {
	d_tim_o = D_tim;		/* remember last pass */

   /* ... delete outdated planes from shared memory */

	sec = D_tim-L_TIME;		/* lifetime limit */

	for (i=1; i<= p_mi->P_MAX_C; i++)
	  {
	     p = pptr(i);
	     if ((p->utms < sec) && (p->utms != 0))
	       {
		  *p = *p_2_empty;
		  if (i == p_mi->P_MAX_C) p_mi->P_MAX_C--;
	       }
	     
	  }
     }
}

#ifdef INTERROG

/* ------------------------------------------------------------------------
 * check for a known interrogator
 * return 1 if ok, 0 otherwise
 * 
 * interrogators are valid if they have been seen at least 'm_cnt/2' times
 * 'm_cnt' is the count of the most often seen interrogator
 *   but starts with preset initial setting of 'm_cnt'
 * 
 * ------------------------------------------------------------------------ */
int known_clic(int clic)
{
   extern int known[I_CNT];
   extern int m_cnt;
					/* count limit for subtraction */
   int n;

					/* high order bits show parity error */
   if ((clic&(~CLIC_MASK)) != 0) return 0;
   clic &= CLIC_MASK;			/* used bits */
   
   if (++known[clic] > m_cnt) m_cnt = known[clic]; /* set current maximum */
 
  					/* avoid overflow */
   if (m_cnt > L_CNT)
     {
	for (n=0; n<I_CNT; n++) known[n] >>= 1; /* divide all by 2 */
	m_cnt >>= 1;
     }
   if (known[clic] > (m_cnt>>1))
     {
	return 1;
     }
   
   return 0;
}

/* ------------------------------------------------------------------------
 * print a list of known interrogators to log file on 'SIGUSR1' signal
 * ------------------------------------------------------------------------ */
void pr_known(int l)
{
   extern int known[I_CNT];
   extern int m_cnt;
   extern FILE *Log_f;
   
   int n;
   
#ifdef INTER_Stat
   
   log_to_file("interrogator status:");
   
    for (n=1; n<I_CNT; n++)
     fprintf(Log_f,"\t\t\t\t%3d %6d\n",n,known[n]);

#endif

   log_to_file("known interrogators now:");
   
   for (n=1; n<I_CNT; n++)
     {
	if (known[n] > (m_cnt>>1))
	  {
	     fprintf(Log_f,"\t\t\t\t%d\n",n);
	  }
     }


   fflush(Log_f);
   return ;
}

#else

/* ------------------------------------------------------------------------
 * ignore 'SIGUSR1' signal if interrogator check not compiled in
 * ------------------------------------------------------------------------ */
void pr_known(int l)
{
   log_to_file("SIGUSR1 received -- interrogator check not compiled in");
   return ;
}

#endif

/* ------------------------------------------------------------------------
 * usage
 * ------------------------------------------------------------------------ */
void p_usage(void)
{
   printf("\nmds02 - version "version"\n"
	  "usage:\n"
	  "\tmds02 [-d <path>] [-a | -u | -r [-x n] [-f <file>]] \\\n"
	  "\t      [-i {PGR|AVR}] [-I <string>] [-T <string>] [-h]]\n"
	  "\t\t-d <path>  path name of PGR device\n"
	  "\t\t           (default: as compiled in)\n"
	  "\t\t-r\t   read data from save file\n"
	  "\t\t-a\t   write all received data to save file\n"
	  "\t\t-u\t   write used data to save file\n"
	  "\t\t-x n\t   speed up replay by adding n (with n integer > 1)\n"
	  "\t\t-f <file>  save file name\n"
	  "\t\t           (default file name: "DEF_SF_NAME")\n"
	  "\t\t-i <..>    input format PGR or AVR (select one !)\n"
	  "\t\t           (default: as compiled in)\n"
	  "\t\t-I <\"..\">  initialization string\n"
	  "\t\t           (default: as compiled in for PGR or AVR)\n"
	  "\t\t-T <\"..\">  termination string\n"
	  "\t\t           (default: as compiled in for PGR or AVR)\n"
	  "\t\t-h\t   print this help\n"
	  "\t\tmultiple use of -I and -T  allowed\n"
	  "\t\terror and other log in file: "LOG_F_NAME"\n"
	  "\n");
   exit(0);
}

/* ------------------------------------------------------------------------
 * ------------------------------------------------------------------------ */
main(int argc, char *argv[]) 
{

   extern char *optarg;
   extern int optind, opterr, optopt;
   extern char Str[sizeof Str];
   extern char init_str[sizeof init_str];
   extern int RW_flag;
   extern int I_fmt;
   
   struct Plane *p; 
   
   char in_file[40];
   char save_file[40] = {DEF_SF_NAME};


   unsigned char uc;
   unsigned char *dfp;			/* pointer to raw data buffer */
   unsigned int ftc;			/* format type code */
   unsigned long int ac;		/* altitude code */
   unsigned long int cs;		/* checksum */
   unsigned long int r;			/* remainder */
   
   int dfx;				/* curent data format */
   int g_fl;				/* on surface flag */
   int opt;
   int sfaf;				/* save file flags */
   int sft_add;				/* replay speed */
   int i,k,l,n;				/* integer for misc. use */

   mode_t sfam;				/* save file mode */
   time_t tim_o, t_next;
   time_t st_c;
   time_t sft_n;   

/* some simple initialization */
   
   tim_o = 0;
   Shm_id = 0;
   RW_flag = 0;				/* realtime data assembly */
   sft_add = 1;				/* replay in 1 sec interval */
   in_file[0] = '\000';
   
/* command line options */   

   strcpy(Str,I_FMT);			/* preset input format (mds02.h) */

   while ((opt = getopt(argc, argv, "raux:f:d:i:I:T:h?")) != -1)
     {
   
	switch (opt)
	  {
	   case 'r':			/* read saved file */
	     RW_flag = -1;
	     break;
	   case 'a':			/* save all received data */
	     RW_flag = 1;
	     break;
	   case 'u':			/* save used data */
	     RW_flag = 2;
	     break;
	   case 'x':			/* speed up replay */
	     sft_add = atoi(optarg);
	     if (sft_add < 1) p_usage();
	     break;
	   case 'f':			/* save file name */
	     strcpy(save_file,optarg);
	     break;
	   case 'd':			/* device file path name */
	     strcpy(in_file,optarg);
	     break;
	   case 'i':
	     strcpy(Str,optarg);	/* input format select */
	     break;
	   case 'I':
	     strcat(init_str,optarg);	/* initialization string */
	     strcat(init_str,"\r");
	     break;
	   case 'T':
	     strcpy(term_str,optarg);	/* termination string */
	     break;
	   default:			/* 'h' `?' ... */
	     p_usage();
	  }
     }
					/* set input format key */
   if (strstr(Str,"PGR")) I_fmt = 1;
   if (strstr(Str,"AVR")) I_fmt = 2;
   if (I_fmt < 0) p_usage();

					/* if filename only */
//   if ((argc > 1) && (RW_flag == 0)) p_usage();

/* more initialization */
   
   if (RW_flag != 0) 
       {
					/* set save file access params */
	  sfam = S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;
	  if (RW_flag < 0)
	    {
	       sfaf = O_RDONLY;
	    }
	  else
	    {
	       sfaf = O_WRONLY|O_CREAT|O_TRUNC;
	    }
					/* open save file */
	  if ((Save_f = open(save_file,sfaf,sfam)) == -1)
	    {
	       perror("mds02 -- save file open error");
	       exit(1);
	    }
       }

/* prepare termination */
   
   signal(SIGKILL, p_term);		/* terminate by KILL signal ... */
   signal(SIGTERM, p_term);
   signal(SIGINT, p_term);		/*           by <CTRL>-C */

   signal(SIGUSR1, pr_known);		/* request list of known iterrog. */
   
/* open log file */
   
   if ((Log_f = fopen(LOG_F_NAME,"w+")) == NULL)
     {
	perror("mds02 -- log file open error");
	exit(1);
     }

   snprintf(Str,sizeof Str,
	    "'mds02' version "version" started");
   log_to_file(Str);

/* make shared memory for airplane structures */   
   
   n = sizeof(struct Plane)*(P_MAX+1);	/* size of shared memory */
   n += sizeof(struct Misc);
					/* create shared memory region */
   if ((Shm_id = shmget(IPC_PRIVATE, n, 
			 (0666 | IPC_CREAT | IPC_EXCL))) < 0)
     { 
	snprintf(Str,sizeof Str,"shared memory not created: %s\n\t\t"
		 "... trying to use present one",
		 strerror(errno));
	log_to_file(Str);
     }

   if ((Shm_id = shmget(IPC_PRIVATE, n, 0666)) < 0)
     {
	log_to_file("error in creating shared memory ID, fatal...");
	p_term_fatal();
     }
   					/* set pointer to shared memory */
   if ( (Shm_ptr=shmat(Shm_id,NULL,0)) == (void *)-1)
       { 
	  snprintf(Str,sizeof Str,"shared memory referencing error:\n\t\t%s",
		   strerror(errno));
	  log_to_file(Str);
	  p_term_fatal();
       }

   snprintf(Str,sizeof Str,
	    "SHM_ID=%d ...",Shm_id);
   log_to_file(Str);
	
/* init input device */

   if (RW_flag >= 0)			/* if no read from save file */
     {   
	if (in_file[0] == '\000') strcpy(in_file,I_F_NAME);
	     strcpy(Str,"stty -F ");
	     strcat(Str,in_file);

	if (I_fmt == 1)			/* for PGR ... */
	  {
//	     strcat(Str," 115200 raw -echo -icanon time 0 min 1");
	     strcat(Str," 115200 raw -echo");
	  }
	
	if (I_fmt == 2)			/* for AVR (line oriented) ... */
	  {
	     strcat(Str," 115200 -echo igncr");
	  }
	     system (Str);

/* open input PGR device */
	
	if (I_fmt == 1)
	  {
	     snprintf(Str,sizeof Str,
		      "input from PGR device %s",in_file);
	     log_to_file(Str);
	     if ((In_fd = open(in_file,O_RDWR|O_NOCTTY)) == -1)
	       { 
		  perror("PGR device open error\n\t");
		  snprintf(Str,sizeof Str,"PGR -- %s",strerror(errno));
		  log_to_file(Str);
		  p_term_fatal();
	       }
	     				/* init string device in realtime */
	     if (init_str[0] == '\000')
	       strncpy(init_str,I_INIT_PGR,sizeof(init_str));
	     snprintf(Str,sizeof Str,"PGR initialized with: %s",init_str);
	     log_to_file(Str);

	     if (com_to_pgr(init_str))
	       {
		  fprintf(stderr,
			  "\nmds02 - error sending PGR init string"
			  "(see log)\n\n");
		  p_term_fatal();
	       }
	     fsync(In_fd);
	  }
	
/* open input AVR device (stream) */
	
	if (I_fmt == 2)
	  {
	     snprintf(Str,sizeof Str,"input from AVR device %s",in_file);
	     log_to_file(Str);
	     
	     for (n=0; n<3; n++)	/* try open 3 times */
	       {
		  i_fl = fopen(in_file,"r+");
		  if (i_fl != NULL) break; /* teminate loop if ok */
		  sleep(1);
	       }

	     if (i_fl == NULL)		/* on error report and terminate */
	       {
		  perror("AVR device open error\n\t");
		  fprintf(stderr,"\n\terrno: %d\n",errno);
		  snprintf(Str,sizeof Str,"AVR -- %s",strerror(errno));
		  log_to_file(Str);
		  p_term_fatal();
	       }
	     
	     if (init_str[0] == '\000')
	       strncpy(init_str,I_INIT_AVR,sizeof(init_str));
	     snprintf(Str,sizeof Str,"AVR initialized with: %s",init_str);
	     log_to_file(Str);

	     if (strlen(init_str) > 0)
	       {
		  n = fprintf(i_fl,"%s",init_str); /* init string to device */
		  if (n != strlen(init_str))
		    {
		       log_to_file("fatal error sending string");
		       fprintf(stderr,
			       "\nmds02 - error sending AVR init string"
			       "(see log)\n\n");
		       p_term_fatal();
		    }
		  fflush(i_fl);
	       }
	  }
     }
   else
     {
	snprintf(Str,sizeof Str,
		 "input from save file: '%s' with speed factor:%d",
		 save_file, sft_add);
	log_to_file(Str);
	I_fmt = 0;
     }

   
/* initialize the first Plane structure with  unknown/empty data */

   p_2_empty = (struct Plane *)((long int)Shm_ptr);
//   p_2_empty = p_2_0();
   pl_empty_init(p_2_empty);
   
/* copy pl_empty to all structures in shared memory */
   
   for (i=1; i<=P_MAX; i++) *pptr(i) = *p_2_empty;
   
/* pointer to Misc structure (just behind last Plane structure) */
   
   p_mi = p_2_misc();
					/* some init */
   p_mi->t_flag = 0;			/* time of last update and flag */
   p_mi->total_p = 0;			/* total of received data packets */
   p_mi->used_p = 0;			/* used packets */

   p_mi->t_start = time(NULL);		/* time at program start */

   st_c = p_mi->t_start;		/* time for save file restore */
   sft_n = 0;

/* main loop */
   
   while(1) 
     {
					/* initialize buffer
					 *  (may be filled only partially) */
	for (i=7; i<(sizeof Dbuf); i++) Dbuf[i] = 0;

/* get valid new buffer */

	if ( RW_flag < 0) 
	  {
	 				/* data from save file */
	     dfx = rest_data();

	     if (sft_n == 0) sft_n = D_tim; /* on first run only */

					/* every second in real time add
					 * 'sft_add' to 'sft_n' for next wait
					 * in input data stream */
	     while (D_tim > sft_n)	/* if new D_time .. */
	       {
		  sft_n += sft_add;	/* .. add for next period .. */
		  st_c++;		/* .. wait for next sytem time sec */
		  while(st_c > time(NULL)) usleep(100000);
	       }
	  }

	if (I_fmt == 1) 		/* PGR or save file */
	  {
	     dfx = NextDataPGR(&Dbuf[0], &n);
	  }
	
	if (I_fmt == 2) 		/* AVR */
	  {
	     dfx = NextDataAVR(&n);
	     if (dfx == -3) continue;	/* hex decoding error: log only */ 
	  }
	
	if (dfx < 0) 
	  {
	     if (dfx == -1)
	       {
		  snprintf(Str,sizeof Str,"fatal error on input file:\n\t\t%s",
			      strerror(n));
		  log_to_file(Str);
	       }
	     else			/* eof */
	       {
		  snprintf(Str,sizeof Str,"EOF %d on input file",dfx);
		  log_to_file(Str);
		  if (RW_flag < 0)
		    printf("\nmds02 -- EOF on save file\n");
	       }
	     
	     p_term(0);
	  }
	p_mi->total_p++;		/* count total */

/* ignore succeeding identical messages */

	k = 0;				/* flag */
	for (i=0; i<(sizeof Dbuf); i++)
	  {
	     if (Dbuf[i] != Dbuf_o[i]) k = 1; /* remember difference */
	     Dbuf_o[i] = Dbuf[i];	/* save */
	  }
	if (k == 0) continue;		/* ignore if same */

/* set time of data in realtime*/

	if (RW_flag >= 0) D_tim = time(NULL);

	dfp = &Dbuf[0];			/* pointer for convenience */
	
/* all data to save file if wanted */
	
	if (RW_flag == 1) save_data();

/* decoding starts now */
	
//	printf("%2d\n",dfx);

/* --- DF11 DF17 DF18 DF19 --- */
	
	if ((dfx == 11) || (dfx == 17) || (dfx == 18) || (dfx == 19))
	  {

	/* comon */
	     				/* parity check */
	     if (dfx == 11) cs = RemPar(dfp,&r,56);
	     else cs = RemPar(dfp,&r,112);
	     
	     clic = (cs^r);

//	     if (dfx != 11)
//	       printf("%.2d %.2d %.2d\n",dfx,FetchFTC(dfp+4,&uc),clic);

#ifdef INTERROG
					/* check for known interrogator */
	     if (clic != 0)
	       {
		  if (known_clic(clic) == 0) continue;
	       }
#else
					/* c-sum must be 0, ignore otherwise */
	     if (clic != 0) continue;
	     
#endif

	     k = SepAA(dfp);		/* icao address */
	     if ((p=p_icao(k)) == NULL) /* if not seen before ... */ 
	       {
		  			/* ... look for empty structure */
		  if ((p=p_empty()) == NULL) continue;
		  p->icao = k;		/* save icao addess */
	       }

	     p->utms = 0;		/* mark working on */
	     
#ifdef INTERROG
	     p->clic = clic&CLIC_MASK;	/* interrogator */
#endif
	     
	     n = Sep0608(dfp);	 	/* ca/cf/af */
	     
	     if (dfx < 18) p->ca = n;	/* is ca */

//	     if (dfx > 17) printf("\007\n");

	/* for DF11 only */
	     
	     if (dfx == 11)
	       {
		  df_fin(p,dfx);	/* all done for DF11 */
		  continue;
	       }

	/* for DF17 DF18 DF19 */

	     if (dfx == 18) p->cf = n;
	     if (dfx == 19) p->af = n;
	     				/* no interpretation of DF18 and DF19
					 * with cf/af != 0 */
	     if ((dfx != 17) && (n != 0))
	       {
		  df_fin(p,dfx);
		  continue;
	       }
	     

	     dfp += 4;			/* now point to ME */
	     
	     ftc = FetchFTC(dfp,&uc);

	     /* FTC 0 */
	     
	     if (ftc == 0)		/* altitude only */
	       {
		  p->sstat = FetchSS(dfp);  /* surveill. status */
		  p->nicsb = FetchNICSB(dfp);  /* nic supl. b  */
		  p->tbit = FetchTBit(dfp);  /* t bit */

		  n = FetchEAC(dfp);	
		  p->alt = AC2FT(n,&p->alt_c);

		  p->alt_s = dfx;
		  p->alt_ftc = ftc;
		  p->alt_t = D_tim;	/* mark last alt update */
	       
		  p->lat = p_2_empty->lat;	/* lat lon unavailable */
		  p->lon = p_2_empty->lon;
		  p->ll_t = p->alt_t;
		  p->pos_ftc = ftc;


		  df_fin(p,dfx);
		  continue;
	       }

	     /* FTC 1 .. 4 */
	     
	     if (ftc <= 4)		/* identification */
	       {
		  p->acc = uc;		/* category */
		  p->acc_ftc = ftc;
		  MB2Ident(dfp+1, p->acident); /* make ident string */
		  
		  df_fin(p,dfx);
		  continue;

	       }

	     /* FTC 19 */
	     
	     if (ftc == 19)	     	/* airborn velocity */
	       {	     	     

//		  printf("%2d %2d\n",
//			 ((*dfp)>>3)&0x1f, (*dfp)&0x7);

					/* data to plane structure direct */
		  if (AirbVel(dfp, p))
		    {
		       log_to_file("illegal subtype in DF17/18/19 FTC01..4");
		       p->utms = D_tim;
		       continue;
		    }

		  p->cs_t = D_tim;
		  
		  df_fin(p,dfx);
		  continue;
	       }
	     
	     /* FTC 5 .. 22 */
	     
	     if ((ftc >= 5) && (ftc <= 22))	/* position */
	       {
		  g_fl = 0;	      		/* assume airborne */
		  if (ftc < 9) g_fl++;		/* on surface */
		  p->g_flag = g_fl;

		  if (!g_fl) 			/* airborne ... */
		    {
		       p->sstat = FetchSS(dfp);  /* surveill. status */
		       p->nicsb = FetchNICSB(dfp);  /* nic supl. b  */
		       p->tbit = FetchTBit(dfp);  /* t bit */
		    }
		  else
		    {
		       n = FetchMOV(dfp);	/* movement on surface */
		       p->sogc = MOV2SPD(n);
		       				/* track on surface */
		       p->cogc = GTRKHDG(dfp);
		    }

		  CPRfromME(dfp,&p->pos);

		  if (p->pos.t_s == 0)		/* need ref. point */
		    {
#ifdef GLOBAL_R
						/* find global absolut pos. 
						 *  from odd and even */
		       l = (p->pos.cprf+1)&0x1;
		       if (p->pos.t[l] != 0)	/* if both exist ... */
			 {
			    n = CPR2LL(&p->pos,g_fl); /* ... find pos. */
			    if (n == 0) 
			      {
				 p->lon = p->pos.lon_s;
				 p->lat = p->pos.lat_s;
				 p->ll_t = p->pos.t_s;
				 p->pos_ftc = ftc;  /* position source ftc */
			      }
			    
			 }
		       else
			 {
			    df_fin(p,dfx);
			    continue;
			 }
		       
#else
						/* set ref point to observer
						 *  position */
		       p->pos.lat_s = O_POS_LAT;
		       p->pos.lon_s = O_POS_LON;
		       p->pos.t_s = 1;
#endif		       
		    }
		  else				/* pos with ref. */
		    {
		       if ((n = RCPR2LL(&p->pos,g_fl)) < 0)
			 {
			    snprintf(Str,sizeof Str,"RCPR2LL error %d",n);
			    log_to_file(Str);;
			 }
		       if (p->pos.t_s > 1)
			 {
			    p->lon = p->pos.lon_s;
			    p->lat = p->pos.lat_s;
			    p->ll_t = p->pos.t_s;
			    p->pos_ftc = ftc;	/* position source ftc */
			 }
		       
		    }
		  
		  if (!g_fl)
		    {				/* real altitude */
		       n = FetchEAC(dfp);
		       p->alt = AC2FT(n,&p->alt_c);
		    }
		  else
		    {				/* no altitude info */
		       p->alt = p_2_empty->alt;
		       p->alt_c = p_2_empty->alt_c;
		    }
		  p->alt_s = dfx;
		  p->alt_ftc = ftc;
		  p->alt_t = D_tim;		/* mark last alt update */
		  
		  df_fin(p,dfx);
		  continue;
	       }

	     /* FTC 28 */
	     
	     if (ftc == 28)		/* only subtype 1 decoded */ 
	       {

		  if (uc = 1)
		    {
		       p->ems = FetchEMS(dfp+1); 
		       p->id = N2SQ(Sep2032(dfp-1));  /* Mode A, bits 12..24*/
		    }
		  df_fin(p,dfx);
		  continue;
	       }

	     /* FTC 23 .. 27, FTC 29 ... */
	     
//	     printf("%c %d %6.6x \n",'\007',ftc,p->icao);

	     p->utms = D_tim;		/* mark structure no longer in use */
	  }
	
/* --- DF04 DF05 DF20 DF21 ---
 * 	no interpretation of (unknown) MB message in DF20 and DF21 */
	
	if ((dfx == 4) || (dfx == 5) || (dfx == 20) || (dfx ==21))
	  {
					/* find icao address */
	     if (dfx < 20) k = AAfromAP56(dfp);
	     else k = AAfromAP112(dfp); 
	     				/* address must be known */
	     if ((p=p_icao(k)) == NULL) continue;

	     p->utms = 0;		/* mark working on */

	     p->fs = Sep0608(dfp);	/* flight status */
	     p->dr = SepDR(dfp);	/* downlink request */
	     p->um = SepUM(dfp);	/* utility message */
	     
	     if ((dfx == 4) || (dfx == 20)) /* DF04 and DF20 specific */
	       {
		  ac = Sep2032(dfp);	/* altitude code */
		  p->alt = AC2FT(ac, &p->alt_c);
		  p->alt_s = dfx;	/* update source ... */
		  p->alt_ftc = p_2_empty->alt_ftc;
		  p->alt_t = D_tim;	/* ... and time */

	       }

//	     if ((dfx == 5) || (dfx == 21))  /* in DF05 and DF21 only */
	     else
	       {
		  p->id = N2SQ(Sep2032(dfp));  /* squawk */

	       }

#ifdef BDS_DIRECT
	     
	     if ((dfx == 20) || (dfx == 21))  /* in DF20 and DF21 only */
	       {
		  dfp += 4;
		  n = DECBDS40(dfp,p);
//		  if (n == 0) printf("40: %6.6x\n",k);
		  if (n < 0) 
		    {
		       n = DECBDS50(dfp,p);
//		       if (n == 0) printf("50: %6.6x\n",k);
		    }
		  if (n < 0) 
		    {
		       n = DECBDS60(dfp,p);
//		       if (n == 0) printf("60: %6.6x\n",k);
		    }
	       }

#endif
	     
	     df_fin(p,dfx);
	     continue;
	  }
	
/* --- DF00 --- */
     
	if (dfx == 0) 
	  {
					/* for known icao address only */
	     if ((k = AAfromAP56(dfp)) <= 1 ) continue;
	     if ((p=p_icao(k)) == NULL) continue;
	     
	     p->utms = 0;		/* mark working on */

	     p->vs = SepVS(dfp);	/* vertical status 0/1 */
	     p->cc = SepCC(dfp);	/* crosslink cap. */
	     p->ri = SepRI(dfp);	/* reply info */
	     
	     ac = Sep2032(dfp);		
	     p->alt = AC2FT(ac, &p->alt_c);
	     p->alt_s = dfx;		/* mark altitude source and time */
	     p->alt_ftc = p_2_empty->alt_ftc;
	     p->alt_t = D_tim;
     
	     df_fin(p,dfx);
	     continue;
	  }
     }
   exit(0);
}
   
