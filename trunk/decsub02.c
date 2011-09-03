/* subroutined for 'PlaneGadget' decoding in 'modes02'
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
 * -------------------------------------------------------------------------
 *  --- alpha test version 20110721 ---
 * -------------------------------------------------------------------------
 * */


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <errno.h>
#include <time.h>

#include "mds02.h"
//#include "decsub02.h"

#define _GNU_SOURCE 1

//unsigned char Dbuf[14];			/* raw input buffer */

/*
 * -------------------------------------------------------------------------
 * left shift 1 place buffer of unsigned character pointed to by 'b' and
 * length 'n'
 * return carry bit shifted out
 * -------------------------------------------------------------------------
 * */

unsigned int LeftShift(unsigned char *b, int n)
{ 
   int m;				/* byte index pointer */
   int msb = 0x80;			/* most significant bit */
   int c = 0;				/* carry bit */

   if ((*b & msb) != 0) c = 1;		/* carry bit */
   *b <<= 1;				/* left shift ms byte */

   for (m=1; m<n; m++)
       {
	  if ((*(b+m) & msb) != 0) *(b+m-1) |= 1; /* carry to next */
	  *(b+m) <<= 1;
       }
   return c;
}

/* 
 * ------------------------------------------------------------------------
 * routines to seperate data from input buffer into a single 
 *	unsigned long integer (at least 32 bits)
 * data in buffer are at fix position
 * 
 * code optimized for speed
 * (bits in buffer are numbered from 1)
 * ------------------------------------------------------------------------
 * */

/* bits 6-8 (3) for different purposes */
/* CA, capability in DF11 DF17 */
/* FS, flight status in DF04 */
unsigned int Sep0608(unsigned char *bufp)
{
   return (*bufp & 0x7);
}

/* bits 20-32 (13) for different purposes */
/* AC, altitude code in DF04 */
/* ID, identity in DF05 */
unsigned long int Sep2032(unsigned char *bufp)
{
   unsigned long int dd;
   
   dd = *(bufp+2);
   dd <<= 8;
   dd |= *(bufp+3);
   
   return (dd&0x1fff);
}

/* AA, address announced in DF11 DF17 DF18, bits 9-32 (24) 
 * 3 byte at byte boundary */
unsigned long int SepAA(unsigned char *bufp)
{
   unsigned long int dd;

   dd = *(bufp+1);
   dd <<= 8;
   dd |= *(bufp+2);
   dd <<= 8;
   dd |= *(bufp+3);

   return (dd&0xffffff);		/* return 24 bits */
}

/* AP, address/parity in DF04 DF05, bits 33-56 (24) */
/*	assume same format as AA but different place */
unsigned long int SepAP56(unsigned char *bufp)
{
   return (SepAA(bufp+3));
}

/* AP, address/parity in DF16 bits 89-112 (24) */
/*	assume same format as AA but different place */
unsigned long int SepAP112(unsigned char *bufp)
{
   return (SepAA(bufp+10));
}

/* PI, parity/interrogator in DF11 DF17 DF18, bits 33-56 (24) */
unsigned long int SepPI(unsigned char *bufp)
{
   return (SepAA(bufp+3));		/* return 24 bits */
}

/* DR, downlink request in DF04, bits 9-13 (5) */
unsigned int SepDR(unsigned char *bufp)
{
   unsigned long int dd;
   
   dd = *(bufp+1) >> 3;
   return (dd&0x1f);
}

/* UM, utility message in DF04, bits 14-19 (6) */
/*	(contains subfields ISS and IDS) */
unsigned int SepUM(unsigned char *bufp)
{
   unsigned long int dd;
   
   dd = (*(bufp+1)&0x7)<<3;
   dd |=(*(bufp+2)>>5)&0x7;
   return (dd&0x3f);

}

/* VS, vertical status in DF00, bit 6 (1) */
unsigned int SepVS(unsigned char *bufp)
{
   unsigned int dd;
   
   dd = *bufp&0x4;
   dd >>= 3;
   return (dd&0x1);
}

/* CC, cross-link capability in DF00, bits 7-8 (2) */
unsigned int SepCC(unsigned char *bufp)
{
   return (*bufp&0x1);
}

/* RI, reply information in DF00, bits 14-17 (4) */
unsigned int SepRI(unsigned char *bufp)
{
   unsigned int dd;
   
   dd = *(bufp+1)&0x7;
   dd <<= 1;
   if ((*(bufp+2)&0x80) != 0) dd |= 0x1;
   
   return dd&0xf;
}
   

/* ------------------------------------------------------------------------
 * find remainder for decoding PI and AP field
 * in 56 or 112 bit ('nb' bits) message 
 *   pointed to by '*b'
 * return remainder in '*r' and last 24 bits from buffer as function value
 * terminates with fatal error, if nb is not 56 and not 112
 * 	source: implementation of hardware solution shown in
 *      ACASA/WP7.4/150 Fig. 4.7-7 and others
 * ------------------------------------------------------------------------ */
