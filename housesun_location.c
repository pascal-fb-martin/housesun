/* HouseSun - A service providing Almanac data from sunrise-sunset.org
 *
 * Copyright 2025, Pascal Martin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 *
 * housesun_location.c - Retrieve the location of the house.
 *
 * SYNOPSYS:
 *
 * This module handles detection of, and communication with, clock
 * services:
 * - Run periodic discoveries to find which servers provide clock data.
 * - Query these servers and capture GPS position.
 * - Retrieve the local timezone.
 *
 * This module caches the current almanac data, so that the process for
 * fetching the data is fully asynchronous.
 *
 * int housesun_location_ready (void);
 *
 *    Return 1 if location data is available.
 *    The purpose is to delay processing that requires the location data.
 *
 * double housesun_location_lat (void);
 * double housesun_location_long (void);
 *
 *    Return the house latitude and longitude coordinates.
 *
 * const char *housesun_location_timezone (void);
 *
 *    Return the house local timezone.
 *
 * void housesun_location_background (time_t now);
 *
 *    The periodic function that detects the clock services.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <echttp.h>
#include <echttp_json.h>

#include "houselog.h"
#include "housediscover.h"

#include "housesun_location.h"

#define DEBUG if (echttp_isdebug()) printf

static int    HouseGpsFix = 0;
static double HouseLatitude = 0.0;
static double HouseLongitude = 0.0;

int housesun_location_ready (void) {
    return HouseGpsFix;
}

double housesun_location_lat (void) {
    return HouseLatitude;
}

double housesun_location_long (void) {
    return HouseLongitude;
}

static ParserToken *housesun_location_prepare (int count) {

    static ParserToken *EventTokens = 0;
    static int EventTokensAllocated = 0;

    if (count > EventTokensAllocated) {
        int need = EventTokensAllocated = count + 128;
        EventTokens = realloc (EventTokens, need*sizeof(ParserToken));
    }
    return EventTokens;
}

static void housesun_location_update (const char *provider,
                                      char *data, int length) {

   int count = echttp_json_estimate(data);
   ParserToken *tokens = housesun_location_prepare (count);

   const char *error = echttp_json_parse (data, tokens, &count);
   if (error) {
       houselog_trace
           (HOUSE_FAILURE, provider, "JSON syntax error, %s", error);
       return;
   }
   if (count <= 0) {
       houselog_trace (HOUSE_FAILURE, provider, "no data");
       return;
   }

   int index = echttp_json_search (tokens, ".clock.gps.fix");
   if (index <= 0) {
       houselog_trace (HOUSE_FAILURE, provider, "no GPS fix indicator");
       return;
   }
   if (!tokens[index].value.bool) return;

   index = echttp_json_search (tokens, ".clock.gps.latitude");
   if (index <= 0) {
       houselog_trace (HOUSE_FAILURE, provider, "no latitude data");
       return;
   }
   double latitude = tokens[index].value.real;

   index = echttp_json_search (tokens, ".clock.gps.longitude");
   if (index <= 0) {
       houselog_trace (HOUSE_FAILURE, provider, "no longitude data");
       return;
   }
   HouseLongitude = tokens[index].value.real;
   HouseLatitude = latitude;
   HouseGpsFix = 1;
   DEBUG ("Obtained house location: Lat %f, Long %f\n", HouseLatitude, HouseLongitude);
}

static void housesun_location_discovered
               (void *origin, int status, char *data, int length) {

   const char *provider = (const char *) origin;

   status = echttp_redirected("GET");
   if (!status) {
       echttp_submit (0, 0, housesun_location_discovered, origin);
       return;
   }

   if (status != 200) {
       houselog_trace (HOUSE_FAILURE, provider, "HTTP error %d", status);
       return;
   }

   housesun_location_update (provider, data, length);
}

static void housesun_location_scan
                (const char *service, void *context, const char *provider) {

    char url[256];

    snprintf (url, sizeof(url), "%s/status", provider);

    DEBUG ("Attempting query at %s\n", url);
    const char *error = echttp_client ("GET", url);
    if (error) {
        houselog_trace (HOUSE_FAILURE, provider, "%s", error);
        return;
    }
    echttp_submit (0, 0, housesun_location_discovered, (void *)provider);
}

const char *housesun_location_timezone (void) {

    static char HouseTimeZone[256] = "";

    if (HouseTimeZone[0] == 0) {
        FILE *f = fopen ("/etc/timezone", "r");
        if (!f) exit(1);
        fgets (HouseTimeZone, sizeof(HouseTimeZone), f);
        fclose (f);
        char *eol = strchr (HouseTimeZone, '\n');
        if (eol) *eol = 0;
        DEBUG ("Obtained house timezone: %s\n", HouseTimeZone);
    }
    return HouseTimeZone;
}

void housesun_location_background (time_t now) {

    static time_t latestdiscovery = 0;

    if (HouseGpsFix) return; // Houses do not move (for now..)

    // If any new service was detected, force a scan now.
    //
    if ((latestdiscovery > 0) &&
        housediscover_changed ("almanac", latestdiscovery)) {
        latestdiscovery = 0;
    }

    if (now <= latestdiscovery + 10) return;
    latestdiscovery = now;

    DEBUG ("Proceeding with clock service discovery\n");
    housediscovered ("clock", 0, housesun_location_scan);
}

