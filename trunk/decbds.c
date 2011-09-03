/* 'decbds' direct bds decoding for 'mds'
 *
 * used for monitor message field in DF20 and DF21
 * 
 * need defined 'BDS_DIRECT' for compilation (from 'mds02.h')
 * 
 *
 * rules for decoding on the basics of:
 * 
 * R.D.Grappel et al.: "Elementary Surveillance (ELS) and
 *  Enhanced Surveillance (EHS) Validation via Mode S
 *  Secondary Radar Surveillance"
 *  MIT, Lincoln Lab., ATC-337, 2008
 * 
 */
/*
    Copyright (C) 2011  Meinolf Braeutigam

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
 *  --- alpha test version mbr20110901 ---
 * ------------------------------------------------------------------------
 * 
 */   

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include "mds02.h"

#ifdef BDS_DIRECT

/*
 * ------------------------------------------------------------------------
 * decoding of DF20/DF21 MB field
 * 	BDS4.0 BDS5.0 or BDS6.0
 * 
 * b points to message buffer
 * p points to structure of plane with valid icao address
 * 
 * return 0 on success and data filled in structure *p
 *   "   <0 if data seem to be invalid 
 *
 * data in structure *p are changed only if associated data are present
 *   and checks for practical range have been passed 
 */

/*
 * BDS 4.0 has vertical intention and baro setting
 * 
 * error return:
 * 	-40	   no data at all
 * 	-41 .. -45 initial status bit check failure
 * 	-46	   reserved field check failure
 */ 

int DECBDS40(unsigned char *b, struct Plane *p)
{
   long int dd;
   double x;
   
/* initial check for BDS 4.0
   
/* at least one status bit set */

   if ( ((*b&0x80) == 0)		/* m-bit  1 */
	&& ((*(b+1)&0x4) == 0)		/*       14 */
	&& ((*(b+3)&0x20) == 0)		/*       27 */
	&& ((*(b+5)&0x1) == 0)		/*       48 */
	&& ((*(b+6)&0x4) == 0)		/*       54 */ 
      )
     return -40;			/* no data at all */   

/* on cleard status bit data have to be 0 */
   
   if ((*b&0x80) == 0)			/* m-bit  1 */
     {
	if ( (*b != 0) 
	     || ((*(b+1)&0xf8) != 0)
	   ) return -41;		/* status = 0 but data present */
     }
      
   if ((*(b+1)&0x4) == 0)		/*       14 */
     {
	if ( ((*(b+1)&0x3) != 0)
	     || (*(b+2) != 0)
	     || ((*(b+3)&0xc) != 0)
	   ) return -42;
     }
   
   if ((*(b+3)&0x20) == 0)		/*       27 */
     {
	if ( ((*(b+3)&0x1f) != 0)
	     || ((*(b+4)&0xfe) != 0)
	   ) return -43;
     }
   
   if ((*(b+5)&0x1) == 0)		/*       48 */
     {
	if ((*(b+6)&0xe0) != 0)
	  return -44;
     }
      
   if ((*(b+6)&0x4) == 0)		/*       54 */ 
     {
	if ((*(b+6)&0x3) != 0)
	  return -45;
     }
   
/* reserved fields should be 0 */

   if ( ((*(b+4)&0x1) != 0)		/* m-bit  40 */
        || ((*(b+5)&0xfe) != 0)		/* m-bits 41 .. 47 */
	|| ((*(b+6)&0x18) != 0) )	/* m-bits 52 .. 53 */
     return -46;
   
/* FCU selected altitude (m-bits 2 .. 13) */

   if ((*b&0x80) != 0)			/* m-bit  1, status  */
     {   
	dd = *b;			/* get m-bits 1 .. 16 */
	dd <<= 8;
	dd |= *(b+1);
	dd >>= 3;			/* right justify m-bits 1 .. 13 */
	dd &= 0xfff;			/* mask m-bits 2 .. 13*/
	dd <<= 4;			/* scaling (*16) */
					/* check for practical limits */
	if ((dd < 50000) && (dd > 100) )
	  p->bds.fcu_alt_40 = (double)dd;
	
	
     }
   
/* FMS selected altitude (m-bits 15 .. 26) (see above) */

   if ((*(b+1)&0x4) != 0)		/*m-bit 14, status */
     {
	dd = *(b+1);
	dd <<= 8;
	dd |= *(b+2);
	dd <<= 8;
	dd |= *(b+3);
	dd >>= 6;
	dd &= 0xfff;
	dd <<= 4;

	if ( (dd < 50000) && (dd > 100) )
	  p->bds.fms_alt_40 = (double)dd;
     }
   
/* baro settings (m-bits 28 .. 39) */
   
   if ((*(b+3)&0x20) != 0)		/*m-bit 27, status */
     {
	dd = *(b+3);
	dd <<= 8;
	dd |= *(b+4);
	dd >>= 1;
	dd &= 0xfff;
	x = (double)dd/10. + 800.;	/* scaling */
	if ( (x > 950.) && (x < 1100.) )
	  p->bds.baro_40 = x;
     }

/* mode flags */
   
   if ((*(b+5)&0x1) != 0)		/*m-bit 48, status */
     {
	if ((*(b+6)&0x80) != 0) p->bds.vnav_40 = 1;
	else p->bds.vnav_40 = 0;

	if ((*(b+6)&0x40) != 0) p->bds.alt_h_40 = 1;
	else p->bds.alt_h_40 = 0;
	
	if ((*(b+6)&0x20) != 0) p->bds.app_40 = 1;
	else p->bds.app_40 = 0;
     }
   
/* target altitude source */

   if ((*(b+6)&0x4) != 0)		/*m-bit 54, status */ 
	p->bds.alt_s_40 = *(b+6)&0x3;
   
/* mark data available */
   
   p->bds.l_bds = 40;			/* last decoded bds */
   p->bds.t_bds = time(NULL);		/* actual time for that */
   
   return 0;
   
}