unsigned long int RemPar(unsigned char *b, unsigned long int *r, int nb)
{
   unsigned char buf[17];		/* 14 bytes + 3 for shift out */
   unsigned char  g[3] = {0xff, 0xf4, 0x08};  /* generator polynom descr. */
   unsigned int n,c,i,s,cs,bs,br,bl;
   
   
   if ((nb != 56) && (nb != 112))
       {
	  fprintf(stderr,
		  "\ndecsub RemPar -- fatal parameter error nb=%d\n\n",nb);
	  exit(1);
       }

   *r = 0;
   cs = 0;
   
   buf[0] = 0;
   buf[1] = 0;
   buf[2] = 0;
   
   br = nb-25;				/* shift count for remainder */
   bl = (nb/8)-3;			/* buffer length in bytes */
   bs = bl+3;				/* bytes to shift */
   
/* fill internal buffer */

   for (n=0; n<bl; n++)	buf[n+3] = *(b+n);
   
/* polynom division */
   
   for (n=0; n<=br; n++)
     {
	c = LeftShift(&buf[0],bs);
	i = buf[2] & 0x1;
	s = (i^c) & 0x1;
	buf[2] = (buf[2]&0xfe) | s;
	
	if (s != 0)
	  {
	     buf[0] ^= g[0];
	     buf[1] ^= g[1];
	     buf[2] ^= g[2];
	  }
     }
/* save remainder after 32/88 shifts */
   *r = (buf[0]<<16) | (buf[1]<<8) | buf[2];
/* return last 24 bits of buffer */
   if (nb == 56) cs = SepAP56(b);
   else cs = SepAP112(b);
   return cs;
   
}
   
/*
 * ------------------------------------------------------------------------
 * specific routines for data fields in received buffer
 * 
 * the buffer pointer in subroutine call points to the specific ME
 * 	datafield already
 * ------------------------------------------------------------------------
 * */

/* Format Type Code (and subtype or other info from fist byte of ME in DF17, 
 * bits 1-5 (5), 6-8 (3)
 * */
unsigned int FetchFTC(unsigned char *b, unsigned char *s)
{
   *s = *b&0x7;
//   return (*b&0xf8)>>3;
   return ((*b)>>3)&0x1f;
}

/* 
 * fetch surveillance status from DF17 ME 'airborne position message'
 * 	bits 6..7 (2)
 * */
unsigned int FetchSS(unsigned char *b)
{
   return (*b&0x6)>>1;
}

/* 
 * fetch nic supplement b  from DF17 ME 'airborne position message'
 * 	bit 8 (1)
 * */
unsigned int FetchNICSB(unsigned char *b)
{
   return (*b&0x1);
}

/* 
 * fetch t bit from DF17 ME 'airborne position message'
 *	bit 21 (1)
 * */
unsigned int  FetchTBit(unsigned char *b)
{
   return (*(b+2)&0x8)>>3;		/* time bit */
}

/* 
 * fetch and return cpr format bit 22 from DF17 ME 'position message'
 * 	pointed to by '*b'
 * */
unsigned int FetchCPRF(unsigned char *b)
{
   return (*(b+2)&0x4)>>2;		/* odd/even cpr format */
}

/* fetch encoded latitude from DF17 ME 'airborne position message' pointed
 * to by '*b'
 * return me bits 23..39 (17) as long int
 * */
unsigned long int FetchELat(unsigned char *b)
{
   unsigned long int dd;
   
   dd = *(b+2)&0x3;			/* me bits 23..24 */
   dd <<= 8;
   dd |= *(b+3);			/*    "    25..32 */
   dd <<=7;
   dd |= (*(b+4)&0xfe)>>1;		/*    "    33..39 */
   
   return dd&0x1ffff;
}

/* fetch encoded longitude from DF17 ME 'airborne position message' pointed
 * to by '*b'
 * return me bits 40..56 (17)
 * */
unsigned long int FetchELon(unsigned char *b)
{
   unsigned long int dd;
   
   dd = *(b+4)&0x1;			/* me bit  40 */
   dd <<= 8;
   dd |= *(b+5);			/* me bits 41..48 */
   dd <<=8;
   dd |= *(b+6);			/*    "    49..52 */
   
   return dd&0x1ffff;
}

/* fetch modified altitude code AC from DF17 ME 'airborne position message'
 * pointed to by '*b'
 * return me bits 9..20 (12) with reconstructed M bit as altitude code AC
 * */
unsigned long int FetchEAC(unsigned char *b)
{
   unsigned long int dd, ddh;
   
   dd = *(b+1);				/* me bits 9..16 */
   dd <<= 8;
   dd |= *(b+2);			/* me bits 17..24 */
   dd >>= 4;				/* bits 9..20 */

					/* rebuild AC with M-bit */
   ddh = (dd&0xfc0)<<1;			/* high order 6 bits */
   dd &= 0x3f;				/* low order 6 bits */
   dd |= ddh;
      
   return dd;
}

/* fetch movement MOV from DF17, FTC5..8 (surface position) ME bits 6..12 */
unsigned long int FetchMOV(unsigned char *b)
{ 
   unsigned long int dd;
   
   dd = *b;
   dd <<= 8;
   dd |= *(b+1);
   dd >>= 4;
   return dd&0x7f;
}

/* fetch 'emergency state' DF17, FTC28, ME bits 9..11 */
unsigned long int FetchEMS(unsigned char *b)
{
   unsigned long int dd;
   
   dd = *b;
   dd >>= 5;
   return dd&0x7;
}

/*
 * ------------------------------------------------------------------------
 * fetch and convert ground track GTRK from DF17, FTC5..8 (surface position)
 *	ME bits 13,14..20 
 * return heading in degree as double (0..<360.0) or value from
 *      empty structure if unknown 
 * ------------------------------------------------------------------------
 * */
double GTRKHDG(unsigned char *b)
{
   struct Plane *p0;

   unsigned long int dd;
   
   dd = *(b+1);
   dd <<= 8;
   dd |= *(b+2);
   dd >>= 4;
   if (dd&0x80) 
     {
	p0 = p_2_0();			/* pointer to empty structure */
	return p0->cogc;		/* 'unknown' value */
     }
   return (double)(dd&0x7f)*360./128.0;
  }

