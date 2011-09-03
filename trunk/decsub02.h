/* header of the subroutines for decoding of mode-s data packets 
 * 
 * */
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
 * -------------------------------------------------------------------------
 *  --- alpha test version mbr20110725 ---
 * -------------------------------------------------------------------------
 * */

#ifndef MDSSUB_HDR
#define MDSSUB_HDR 1

/* function prototypes */

unsigned int LeftShift(unsigned char *b, int n);

unsigned long int RemPar(unsigned char *b, unsigned long int *r, int nb);
   
unsigned int Sep0608(unsigned char *bufp);

unsigned long int Sep2032(unsigned char *bufp);

unsigned long int SepAA(unsigned char *bufp);

unsigned long int SepAP56(unsigned char *bufp);

unsigned long int SepAP112(unsigned char *bufp);

unsigned long int SepPI(unsigned char *bufp);

unsigned int SepDR(unsigned char *bufp);

unsigned int SepUM(unsigned char *bufp);

unsigned int SepVS(unsigned char *bufp);

unsigned int SepCC(unsigned char *bufp);

unsigned int SepRI(unsigned char *bufp);

unsigned int FetchFTC(unsigned char *b, unsigned char *s);

unsigned int FetchSS(unsigned char *b);

unsigned int FetchNICSB(unsigned char *b);

unsigned int  FetchTBit(unsigned char *b);
unsigned int FetchCPRF(unsigned char *b);

unsigned long int FetchELat(unsigned char *b);

unsigned long int FetchELon(unsigned char *b);

unsigned long int FetchEAC(unsigned char *b);

unsigned long int FetchMOV(unsigned char *b);

unsigned long int FetchEMS(unsigned char *b);

double GTRKHDG(unsigned char *b);

void MB2Ident_2(unsigned char *b, char *s);

int MB2Ident(unsigned char *b, char *s);

int AirbVel(unsigned char *b, struct Plane *sp);

double MOV2SPD(unsigned long int mov);

unsigned long int N2SQ(unsigned long int id);

double GIL2FT(unsigned long int gil);

double AC2FT(unsigned long int ac, int *status);

int Lat2NL(double lat);

double MOD(double x, double y);

int CPR2LL(struct CPR *xyt, int flag);

int RCPR2LL(struct CPR *xyt, int flag);

unsigned long int AAfromAP56(unsigned char *bufp);

unsigned long int AAfromAP112(unsigned char *bufp);

int CPRfromME(unsigned char *d, struct CPR *c);

#endif