/*
 * BDS 5.0 has roll angle, true track, ground speed, track angle rate,
 * 	true airspeed (tas)
 * 
 * error return:
 * 	-50	   no data at all
 * 	-52 .. -55 initial status bit /data check failure
 * 
 */

int DECBDS50(unsigned char *b, struct Plane *p)
{
   long int dd;
   double x;
   
/* check for BDS5.0 */
   
/* at least 1 status bits must be set */

   if( ((*(b)&0x80) == 0)	    	/* m-bit  1 */
       && ((*(b+1)&0x10) == 0)		/*       12 */
       && ((*(b+2)&0x1) == 0)		/*       24 */
       && ((*(b+4)&0x20) == 0)		/*       35 */
       && ((*(b+5)&0x4) == 0))		/*       46 */
     return -50;
   
/* if status cleared, data must be 0 */
   
   if ((*b&0x80) == 0)			/* m-bit  1 */
     {
	if ( (*b != 0) 
	     || ((*(b+1)&0xe0) != 0)
	   ) return -51;		/* status = 0 but data present */
     }
      
   if ((*(b+1)&0x10) == 0)		/* m-bit 12 */
     {
	if ( ((*(b+1)&0xf) != 0)
	     || ((*(b+2)&0xfe) != 0)
	   ) return -52;
     }
   
   if ((*(b+2)&0x1) == 0)		/* m-bit 24 */
     {
	if ( (*(b+3) != 0)
	     || ((*(b+4)&0xc0) != 0)
	   ) return -53;
     }
   
   if ((*(b+4)&0x20) == 0)		/* m-bit 35 */
     {
	if ( ((*(b+4)&0x1f) != 0)
	     || ((*(b+5)&0xf8) != 0)
	   ) return -54;
     }
   
   if ((*(b+5)&0x4) == 0)		/* m-bit 46 */
     {
	if ( ((*(b+5)&0x3) != 0)
	     || (*(b+6) != 0)
	   ) return -55;
     }
   
/* roll angle (m-bits 2 .. 11) */
   
   if ((*(b)&0x80) != 0)	    	/* m-bit  1 */
     {
	dd = *b;
	dd <<=8;
	dd |= *(b+1);
	dd >>= 5;
	dd &= 0x3ff;			/* m-bits 2 .. 11, right justif. */
	if ((dd&0x200) != 0) dd |= ~0x3ff; /* sign externd */
	x = (double)dd * (45./256.);    /* scaling +90 .. -90 degrees */
	if (fabs(x) < 30.)		/* check for practical range */
	  p->bds.roll_50 = x;
     }

/* track angle (m-bits 13 .. 23) */

   if ((*(b+1)&0x10) != 0)		/* m-bit 12 */
     {   
	dd = *(b+1);
	dd <<= 8;
	dd |= *(b+2);
	dd >>= 1;
	dd = dd&0x7ff;
	if ((dd&0x400) != 0) dd |= ~0x7ff; /* sign extend */

	x = (double)dd * (90./512.);	/* scaling to +180 .. -180 degree */
	if (x < 0.) x += 360.;
	p->bds.track_50 = x;
     }
   
/* ground speed (m-bits 25 .. 34) */
   
   if ((*(b+2)&0x1) != 0)		/* m-bit 24 */
     {
	dd = *(b+3);
	dd <<= 8;
	dd |= *(b+4);
	dd >>= 6;
	dd &= 0x3ff;			/* m-bits 25 .. 34, right justif. */
	dd *= 2;			/* scaling to 0 .. 2046 kn */
	if ((dd < 600) && (dd > 40))	/* practical speed limits 40 .. 600 */
	  p->bds.g_speed_50 = (double)dd;
     }
   
/* track angle rate (m-bits 36 .. 45) */

   if ((*(b+4)&0x20) != 0)		/* m-bit 35 */
     {
	dd = *(b+4);
	dd <<= 8;
	dd |= *(b+5);
	dd >>= 3;
	dd &= 0x3ff;
	if ((dd&0x200) != 0) dd |= ~0x3ff; /* sign externd */
	p->bds.t_rate_50 = 
	  (double)dd * (8./256.);	/* scaling to +16 .. -16 degree */
     }
   
   
/* true airspeed (m-bits 47 .. 56) */

   if ((*(b+5)&0x4) != 0)		/* m-bit 46 */
     {
	dd = *(b+5);
	dd <<= 8;
	dd |= *(b+6);
	dd &= 0x3ff;
	dd *= 2;			/* scaling to 0 .. 2046 kn */
	if ((dd < 600) && (dd > 40))	/* check for practiacal range */
	  p->bds.tas_50 = (double)dd;
     }
   
/* data ok */

   p->bds.l_bds = 50;			/* last decoded bds */
   p->bds.t_bds = time(NULL);		/* actual time for that */
   
   return 0;
}