/*
 * ------------------------------------------------------------------------
 * convert 8 characters from buffer '*b' of DF17 field ME with FTC01..FTC04
 *	to terminated ascii sting in buffer '*s'
 * 	(ascii buffer has to be at leat 9 char long - not checked)
 * return 0 if ok, 1 if undefined char found
 * (split into 2 conversions, each starting at byte boundary)
 * ------------------------------------------------------------------------
 * */
/* the function for internal use: */
void MB2Ident_2(unsigned char *b, char *s)
{
   unsigned int c;
/* seperate 4 char of 6 bit each */
   *s = ((*b)&0xfc)>>2;

   c = ((*b)&0x3)<<4;
   c |= (*(b+1)&0xf0)>>4;
   *(s+1) = c;
   
   c = (*(b+1)&0xf)<<2;
   c |= (*(b+2)&0xc0)>>6;
   *(s+2) = c;
   
   *(s+3) = *(b+2)&0x3f;
}

/* ------------------------------------------------------------------------*/
/* the function for external use: */
int MB2Ident(unsigned char *b, char *s)
{
   unsigned char c;
   int r;
   int n;

   MB2Ident_2(b,s);			/* first 3 bytes */
   MB2Ident_2(b+3, s+4);		/* next 6 bytes */
   
   r = 0;
   
   for (n=0; n<8; n++)
     {
	c = *(s+n);			/* character range 01..1a,20,30..39 */
	if ((c>0x39) || (c<0x01)) r = 1;
	if ((c>0x20) && (c<0x30)) r = 1;
	if ((c>0x1a) && (c<0x20)) r = 1;
	if (c < 0x20) *(s+n) += 0x40; /* 6 bit to ascii */
     }
   *(s+8) = '\000';			/* string terminator */
   return r;
}

/*
 * ------------------------------------------------------------------------
 * fill structure 'AVel' from DF17 field ME with FTC19 pointed to by '*b'
 * 
 * writes structure Plane *p members:
 * 	int verts
 *	double vert
 * 	double alt_d
 * 	double veast
 * 	double vnorth
 * 	double cogc
 * 	double sogc
 * 	double hdg
 * 	double tas
 * 	double ias
 * 
 * 
 * returns 0 on success
 *         1 on subtype error
 * ------------------------------------------------------------------------
 * */
int AirbVel(unsigned char *b, struct Plane *p)
{
   int m,n;
   int subt;				/* ftc subtype */
   unsigned long int l;
   double sf;				/* scale factor */
   double x;

   subt = *b&0x7;			/* subtype 1..4 (bits 6..8) */
   if ((subt > 4) || (subt <= 0)) return 1;

/* common to all subtypes */

   /* some flags */
   if (*(b+4)&0x10 != 0) p->verts = 1; /* vertical source (bit 36) */

   /* vertical rate (in sign magnitude) to ft/min */
   
   l = *(b+4)<<8;
   l |= *(b+5);
   l >>= 2;
   l &= 0x1ff;
   if (l != 0)
     {
	p->vert = (double)((l-1)*64);		/* rate in ft/min */
	if ((*(b+4)&0x8) != 0) p->vert = -p->vert; /* if sign flag set */
     }

   /* altitude difference */
   m = *(b+6)&0x7f;			/* GNSS-BARO (bits 49..56) */
   if (m != 0)
     {
	m -=1;
	p->alt_d = (double)m * 25.; 
	if (*(b+6)&0x80) p->alt_d = -p->alt_d;
     }
   
/* find scale factor for speed */
   sf = 1.;
   if ((subt == 2) || (subt == 4)) sf=4.;

/* subtype 1 or 2 (ground speed components) */
   if (subt < 3)
     {
	/* speed components */
	m = (*(b+1)&0x3)<<8;		/* east/west (bits 14..24)*/
	m |= *(b+2);
        n = (*(b+4)&0xe0)>>5;		/* north/south (bits 25..35)*/
	n |= (*(b+3)&0x7f)<<3;
					/* compute only if valid */
	if ((m!=0) && (n!=0))
	  {
	     p->veast = (double)(m-1)*sf;
	     if ( (*(b+1)&0x4) != 0 ) p->veast = -p->veast;

	     p->vnorth = (double)(n-1)*sf;
	     if ( (*(b+3)&0x80) != 0 ) p->vnorth = -p->vnorth;

	     /* computed heading and airspeed */	
	     p->cogc = -atan2(p->vnorth,p->veast)*180./M_PI + 90.;
	     if (p->cogc < 0.) p->cogc += 360.; /* angle to course */
	
	     p->sogc = sqrt(pow(p->veast,2)+pow(p->vnorth,2));
	  }
     }
/* subtype 3 and 4 (heading and airspeed) */
   else
     {
	/* heading */
	if ((*(b+1)&4) != 0)		/* heading avail. (bit 14)*/
	  { 
					/* magnetic heading (bits 15..24) */
	     m = *(b+2);
	     m |= (*(b+1)&0x3)<<8;
	     p->hdg = (double)m*360./1024.;
	  }
	
	/* airspeed */		 
	m = (*(b+3)&0x7f)<<3;		/* speed (bits 26..35) */
	m |= (*(b+4)&0xe0)>>5;
	if (m != 0)
	  {
	     x = (double)(m-1)*sf;
	     				/* 1=TAS, 0=IAS (bit 25) */
	     if ((*(b+3)&0x80) != 0)
	       p->tas = x;
	     else
	       p->ias = x;
	  }
     }
   return 0;
}

