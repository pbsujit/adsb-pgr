/*
  * This file is part of adsb-pgr.
  *
  * adsb-pgr is copyright (c) 2011 Meinolf Braeutigam
  *
  * adsb-gr is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  *
  * adsb-pgr is distributed in the hope that it will be useful, but WITHOUT
  * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
  * for more details.
  *
  * You should have received a copy of the GNU General Public License along
  * with adsb-pgr.  If not, see <http://www.gnu.org/licenses/>.
  *
  * Meinolf Braeutigam, Kannenbaeckerstr. 14
  * 53395 Rheinbach, GERMANY
  * meinolf.braeutigam@arcor.de
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <vector>
#include <algorithm>
#include <string>
#include <iostream>
#include <map>
#include <fstream>
#include <sstream>
#include <iomanip>
using namespace std;

#include "mds02.h"      /* description of structures
           * and more */

char t_string[5];     /* time string */

struct Misc *p_mi;      /* pointer to 'Misc' structure in
           * shared memory */

/* ------------------------------------------------------------------------
 * make time string MM:SS 'tstr' from unix time
 * ------------------------------------------------------------------------ */
void mktstr(time_t t, char *tstr)
{

   struct tm *syst;

   syst = gmtime(&t);     /* unix calendar time to
           *   broken down time */
   sprintf(tstr,"%.2d:%.2d",
    syst->tm_min, syst->tm_sec);
}


static const int UNKNOWN = -9999;

struct aircraft {

  Plane* p; // from shared memory

  unsigned int hex; // 24-bit aircraft address assigned by ICAO
  string icao; // hex -> icao string
  string flight_number;

  double lat, lon, height; // plane position
  double distance2; // will sort planes based on distance from observer location


  aircraft (Plane* pp) : p(pp) {
    hex = p->icao;
    flight_number = p->acident;
    lat = p->lat;
    lon = p->lon;
    height = p->alt;
    double dx = lat - O_POS_LAT, dy = lon - O_POS_LON, dz = height;
    distance2 = dx * dx + dy * dy + dz * dz;
    stringstream ss; ss << setw(6) << setfill ('0') << std::hex << hex; ss >> icao;
    transform (icao.begin(), icao.end(), icao.begin(), ::toupper);
  }

};

struct closest {
  bool operator() (const aircraft& a1, const aircraft& a2) {
    return (a1.distance2 < a2.distance2);
  }
};

struct state {

  string stat; // ascending, descending or level
  double alt; // current altitude in feet
  double last_alt; // last altitude
  double fcu_alt; // intended altitude

  // take MAX_DELTA height changes b4 deciding ascending or descending
  int ndelta;
  double delta_alt;
  static const int MAX_DELTAS = 3;

  state () : stat ("unknown") {
    alt = last_alt = fcu_alt = 0;
    ndelta = 0;
    delta_alt = 0;
  }

};

struct info { // from web lookup

  string reg; // registration number
  string callsign;
  string type;
  string from;
  string to;
  string milesdown;
  string milestogo;

  int tries; // num times to lookup web b4 giving up
  info () : tries(0), milesdown ("0"), milestogo ("0") {}

};

typedef string plane_hex;
typedef string flight_id;

map<plane_hex, info> plane_info;
map<flight_id, state> plane_state;

string show ("both");