/*
 * BDS6.0 has magn. heading, ind. airspeed (ias), mach,
 * 	barometric v-speed, inertial v-speed
 * error return:
 *	-60	no data at all
 *	-61	initial status bit check
 * 	-62	ias out of practical range
 * 	-63	baro and inertial vertical rate differ to much
 * 
 */ 

int DECBDS60(unsigned char *b, struct Plane *p)
{
   long int dd;
   double x;
   
/* check for BDS6.0 */
	
   if( ((*b&0x80) == 0)			/* m-bit  1 */
       && ((*(b+1)&0x8) == 0)		/*       13 */
       && ((*(b+2)&0x1) == 0)           /*       24 */
       && ((*(b+4)&0x20) == 0)          /*       35 */
       && ((*(b+5)&0x4) == 0)           /*       46 */
     ) 
     return -60;
   
/* if status cleared, data must be 0 */
   
   if ((*b&0x80) == 0)			/* m-bit  1 */
     {
	if ( (*b != 0) 
	     || ((*(b+1)&0xf0) != 0)
	   ) return -61;		/* status = 0 but data present */
     }
      
   if ((*(b+1)&0x8) == 0)		/* m-bit 13 */
     {
	if ( ((*(b+1)&0x7) != 0)
	     || ((*(b+2)&0xfe) != 0)
	   ) return -62;
     }
   
   if ((*(b+2)&0x1) == 0)		/* m-bit 24 */
     {
	if ( (*(b+3) != 0)
	     || ((*(b+4)&0xc0) != 0)
	   ) return -63;
     }
   
   if ((*(b+4)&0x20) == 0)		/* m-bit 35 */
     {
	if ( ((*(b+4)&0x1f) != 0)
	     || ((*(b+5)&0xf8) != 0)
	   ) return -64;
     }
   
   if ((*(b+5)&0x4) == 0)		/* m-bit 46 */
     {
	if ( ((*(b+5)&0x3) != 0)
	     || (*(b+6) != 0)
	   ) return -65;
     }
   

/* magnetic heading (m-bits 2 .. 12) */
   if ((*b&0x80) != 0)
     {
	dd = *(b);
	dd <<= 8;
	dd |= *(b+1);
	dd >>= 4;
	dd &= 0x7ff;
	if ((dd&0x400) != 0) dd |= ~0x7ff; /* sign extend */
   
	x = (double)dd*90./512.;	/* heading in degree */
	if (x < 0.) x += 360.;
	p->bds.hdg_60 = x;
     }

/* indicated airspeed ias (m-bits 14 .. 23) */

   if ((*(b+1)&0x8) != 0)		/* m-bit 13 */
     {
	dd = *(b+1);
	dd <<= 8;
	dd |= *(b+2);
	dd >>= 1;
	dd &= 0x3ff;
	if ((dd < 600) && (dd > 40))	/* practical speed limits 40 .. 600 */
	  p->bds.ias_60 = (double)dd;	/* speed in kn */
     }
	
/* mach (m-bits 25 .. 34) */	

   if ((*(b+2)&0x1) != 0)		/* m-bit 24 */
     {	
	dd = *(b+3);
	dd <<= 8;
	dd |= *(b+4);
	dd >>= 6;
	dd &= 0x3ff;
	dd *= 4;			/* scaling (milli-mach) */
	if (dd < 1000)			/* practical range <1 mach */
	  p->bds.mach_60 = (double)dd/1000.;
     }
       
	
/* barometric altitude rate (m-bits 36 .. 45) */

   if ((*(b+4)&0x20) != 0)		/* m-bit 35 */
     {	
	dd = *(b+4);
	dd <<=8;
	dd |= *(b+5);
	dd >>= 3;	
	dd &= 0x3ff;		
	if ((dd&0x200) != 0) dd |= ~0x3ff; /* sign extend */
	p->bds.vert_b_60 = (double)dd*32.;
     }
	
/* inertial vertical velocity (m-bits 47 .. 56) */
   
   if ((*(b+5)&0x4) != 0)		/* m-bit 46 */
     {
	dd = *(b+5);
	dd <<= 8;
	dd |= *(b+6);
	dd &= 0x3ff;
	if ((dd&0x200) != 0) dd |= ~0x3ff;
	p->bds.vert_i_60 = (double)dd*32.;
     }

/* data ok */
   
   p->bds.l_bds = 60;			/* last decoded bds */
   p->bds.t_bds = time(NULL);		/* actual time for that */
   
   return 0;

}

#endif
					/* end BDS_DIRECT */