/*
 * ------------------------------------------------------------------------
 * routines for more complex data conversion
 * ------------------------------------------------------------------------
 * */

/*
 * ------------------------------------------------------------------------
 * fetch and convert surface position movement from DF17 FTC5..8
 * to speed in kn
 * returns speed as double
 *    "    speed value from empty/unknown structure if unknown
 *    "    175.5 for greater 175
 * ------------------------------------------------------------------------
 * */
double MOV2SPD(unsigned long int mov)
{
   
   struct Plane *p0;			/* 'empty/unknown' structure */
   
   if ((mov >= 125) || (mov == 0)) 
     {
	p0 = p_2_0();
	return p0->sogc;
     }
   
   if (mov == 124) return 175.50;
   if (mov >= 123) return 175.00;
   if (mov >= 109) return 105.00+5.00*(mov-109);
   if (mov >=  94) return  72.00+2.00*(mov- 94);
   if (mov >=  39) return  16.00+1.00*(mov- 39);
   if (mov >=  13) return   2.50+0.50*(mov- 13);
   if (mov >=   9) return   1.25+0.25*(mov-  9);
   if (mov >=   3) return   0.270833+0.145833*(mov-3);
   if (mov ==   2) return   (0.125);
   if (mov ==   1) return   0.0;
}

/*
 * ------------------------------------------------------------------------
 * ID to squawk or AC to gillham
 * input with 13 significant bits, from right (least sign. bit) to left:
 * 	D4-B4-D2-B2-D1-B1-ZERO-A4-C4-A2-C2-A1-C1
 * squawk representation in digits is ABCD
 * ------------------------------------------------------------------------
 * */

unsigned long int N2SQ(unsigned long int id)
{
					/* table of bit significance
					 * in octal */
   unsigned long int tab[] = 
     {
	  00004,			/* D4 */
	  00400,			/* B4 */
	  00002,			/* D2 */
	  00200,			/* B2 */
	  00001,			/* D1 */
	  00100,			/* B1 */
	  00000,			/* ZERO */
	  04000,			/* A4 */
	  00040,			/* C4 */
	  02000,			/* A2 */
	  00020,			/* C2 */
	  01000,			/* A1 */
	  00010				/* C1 */
     };
   
   unsigned long int sq = 0;
   unsigned long int mask = 1;
   int n;
   
   for (n=0; n<13; n++)
     {
	if ((id&mask) != 0) sq += tab[n]; /* if bit set add table contents */
	if (n == 6)			  /* must be ZERO */
	  {
	     if ((id&mask) != 0) return (07777); /* on error */
	  }
	mask <<= 1;			  /* select next bit */
     }
   return (sq);
}

/*
 * ------------------------------------------------------------------------
 * gillham code to altitude conversion
 * 	table value in octal:	A1-A2-A4-B1-B2-B4-C1-C2-C4-D1-D2-D4
 *      altitude from table index: alt = (i*100)-1200
 * 	return value from 'empty/unknown' structure on error
 * ------------------------------------------------------------------------
 * */

