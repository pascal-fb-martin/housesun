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
 * housesun_sunsetsunrise.c -- Sun set and rise times from sunset-sunrise.org
 *
 * void housesun_sunsetsunrise_location (const char *timezone,
 *                                       float latitude, float longitude);
 *
 *    Set the current location for which future almanac data is needed.
 *    No almanac data will be available until the location is known.
 *
 * typedef void SunResponseListener (const char *day, int rise, int set);
 * void housesun_sunsetsunrise_register (SunResponseListener *listener);
 *
 *    Register a listener for sunset-sunrise.org responses. The rise and set
 *    values are the time of day in seconds.
 *
 * const char *housesun_sunsetsunrise_query (const char *day);
 *
 *    Launch a query for the specified day. The response is asynchronous.
 *    Return null on success, an error message on failure.
 *
 * const char *housesun_sunsetsunrise_origin (void);
 *
 *    Return a static string describing the origin of the data.
 */

// #include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <strings.h>
#include <time.h>

#include "echttp.h"
#include "echttp_libc.h"
#include "echttp_json.h"

#include "houselog.h"
#include "housesun_sunsetsunrise.h"

#define DEBUG if (echttp_isdebug()) printf

static char *SunTimezone = 0;
static float SunLatitude = 0.0;
static float SunLongitude = 0.0;

static SunResponseListener *SunListener = 0;

static const char *SunSetSunRiseUrl = "https://api.sunrise-sunset.org/json";
static const char *SunSetSunRiseWeb = "https://sunrise-sunset.org";

static const char SunSetPath[] = ".results.sunset";
static const char SunRisePath[] = ".results.sunrise";

void housesun_sunsetsunrise_location (const char *timezone,
                                      float latitude, float longitude) {

    if (SunTimezone) {
        if (!strsame (timezone, SunTimezone)) {
            free (SunTimezone);
            SunTimezone = 0;
        }
    }
    if (!SunTimezone) SunTimezone = strdup (timezone);

    SunLatitude = latitude;
    SunLongitude= longitude;
}

void housesun_sunsetsunrise_register (SunResponseListener *listener) {
    SunListener = listener;
}

static int housesun_response_time (const char *ascii) {

    int hour = atoi (ascii);
    const char *sep = strchr (ascii, ':');
    int minute = sep?atoi(sep+1):0;
    return (hour * 3600) + (minute * 60);
}

static void housesun_response
                (void *origin, int status, char *data, int length) {

    ParserToken tokens[100];
    int  count = 100;

    status = echttp_redirected("GET");
    if (!status) {
        echttp_submit (0, 0, housesun_response, (void *)0);
        return;
    }

    if (status != 200) {
        houselog_trace (HOUSE_FAILURE, "HTTP",
                        "ERROR %d on %s", status, SunSetSunRiseUrl);
        return;
    }

    if (!data) {
       houselog_trace (HOUSE_FAILURE, "HTTP",
                       "NO DATA from %s", SunSetSunRiseUrl);
       DEBUG ("No data from %s\n", SunSetSunRiseUrl);
       return;
    }
    DEBUG ("sunrise-sunset.org response: %s\n", data);
    const char *requested = origin;

    const char *error = echttp_json_parse (data, tokens, &count);
    if (error) {
        houselog_trace (HOUSE_FAILURE, "JSON", "SYNTAX ERROR %s", error);
        return;
    }
    if (count <= 0) {
        houselog_trace (HOUSE_FAILURE, "JSON", "NO DATA");
        return;
    }

    int index = echttp_json_search (tokens, SunSetPath);
    if (index <= 0) {
        houselog_trace (HOUSE_FAILURE, "JSON", "NO SUNSET TIME FOUND");
        return;
    }
    const char *sunsetascii = tokens[index].value.string;

    index = echttp_json_search (tokens, SunRisePath);
    if (index <= 0) {
        houselog_trace (HOUSE_FAILURE, "JSON", "NO SUNRISE TIME FOUND");
        return;
    }
    const char *sunriseascii = tokens[index].value.string;

    SunListener (requested, housesun_response_time (sunriseascii),
                            (12 * 3600) + housesun_response_time (sunsetascii));
}

const char *housesun_sunsetsunrise_query (const char *day) {

    if (!SunTimezone) return "unknown location";
    if (!SunListener) return "no listener";
    houselog_event_local ("WEB", SunSetSunRiseWeb, "QUERY", "FOR %s", day);

    char url[1024];

    snprintf (url, sizeof(url), "%s?lat=%1.7f&lng=%1.7f&date=%s&tzid=%s",
              SunSetSunRiseUrl, SunLatitude, SunLongitude, day, SunTimezone);
    DEBUG ("Launching query: %s\n", url);

    const char *error = echttp_client ("GET", url);
    if (error) {
        houselog_trace (HOUSE_FAILURE, "HTTP", "ERROR %s", error);
        return error;
    }
    echttp_submit (0, 0, housesun_response, (void *)day);
    return 0;
}

const char *housesun_sunsetsunrise_origin (void) {
    return SunSetSunRiseWeb;
}