int p_ref (void)
{
   struct Plane *p;     /* pointer to structures with data */
   struct Plane *p0;      /* pointer to empty structure */

   int n = 0;


/* fetching data from shared memory using routines from 'shmsub02' */

   p0 = p_icao(0);      /* p0 points to empty structure */

   vector<aircraft> planes;
   while ((p = p_icao(1)) != NULL) {
    if (p->lat != UNKNOWN && p->lon != UNKNOWN && strlen (p->acident) > 3) {
     planes.push_back (aircraft (p));
     aircraft& plane = planes [planes.size() - 1];
     state& ps = plane_state [plane.flight_number];
     if (ps.stat == "unknown") {
      ps.stat = "level";
      ps.last_alt = ps.alt = ps.fcu_alt = p->alt;
     } else {
      ps.alt = p->alt;
      double delta_alt = ps.alt - ps.last_alt;
      ps.delta_alt += delta_alt;
      if (++ps.ndelta >= state::MAX_DELTAS) {
        delta_alt = ps.delta_alt / state::MAX_DELTAS;
        ps.ndelta = ps.delta_alt = 0;
        if (delta_alt > 0) ps.stat = "ascending"; else if (delta_alt < 0) ps.stat = "descending";
      }
      ps.last_alt = ps.alt;
    }

    // round fcu_alt to nearest 10
    ps.fcu_alt = p->bds.fcu_alt_40 - (int) p->bds.fcu_alt_40 % 10;
    if (ps.fcu_alt < 0) ps.fcu_alt = 0;

   }

  }

  // sort planes closest to observer
  sort (planes.begin(), planes.end(), closest ());

  // print
  for (int i = 0, j = planes.size (), k = 0; i < j; ++i) {
    aircraft& a = planes [i];
    info& d = plane_info [a.icao];
    state& s = plane_state [a.flight_number];
    if (d.from == "" && d.to == "" && ++d.tries < 3) { // look web to find flight itinerary for flight number(see ./lookup)
      static char from [256], to [256], callsign [256], latlon [256], milesdown [256], milestogo [256];
      sprintf (latlon, " %f %f ", a.p->lat, a.p->lon);
      string cmd("./lookup " + d.reg + ' ' + string(a.p->acident) + latlon + a.icao);
      system (cmd.c_str());
      ifstream fout ("out");
      fout.getline (from, 256, '\n');
      fout.getline (to, 256, '\n');
      fout.getline (callsign, 256, '\n');
      fout.getline (milesdown, 256, '\n');
      fout.getline (milestogo, 256, '\n');
      d.from = from;
      d.to = to;
      d.callsign = callsign;
      d.milesdown = milesdown;
      d.milestogo = milestogo;
      system ("rm -f out");
    }

    if (show == "both" || show == s.stat || show == "level") {
      if (s.stat == "ascending") printf ("\e[1;35m"); else if (s.stat == "descending") printf ("\e[1;31m");
      printf ("%03d %8s %6s %6s %4s %05.0f %10s to %05.0f %s -> %s (%s) \n", ++k, a.flight_number.c_str(), a.icao.c_str(), d.reg.c_str(), d.type.c_str(), s.alt, s.stat.c_str(), s.fcu_alt, d.from.c_str(), d.to.c_str(), d.callsign.c_str());
    }
    printf ("\e[0;30m");
  }

}

/* ------------------------------------------------------------------------
 * ------------------------------------------------------------------------ */
main(int argc, char** argv)
{
  if (argc < 2) show = "both"; else show = argv[1];

  int n,k;

/* prepare shared memory access (with params from 'mds02.h') */

  if ((k = get_shm_id()) == 0) exit(1); /* get shared mem id*/
  if ((n = shm_att(k)) != 0)   /* connect to shared memory */ {
   fprintf(stderr,"\nshared memory connection error:\n\t%s\n",
   strerror(n));
   exit(1);
  }

  /* pointer to 'Misc' structure */

  p_mi = p_2_misc();

  // read plane icao, registration & type database
  ifstream dbf ("db.dat");
  while (!dbf.eof()) {
    string hex, reg, type;
    dbf >> hex >> reg >> type;
    info& d = plane_info [hex];
    d.reg = reg; d.type = type;
  }

  char top[] = {"\033[0;0H\033[J"}; /* vt100 'top of clear page' string  */

  /* loop until terminated manually by <CTRL>-C */

  while(1) /* loop forever */ {
    printf("%s",top);
    printf ("%3s %8s %6s %6s %4s %5s %19s %s\n", "No:", "Flight  ", "ICAO  ", "Regn  ", "Type", "  Alt", "     State", "From -> To");
    p_ref();
    sleep(1);
  }

}