double GIL2FT(unsigned long int gil)
{
   unsigned int gillham[] =
     {
 	00040, 00060, 00020, 00030, 00010, 00410, 00430, 00420, 00460, 00440, 
	00640, 00660, 00620, 00630, 00610, 00210, 00230, 00220, 00260, 00240, 
	00340, 00360, 00320, 00330, 00310, 00710, 00730, 00720, 00760, 00740, 
	00540, 00560, 00520, 00530, 00510, 00110, 00130, 00120, 00160, 00140, 
	04140, 04160, 04120, 04130, 04110, 04510, 04530, 04520, 04560, 04540, 
	04740, 04760, 04720, 04730, 04710, 04310, 04330, 04320, 04360, 04340, 
	04240, 04260, 04220, 04230, 04210, 04610, 04630, 04620, 04660, 04640, 
	04440, 04460, 04420, 04430, 04410, 04010, 04030, 04020, 04060, 04040, 
	06040, 06060, 06020, 06030, 06010, 06410, 06430, 06420, 06460, 06440, 
	06640, 06660, 06620, 06630, 06610, 06210, 06230, 06220, 06260, 06240, 
	06340, 06360, 06320, 06330, 06310, 06710, 06730, 06720, 06760, 06740, 
	06540, 06560, 06520, 06530, 06510, 06110, 06130, 06120, 06160, 06140, 
	02140, 02160, 02120, 02130, 02110, 02510, 02530, 02520, 02560, 02540, 
	02740, 02760, 02720, 02730, 02710, 02310, 02330, 02320, 02360, 02340, 
	02240, 02260, 02220, 02230, 02210, 02610, 02630, 02620, 02660, 02640, 
	02440, 02460, 02420, 02430, 02410, 02010, 02030, 02020, 02060, 02040, 
	03040, 03060, 03020, 03030, 03010, 03410, 03430, 03420, 03460, 03440, 
	03640, 03660, 03620, 03630, 03610, 03210, 03230, 03220, 03260, 03240, 
	03340, 03360, 03320, 03330, 03310, 03710, 03730, 03720, 03760, 03740, 
	03540, 03560, 03520, 03530, 03510, 03110, 03130, 03120, 03160, 03140, 
	07140, 07160, 07120, 07130, 07110, 07510, 07530, 07520, 07560, 07540, 
	07740, 07760, 07720, 07730, 07710, 07310, 07330, 07320, 07360, 07340, 
	07240, 07260, 07220, 07230, 07210, 07610, 07630, 07620, 07660, 07640, 
	07440, 07460, 07420, 07430, 07410, 07010, 07030, 07020, 07060, 07040, 
	05040, 05060, 05020, 05030, 05010, 05410, 05430, 05420, 05460, 05440, 
	05640, 05660, 05620, 05630, 05610, 05210, 05230, 05220, 05260, 05240, 
	05340, 05360, 05320, 05330, 05310, 05710, 05730, 05720, 05760, 05740, 
	05540, 05560, 05520, 05530, 05510, 05110, 05130, 05120, 05160, 05140, 
	01140, 01160, 01120, 01130, 01110, 01510, 01530, 01520, 01560, 01540, 
	01740, 01760, 01720, 01730, 01710, 01310, 01330, 01320, 01360, 01340, 
	01240, 01260, 01220, 01230, 01210, 01610, 01630, 01620, 01660, 01640, 
	01440, 01460, 01420, 01430, 01410, 01010, 01030, 01020, 01060, 01040, 
	01044, 01064, 01024, 01034, 01014, 01414, 01434, 01424, 01464, 01444, 
	01644, 01664, 01624, 01634, 01614, 01214, 01234, 01224, 01264, 01244, 
	01344, 01364, 01324, 01334, 01314, 01714, 01734, 01724, 01764, 01744, 
	01544, 01564, 01524, 01534, 01514, 01114, 01134, 01124, 01164, 01144, 
	05144, 05164, 05124, 05134, 05114, 05514, 05534, 05524, 05564, 05544, 
	05744, 05764, 05724, 05734, 05714, 05314, 05334, 05324, 05364, 05344, 
	05244, 05264, 05224, 05234, 05214, 05614, 05634, 05624, 05664, 05644, 
	05444, 05464, 05424, 05434, 05414, 05014, 05034, 05024, 05064, 05044, 
	07044, 07064, 07024, 07034, 07014, 07414, 07434, 07424, 07464, 07444, 
	07644, 07664, 07624, 07634, 07614, 07214, 07234, 07224, 07264, 07244, 
	07344, 07364, 07324, 07334, 07314, 07714, 07734, 07724, 07764, 07744, 
	07544, 07564, 07524, 07534, 07514, 07114, 07134, 07124, 07164, 07144, 
	03144, 03164, 03124, 03134, 03114, 03514, 03534, 03524, 03564, 03544, 
	03744, 03764, 03724, 03734, 03714, 03314, 03334, 03324, 03364, 03344, 
	03244, 03264, 03224, 03234, 03214, 03614, 03634, 03624, 03664, 03644, 
	03444, 03464, 03424, 03434, 03414, 03014, 03034, 03024, 03064, 03044, 
	02044, 02064, 02024, 02034, 02014, 02414, 02434, 02424, 02464, 02444, 
	02644, 02664, 02624, 02634, 02614, 02214, 02234, 02224, 02264, 02244, 
	02344, 02364, 02324, 02334, 02314, 02714, 02734, 02724, 02764, 02744, 
	02544, 02564, 02524, 02534, 02514, 02114, 02134, 02124, 02164, 02144, 
	06144, 06164, 06124, 06134, 06114, 06514, 06534, 06524, 06564, 06544, 
	06744, 06764, 06724, 06734, 06714, 06314, 06334, 06324, 06364, 06344, 
	06244, 06264, 06224, 06234, 06214, 06614, 06634, 06624, 06664, 06644, 
	06444, 06464, 06424, 06434, 06414, 06014, 06034, 06024, 06064, 06044, 
	04044, 04064, 04024, 04034, 04014, 04414, 04434, 04424, 04464, 04444, 
	04644, 04664, 04624, 04634, 04614, 04214, 04234, 04224, 04264, 04244, 
	04344, 04364, 04324, 04334, 04314, 04714, 04734, 04724, 04764, 04744, 
	04544, 04564, 04524, 04534, 04514, 04114, 04134, 04124, 04164, 04144, 
	00144, 00164, 00124, 00134, 00114, 00514, 00534, 00524, 00564, 00544, 
	00744, 00764, 00724, 00734, 00714, 00314, 00334, 00324, 00364, 00344, 
	00244, 00264, 00224, 00234, 00214, 00614, 00634, 00624, 00664, 00644, 
	00444, 00464, 00424, 00434, 00414, 00014, 00034, 00024, 00064, 00044, 
	00046, 00066, 00026, 00036, 00016, 00416, 00436, 00426, 00466, 00446, 
	00646, 00666, 00626, 00636, 00616, 00216, 00236, 00226, 00266, 00246, 
	00346, 00366, 00326, 00336, 00316, 00716, 00736, 00726, 00766, 00746, 
	00546, 00566, 00526, 00536, 00516, 00116, 00136, 00126, 00166, 00146, 
	04146, 04166, 04126, 04136, 04116, 04516, 04536, 04526, 04566, 04546, 
	04746, 04766, 04726, 04736, 04716, 04316, 04336, 04326, 04366, 04346, 
	04246, 04266, 04226, 04236, 04216, 04616, 04636, 04626, 04666, 04646, 
	04446, 04466, 04426, 04436, 04416, 04016, 04036, 04026, 04066, 04046, 
	06046, 06066, 06026, 06036, 06016, 06416, 06436, 06426, 06466, 06446, 
	06646, 06666, 06626, 06636, 06616, 06216, 06236, 06226, 06266, 06246, 
	06346, 06366, 06326, 06336, 06316, 06716, 06736, 06726, 06766, 06746, 
	06546, 06566, 06526, 06536, 06516, 06116, 06136, 06126, 06166, 06146, 
	02146, 02166, 02126, 02136, 02116, 02516, 02536, 02526, 02566, 02546, 
	02746, 02766, 02726, 02736, 02716, 02316, 02336, 02326, 02366, 02346, 
	02246, 02266, 02226, 02236, 02216, 02616, 02636, 02626, 02666, 02646, 
	02446, 02466, 02426, 02436, 02416, 02016, 02036, 02026, 02066, 02046, 
	03046, 03066, 03026, 03036, 03016, 03416, 03436, 03426, 03466, 03446, 
	03646, 03666, 03626, 03636, 03616, 03216, 03236, 03226, 03266, 03246, 
	03346, 03366, 03326, 03336, 03316, 03716, 03736, 03726, 03766, 03746, 
	03546, 03566, 03526, 03536, 03516, 03116, 03136, 03126, 03166, 03146, 
	07146, 07166, 07126, 07136, 07116, 07516, 07536, 07526, 07566, 07546, 
	07746, 07766, 07726, 07736, 07716, 07316, 07336, 07326, 07366, 07346, 
	07246, 07266, 07226, 07236, 07216, 07616, 07636, 07626, 07666, 07646, 
	07446, 07466, 07426, 07436, 07416, 07016, 07036, 07026, 07066, 07046, 
	05046, 05066, 05026, 05036, 05016, 05416, 05436, 05426, 05466, 05446, 
	05646, 05666, 05626, 05636, 05616, 05216, 05236, 05226, 05266, 05246, 
	05346, 05366, 05326, 05336, 05316, 05716, 05736, 05726, 05766, 05746, 
	05546, 05566, 05526, 05536, 05516, 05116, 05136, 05126, 05166, 05146, 
	01146, 01166, 01126, 01136, 01116, 01516, 01536, 01526, 01566, 01546, 
	01746, 01766, 01726, 01736, 01716, 01316, 01336, 01326, 01366, 01346, 
	01246, 01266, 01226, 01236, 01216, 01616, 01636, 01626, 01666, 01646, 
	01446, 01466, 01426, 01436, 01416, 01016, 01036, 01026, 01066, 01046, 
	01042, 01062, 01022, 01032, 01012, 01412, 01432, 01422, 01462, 01442, 
	01642, 01662, 01622, 01632, 01612, 01212, 01232, 01222, 01262, 01242, 
	01342, 01362, 01322, 01332, 01312, 01712, 01732, 01722, 01762, 01742, 
	01542, 01562, 01522, 01532, 01512, 01112, 01132, 01122, 01162, 01142, 
	05142, 05162, 05122, 05132, 05112, 05512, 05532, 05522, 05562, 05542, 
	05742, 05762, 05722, 05732, 05712, 05312, 05332, 05322, 05362, 05342, 
	05242, 05262, 05222, 05232, 05212, 05612, 05632, 05622, 05662, 05642, 
	05442, 05462, 05422, 05432, 05412, 05012, 05032, 05022, 05062, 05042, 
	07042, 07062, 07022, 07032, 07012, 07412, 07432, 07422, 07462, 07442, 
	07642, 07662, 07622, 07632, 07612, 07212, 07232, 07222, 07262, 07242, 
	07342, 07362, 07322, 07332, 07312, 07712, 07732, 07722, 07762, 07742, 
	07542, 07562, 07522, 07532, 07512, 07112, 07132, 07122, 07162, 07142, 
	03142, 03162, 03122, 03132, 03112, 03512, 03532, 03522, 03562, 03542, 
	03742, 03762, 03722, 03732, 03712, 03312, 03332, 03322, 03362, 03342, 
	03242, 03262, 03222, 03232, 03212, 03612, 03632, 03622, 03662, 03642, 
	03442, 03462, 03422, 03432, 03412, 03012, 03032, 03022, 03062, 03042, 
	02042, 02062, 02022, 02032, 02012, 02412, 02432, 02422, 02462, 02442, 
	02642, 02662, 02622, 02632, 02612, 02212, 02232, 02222, 02262, 02242, 
	02342, 02362, 02322, 02332, 02312, 02712, 02732, 02722, 02762, 02742, 
	02542, 02562, 02522, 02532, 02512, 02112, 02132, 02122, 02162, 02142, 
	06142, 06162, 06122, 06132, 06112, 06512, 06532, 06522, 06562, 06542, 
	06742, 06762, 06722, 06732, 06712, 06312, 06332, 06322, 06362, 06342, 
	06242, 06262, 06222, 06232, 06212, 06612, 06632, 06622, 06662, 06642, 
	06442, 06462, 06422, 06432, 06412, 06012, 06032, 06022, 06062, 06042, 
	04042, 04062, 04022, 04032, 04012, 04412, 04432, 04422, 04462, 04442, 
	04642, 04662, 04622, 04632, 04612, 04212, 04232, 04222, 04262, 04242, 
	04342, 04362, 04322, 04332, 04312, 04712, 04732, 04722, 04762, 04742, 
	04542, 04562, 04522, 04532, 04512, 04112, 04132, 04122, 04162, 04142, 
	00142, 00162, 00122, 00132, 00112, 00512, 00532, 00522, 00562, 00542, 
	00742, 00762, 00722, 00732, 00712, 00312, 00332, 00322, 00362, 00342, 
	00242, 00262, 00222, 00232, 00212, 00612, 00632, 00622, 00662, 00642, 
	00442, 00462, 00422, 00432, 00412, 00012, 00032, 00022, 00062, 00042, 
        0	  
     };
   

   struct Plane *p0;			/* 'empty/unknown' structure */
   unsigned long int g;
   int i = 0;

   while ((g=gillham[i]) != 0)
     {
	if (g == gil) break;
	i++;
     }

   if (g == 0) 
     {
	p0 = p_2_0();
	return p0->alt;
     }
   
   return ((double)(i*100 - 1200));
}

