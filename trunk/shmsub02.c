/* Subroutines for support of programs using shared memory
 *   of 'mds02'
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "mds02.h"

void *Shm_ptr;				/* pointer to shared memory */

//struct Misc misc;

/* ------------------------------------------------------------------------
 * 
 * get shm_id from mds02 log file
 * return id on success
 *        0 on failure
 * ------------------------------------------------------------------------ */

int get_shm_id(void)
{
					/* from mds02.h:
					 * 	LOG_F_NAME
					 * */
#define l_size 80   
   char line[l_size];
   char *cp;
   FILE *log_f;
   
   if ((log_f = fopen(LOG_F_NAME,"r")) != NULL)
     {
	while(fgets(line,l_size,log_f) != NULL)
	  {
	     if ((cp = strstr(line,"SHM_ID=")) != NULL)
	       {
		  fclose(log_f);
		  return (atoi(cp+7));
	       }
	  }
     }
   fclose(log_f);
   fprintf(stderr,"'get_shm_key' unable to find shared memory key\n");
   return (0);
}

/* ------------------------------------------------------------------------
 * prepare use of shared memory containing plane structures
 * 	for 'slave' programs of 'mds02' 
 * reports  0 in success and sets global pointer 'Shm_ptr'
 *          'errno' on error
 * ------------------------------------------------------------------------ */
int shm_att(int shm_id)
{
   extern void *Shm_ptr;

   if ( (Shm_ptr=shmat(shm_id,NULL,0)) == (void *)-1) return (errno);
   return (0);
}

/* ------------------------------------------------------------------------
 * report pointer in shared memory pointing to
 *   'Misc' structure 'misc' (behind all Plane structures)
 * ------------------------------------------------------------------------ */
struct Misc *p_2_misc(void)
{   
					/* from mds02.h:
					 * 	P_MAX
					 * 	struct Plane
					 * */
   extern void *Shm_ptr;

   return (struct Misc *)((long int)Shm_ptr + (P_MAX+1)*sizeof(struct Plane));
}

/* ------------------------------------------------------------------------
 * report pointer in shared memory pointing to
 *   empty/template plane structure (first plane structure with index 0)
 * ------------------------------------------------------------------------ */
struct Plane *p_2_0(void)
{   
					/* from mds02.h:
					 * 	P_MAX
					 * 	struct Plane
					 * */
   extern void *Shm_ptr;

   return (struct Plane *)(long int)Shm_ptr;
}

/* ------------------------------------------------------------------------
 * find and report pointer in shared memory pointing to
 *   plane structure of index i
 * return NULL on error
 * ------------------------------------------------------------------------ */
struct Plane *pptr(int i)
{
					/* from mds02.h:
					 * 	P_MAX
					 * 	struct Plane
					 * */
   extern void *Shm_ptr;

   long int n;
   
   if ((i<0) || (i>P_MAX)) return(NULL);
   n = i * sizeof(struct Plane);
   n += (long int)Shm_ptr;
   return (struct Plane *)n;
}

/* ------------------------------------------------------------------------
 * find pointer to plane structure with given icao address in shared memory
 * 	return NULL if not found
 * special cases:
 * if icao addrss is 1 
 *	- return pointer to next structure with valid address
 * 		returns NULL if at end of database
 * 		but loops araound automatically at next call with 
 *		  icao addr = 1
 * if icao address is 0 
 *	- return pointer to data structure with 
 *		'unknown'/'not available'/'empty' data
 *	- reset pointer for requesting next to begin of database 
 * 
 * returns pointer to the 'unknown ...' data structure
 *		if invalid address is given
 * ------------------------------------------------------------------------ */
struct Plane *p_icao(unsigned int addr)
{
					/* from mds02.h:
					 * 	P_MAX
					 * 	struct Plane
					 * 	struct Misc
					 * */
   extern void *Shm_ptr;
   
   static int is = 1;

   int i;

   struct Plane *p;
   struct Misc *p_misc;
   
   time_t t_oldest;
   
/* pointer to the empty/unused structure */

   if (addr == 0)
     {
	is = 1;
	return (struct Plane *)(long int)Shm_ptr;
     }


   p_misc = p_2_misc();		/* from now we need this pointer */
   
/* find pointer to unique address */

   if (addr > 1)
     {
	for (i=1; i <= p_misc->P_MAX_C; i++)
//	for (i=1; i <= P_MAX; i++)
	  {
	     p = pptr(i);
	     if (p->icao == addr) return (p);
	  }
	return (NULL);
     }

/* next  */

   if (addr == 1)			/* next valid */
     {
	while (is <= p_misc->P_MAX_C)		/* if not at end ... */
//	while (is <= P_MAX)		/* if not at end ... */
	  {
	     p = pptr(is++);
	     if (p->icao > 0x000001) return p;	/* if valid, return */
	  }
	is = 1;				/* ... next at begin */
	return NULL;
     }
   
   return (struct Plane *)(long int)Shm_ptr;	/* on error in icao address */
}

