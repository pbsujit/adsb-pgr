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

struct Misc *p_mi;      /* pointer to 'Misc' structure in
           * shared memory */

int lookup_timeout = 60;

struct aircraft {

  Plane* p; // from shared memory

  unsigned int hex; // 24-bit aircraft address assigned by ICAO
  string icao; // hex -> icao; as string
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
  double delta_alt; // alt - last_alt
  double fcu_alt; // intended altitude
  double lat, last_lat, lon, last_lon;
  double delta_lat, delta_lon;
  double groundspeed;

  state () : stat ("unknown") {
    alt = last_alt = fcu_alt = 0;
    lat = last_lat = lon = last_lon = 0;
    delta_lat = delta_lon = 0;
    delta_alt = 0;
    groundspeed = 0;
  }

};

struct info { // from web lookup

  string reg; // registration ie tail number
  string airline;
  string type;
  string from;
  string to;

  int lookup;
  info () : reg("------"), airline("------"), type("----"), from("------"), to("------"), lookup (lookup_timeout) {}

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
  static const int UNKNOWN = -9999;
  static const int SIZE = 1024;
  static char from [SIZE], to [SIZE], airline [SIZE], altitude [SIZE], intent[SIZE], status[SIZE];
  string latitude, longitude;

  while ((p = p_icao(1)) != NULL) {
    if (p->lat != UNKNOWN && p->lon != UNKNOWN && strlen (p->acident) > 3) {
      planes.push_back (aircraft (p));
      aircraft& plane = planes [planes.size() - 1];
      state& ps = plane_state [plane.flight_number];

      if (ps.stat == "unknown") {
        ps.stat = "level at ";
        ps.last_alt = ps.alt = ps.fcu_alt = p->alt;
        ps.last_lat = ps.lat = p->lat;
        ps.last_lon = ps.lon = p->lon;
      } else {
        ps.alt = p->alt;
        ps.lat = p->lat;
        ps.lon = p->lon;
        ps.delta_lat = ps.lat - ps.last_lat;
        ps.delta_lon = ps.lon - ps.last_lon;
        ps.delta_alt = ps.alt - ps.last_alt;
        if (ps.delta_alt > 0) ps.stat = "ascending to"; else if (ps.delta_alt < 0) ps.stat = "descending to"; else ps.stat = "level at";
        ps.last_alt = ps.alt;
        ps.last_lat = ps.lat;
        ps.last_lon = ps.lon;
      }

      // round fcu_alt to nearest 10
      ps.fcu_alt = p->bds.fcu_alt_40 - (int) p->bds.fcu_alt_40 % 10;
      if (ps.fcu_alt < 0) ps.fcu_alt = 0;

      ps.groundspeed = p->bds.g_speed_50;

    }
  }

  // sort planes closest to observer
  // sort (planes.begin(), planes.end(), closest ());

  // print
  stringstream ss;
  for (int i = 0, j = planes.size (), k = 0; i < j; ++i) {
    aircraft& a = planes [i];
    info& d = plane_info [a.icao];
    state& s = plane_state [a.flight_number];
    string flightid (a.p->acident);
    if (flightid.length() < 3) flightid = "------";
    sprintf (altitude, "%05d", (int) s.alt);
    sprintf (intent, "%05d", (int) s.fcu_alt);
    sprintf (status, "%s", s.stat.c_str());
    if (d.reg.length () < 3) {
      d.reg = "------";
      d.type = "----";
      d.lookup = lookup_timeout;
    }

    string get_from_to ("0"), bg;
    if (++d.lookup > lookup_timeout) {
      get_from_to = "1";
      bg = " &";
      d.lookup = 0;
    }

    if (((int)s.delta_alt != 0) || (get_from_to == "1") || (s.delta_lat != 0) || (s.delta_lon != 0)) {

      ss.clear ();
      ss << a.lat << ' ' << a.lon;
      ss >> latitude >> longitude;

      ss.clear ();
      ss << a.p->bds.hdg_60;
      string heading; ss >> heading;

      ss.clear ();
      ss << s.groundspeed * 1.15077945;
      string gspeed; ss >> gspeed;

      cout << "ground speed = " << gspeed << endl;

      string cmd("./lookup " + flightid + ' ' + a.icao + ' ' + d.reg + ' ' + d.type + ' ' + get_from_to + ' ' + latitude + ' ' + longitude + ' ' + altitude + ' ' + intent + ' ' + heading + ' ' + status + ' ' + gspeed + bg);
      system (cmd.c_str());

      if (get_from_to == "1") {
        ifstream fout ("out");
        fout.getline (from, SIZE, '\n');
        fout.getline (to, SIZE, '\n');
        fout.getline (airline, SIZE, '\n');
        d.from = from;
        d.to = to;
        d.airline = airline;
        system ("rm -f out");
      }

    }

    if (show == "both" || show == s.stat || show == "level") {
      if (s.stat == "ascending to") printf ("\e[1;35m"); else if (s.stat == "descending to") printf ("\e[1;31m");
      printf ("%03d %8s %6s %6s %4s %05.0f %13s %05.0f %s -> %s (%s) \n", ++k, a.flight_number.c_str(), a.icao.c_str(), d.reg.c_str(), d.type.c_str(), s.alt, s.stat.c_str(), s.fcu_alt, d.from.c_str(), d.to.c_str(), d.airline.c_str());

    }
    printf ("\e[0;30m");
  }

}

main(int argc, char** argv)
{
  if (argc < 2) {
    show = "both";
  } else {
    show = argv[1];
  }

  if (argc == 3) {
    stringstream ss3; ss3 << argv[2];
    ss3 >> lookup_timeout;
  } else lookup_timeout = 60;

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

  while (1) /* loop forever */ {
    printf("%s",top);
    printf ("%3s %8s %6s %6s %4s %5s %19s %s\n", "No:", "Flight  ", "ICAO  ", "Regn  ", "Type", "  Alt", "     State", "From -> To");
    p_ref();
    sleep(1);
  }

}