/*
 * ------------------------------------------------------------------------
 * altitude in ft from AC
 * 	return value from 'empty/unknown' structure on error
 * return *status = -1 on invalid or no altitude (ac=0)
 *                   0 on gillham 100 ft decoding
 * 		     1 on binary 25 ft decoding
 * 		    -2 on metric decoding (reserved, no scaling known)
 * ------------------------------------------------------------------------
 * */
double AC2FT(unsigned long int ac, int *status)
{
   struct Plane *p0;			/* 'unknown/empty' structure */
   
   unsigned long int m_mask = 0x40;	/* M bit mask */
   unsigned long int q_mask = 0x10;	/* Q bit mask */
   
   unsigned long int n;

   int m_flag = 0;
   int q_flag = 0;
   
   if ((ac&0x1fff) == 0)
     {
	*status = -1;
	p0 = p_2_0();
	return p0->alt;
     }
      
   m_flag = ac&m_mask;
   q_flag = ac&q_mask;
   
   if (m_flag == 0)
       {
	  if (q_flag == 0)		/* gillham code */
	    {
	       *status = 0;
		n = N2SQ(ac); 
		return (GIL2FT(n)); 
	    }
	  				/* binary 25 ft with M and Q bit */
	  n = ac&0xf;			/* ls 4 bits */
	  n |= (ac&0x20)>>1;		/* the bit between M and Q */
	  n |= (ac&0x1f80)>>2;		/* the ms 6 bits */
	  
	  *status = 1;
	  return ((double)(25*n)-1000.);
       }

       					/* M = 1 -- metric, no Q */
   
       n = ac&0x3f;			/* ls 6 bits */
       n |= (ac&0x1f80)>>1;		/* ms 6 bits, M removed */
       
       *status = -2;
       return ((double)n/0.3048);	/* meter to ft (???) */
}

/*
 * ------------------------------------------------------------------------
 * routines for CPR decoding
 * 
 * latitude will be reported in the range of  -90 .. 0 ..  +90 degree
 * longitude                "                -180 .. 0 .. +180 degree
 *   with negativ values for south and west
 * 	
 * source docu:
 * 	'Working paper 1090-WP30-18 as DRAFT Version 4.2 of DO-282B'
 * 	Appendix A, PP. 55
 *	'CPR_1090_WP-9-14', '1090-WP30-12' and others
 * 
 * ------------------------------------------------------------------------
 * */
/*
 * ------------------------------------------------------------------------
 * find NL(lat)
 * return 0 on error (|lat|>=90.)
 * 
 * uses lookup table (filled on first run)
 * 
 * ------------------------------------------------------------------------
 * */
int Lat2NL(double lat)
{
   static double lat_s[60];
   static int lat_sf = 0;

   int nz = 15;
   int nl;
   
   double x;
   double alat;
					/* make lookup table - once only */   
   if (!lat_sf)
     {
	for (nl=2; nl<4*nz; nl++)
	  {
	     x = sqrt((1.-cos(M_PI/(2.*nz)))/(1.-cos((2.*M_PI)/nl)));
	     lat_s[nl] = (180./M_PI)*acos(x);
	  }
	lat_s[2] = 87.;
	lat_sf = 1;
     }

   alat = fabs(lat);
   if (alat>=90.) return 0;		/* on error */
   if (alat>=lat_s[2]) return 1;	/* >=87. */

   nl = 59;
   while (alat>=lat_s[nl]) nl--;	/* use lookup table */
   return nl;
}

/*
 * ------------------------------------------------------------------------
 * the special MOD function
 * ------------------------------------------------------------------------
 */
double MOD(double x, double y)
{
   return x-y*floor(x/y);
}

/*
 * ------------------------------------------------------------------------
 * find 'Globally Unambiguous Position' from odd/even data
 * in structure type CPR pointed to by '*xyt'
 * 
 * flag = 0:	airborne position
 * flag = 1:	surface position
 * 
 * solve surface position ambiguity with external offset data
 * 
 * return 0 on success
 *        -1 time difference odd/even too long
 *        -2 NL(rlat) == 0 error
 *	  -3 NL(rlat0/1) error
 * ------------------------------------------------------------------------
 */
int CPR2LL(struct CPR *xyt, int flag)
{

   static double two17 = 131072.;	/* pow(2,17) */
   
   int i;
   int j;
   int nl0, nl1, nl;
   int n0, n1;
   int m;
   
   double dlon0, dlon1;
   double dlat0, dlat1;
   double gmax;
   
   gmax = 360.;				/* assume airborne position */
   if (flag) gmax = 90.;
   
   dlat0 = gmax/60.;
   dlat1 = gmax/59.;
					/* max. time difference check */
   if (fabs(xyt->t[0] - xyt->t[1]) > 10) return -1;
//   if (fabs(xyt->t[0] - xyt->t[1]) > 20) return -1;

   j = floor(((59*xyt->yz[0] - 60*xyt->yz[1])/two17)+0.5);

					/* latitude with correction for
					 * southern hemisphere */
   xyt->rlat[0] = dlat0*(MOD(j,60)+(xyt->yz[0]/two17));
   if (xyt->rlat[0] > 90.) xyt->rlat[0] -=360.;
   xyt->rlat[1] = dlat1*(MOD(j,59)+(xyt->yz[1]/two17));
   if (xyt->rlat[1] > 90.) xyt->rlat[1] -=360.;

   nl0 = Lat2NL(xyt->rlat[0]); 
   nl1 = Lat2NL(xyt->rlat[1]); 

   if (nl0 != nl1) return -3;		/* nl0 must be == nl1 ...*/
   nl = nl0;
   if (nl == 0) return -2;		/* ... and none must be 0 */
   
   n0 = nl;
   n1 = nl-1;
   if (n1<1) n1 = 1;
   
   dlon0 = gmax/(double)n0;
   dlon1 = gmax/(double)n1;
   
   m = floor( ((xyt->xz[0]*(nl-1) - xyt->xz[1]*nl)/two17)+0.5 );
   					/* longitude with correction for
					 * west - east */
   xyt->rlon[0] = dlon0*(MOD(m,n0)+(xyt->xz[0]/two17));
   if (xyt->rlon[0] > 180.) xyt->rlon[0] -= 360.;
   xyt->rlon[1] = dlon1*(MOD(m,n1)+(xyt->xz[1]/two17));
   if (xyt->rlon[1] > 180.) xyt->rlon[1] -= 360.;

   
/* set last/reference position */
   
   i = xyt->cprf;
   
   xyt->t_s = xyt->t[i];
   xyt->lat_s = xyt->rlat[i];
   xyt->lon_s = xyt->rlon[i];

   if (flag)				/* surface position */
     {
	xyt->lat_s += SP_BASELAT;
	xyt->lon_s += SP_BASELON;
     }

   return 0;
}

/*
 * ------------------------------------------------------------------------
 * find 'Globally Unambiguous Position' from data
 *   in structure type CPR pointed to by '*xyt'
 * uses 'lat_s' and 'lon_s' from structure as reference point
 * 
 * flag = 0:	airborne position
 * flag = 1:	surface position
 * 
 * solve surface position ambiguity by external offset data
 * 
 * return 0 success
 *        +1 (warning) lat or lon diff between ref and new pos > 1/4 zone
 *        -1 ref. point lat/lon out of range
 *        -2 NL(rlat) == 0 error
 *        -3 lat or lon  difference between ref. and new position > 1/2 zone
 * ------------------------------------------------------------------------
 */
int RCPR2LL(struct CPR *xyt, int flag)
{
   static double two17 = 131072.;	/* pow(2,17) */
   
   int i;
   int nl;
   int succ;
   
   double dlat, dlon;
   double j, m;
   double gmax;
   double lat_s, lon_s;
   double y, x;
   double diff;
   
   succ = 0;

   lat_s = xyt->lat_s;
   lon_s = xyt->lon_s;
   if (lon_s<0) lon_s +=360;		/* lon in range 0..360 */
   					/* range check */
   if ((fabs(lat_s)>=90.) || (lon_s<0.) || (lon_s>=360.)) return -1;
   
   gmax = 360.;				/* assume airborne position */
   if (flag) gmax = 90.;
   
   i = xyt->cprf;			/* the last CPR data index */
   					/* compute latitude ... */
   dlat = gmax/(60.-(double)i);

   lat_s = xyt->lat_s;
   y = xyt->yz[i]/two17;

   j = floor(lat_s/dlat) + floor(0.5 + MOD(lat_s,dlat)/dlat - y);
   xyt->rlat[i] = dlat*(j+y);
					/* error/warning on pos difference */
   diff = fabs(xyt->rlat[i]-lat_s);
   if (diff>dlat/2.) return -3;
   if (diff>dlat/4.) succ = 1;
     
					/* ... and longitude */   
   nl = Lat2NL(xyt->rlat[i]);
   if (nl<1) return -2;
   nl -= i;
   
   dlon = gmax;
   if (nl>0) dlon = gmax/(double)nl;
   
   x = xyt->xz[i]/two17;
   m = floor(lon_s/dlon) + floor(0.5 + MOD(lon_s,dlon)/dlon - x);
   xyt->rlon[i] = dlon*(m+x);
   
   diff = fabs(xyt->rlon[i]-lon_s);
   if (diff>dlon/2.) return -3;
   if (diff>dlon/4.) succ =1;
   
   if (xyt->rlon[i] > 180.) xyt->rlon[i] -=360.;
   
					/* set new (reference) position */
   xyt->t_s = xyt->t[i];
   xyt->lat_s = xyt->rlat[i];
   xyt->lon_s = xyt->rlon[i];

   return succ;
}


/* ------------------------------------------------------------------------
 * some routined for convenience
 * (combination of routines above)
 * ------------------------------------------------------------------------ */

/*
 * AA from AP in bits 33-56, in different DF
 * 
 * */
unsigned long int AAfromAP56(unsigned char *bufp)
{   
   unsigned long int ac, r;
	
   ac = RemPar(bufp,&r,56);
   return ((ac^r)&0xffffff);
}
   
/*
 * AA from AP in bits 89-112, in different DF
 * 
 * */
unsigned long int AAfromAP112(unsigned char *bufp)
{   
   unsigned long int ac, r;
	
   ac = RemPar(bufp,&r,112);
   return ((ac^r)&0xffffff);
}
   
/*
 * ------------------------------------------------------------------------
 * fill CPR structure pointed to by '*c' with data from ME buffer
 * 	pointed to be '*d'
 * ------------------------------------------------------------------------
 * */
int CPRfromME(unsigned char *d, struct CPR *c)
{
   int i;
   
   i = FetchCPRF(d);
   c->cprf = i;
   c->t[i] = time(NULL);
   c->yz[i] = FetchELat(d);
   c->xz[i] = FetchELon(d);
}

