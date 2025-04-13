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
 * DESIGN
 *
 * This service implements the almanac web API and feeds its data
 * from sunrise-sunset.org. However this web site require the location
 * for which the times should be calculated.
 *
 * Since this location can be provided as a latitude/longitude pair,
 * this program interrogates the clock services until it gets a GPS location.
 */

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "echttp.h"
#include "echttp_static.h"
#include "echttp_json.h"
#include "echttp_xml.h"
#include "houseportalclient.h"

#include "housediscover.h"
#include "houselog.h"
#include "housesun_location.h"

#define DEBUG if (echttp_isdebug()) printf

static time_t SunTimestamp = 0;
static time_t SunSet = 0;
static time_t SunRise = 0;

static const char *SunSetSunRiseUrl = "https://api.sunrise-sunset.org/json";

static const char SunSetPath[] = ".results.sunset";
static const char SunRisePath[] = ".results.sunrise";

static const char *housesun_nextnight (const char *method, const char *uri,
                                       const char *data, int length) {
    static char buffer[65537];
    static char pool[65537];
    static char host[256];

    ParserToken token[1024];

    if (SunRise < time(0)) {
        // This data is too old. Let's hope that the HouseAlmanac service
        // is running as a fallback.
        echttp_error (500, "EXPIRED ALMANAC DATA");
        return "";
    }
    if (host[0] == 0) gethostname (host, sizeof(host));

    ParserContext context = echttp_json_start (token, 1024, pool, sizeof(pool));

    int root = echttp_json_add_object (context, 0, 0);
    echttp_json_add_string (context, root, "host", host);
    echttp_json_add_string (context, root, "proxy", houseportal_server());
    echttp_json_add_integer (context, root, "timestamp", (long)time(0));
    int top = echttp_json_add_object (context, root, "almanac");
    echttp_json_add_integer (context, top, "priority", 10);
    echttp_json_add_integer (context, top, "sunset", SunSet);
    echttp_json_add_integer (context, top, "sunrise", SunRise);

    // Extra information that can be used as status.
    //
    echttp_json_add_integer (context, top, "updated", SunTimestamp);
    if (housesun_location_ready()) {
        echttp_json_add_real (context, top, "lat", housesun_location_lat());
        echttp_json_add_real (context, top, "long", housesun_location_long());
    }
    echttp_json_add_string (context, top, "timezone", housesun_location_timezone());

    const char *error = echttp_json_export (context, buffer, sizeof(buffer));
    if (error) {
        echttp_error (500, error);
        return "";
    }
    echttp_content_type_json ();
    return buffer;
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

    time_t now = time(0);

    if (strcmp (requested, "today")) {
        // This is tomorrow's times. Only use the sunrise time.
        time_t tomorrow = now + (24*60*60);
        struct tm datetime = *localtime (&tomorrow);
        datetime.tm_hour = atoi (sunriseascii);
        const char *sep = strchr (sunriseascii, ':');
        datetime.tm_min = sep?atoi(sep+1):0;
        SunRise = mktime (&datetime);
        DEBUG ("Sunrise time for %s: %lld\n", requested, (long long) SunRise);
    } else {
        // This is today's times. Only use the sunset time.
        struct tm datetime = *localtime (&now);
        datetime.tm_hour = atoi (sunsetascii);
        const char *sep = strchr (sunsetascii, ':');
        datetime.tm_min = sep?atoi (sep+1):0;
        SunSet = mktime (&datetime);
        DEBUG ("Sunset time for %s: %lld\n", requested, (long long) SunSet);
    }
    DEBUG ("Current time: %lld\n", (long long)now);

    if ((SunRise > now) &&
        (SunRise > SunSet) && (SunSet > SunRise - (24*60*60))) {
        SunTimestamp = now;
        DEBUG ("Almanac data is now available: sunrise = %lld, sunset = %lld\n",
               (long long)SunRise, (long long)SunSet);
    }
}

static void housesun_query_almanach (const char *date) {

    char url[1024];

    snprintf (url, sizeof(url), "%s?lat=%1.7f&lng=%1.7f&date=%s&tzid=%s",
              SunSetSunRiseUrl,
              housesun_location_lat(), housesun_location_long(),
              date, housesun_location_timezone());
    DEBUG ("Launching query: %s\n", url);

    const char *error = echttp_client ("GET", url);
    if (error) {
        houselog_trace (HOUSE_FAILURE, "HTTP", "ERROR %s", error);
        return;
    }
    echttp_submit (0, 0, housesun_response, (void *)date);
}

static void housesun_background (int fd, int mode) {

    static time_t LastCall = 0;
    static time_t LastQuery = 0;
    time_t now = time(0);

    if (now == LastCall) return;
    LastCall = now;

    if (echttp_dynamic_port()) {
        static time_t Renewed = 0;
        if (Renewed) {
            if (now > Renewed + 60) {
                houseportal_renew();
                Renewed = now;
            }
        } else if (now % 5 == 0) {
            static const char *path[] = {"almanac:/sun"};
            houseportal_register (echttp_port(4), path, 1);
            Renewed = now;
        }
    }

    housediscover (now);
    houselog_background (now);
    housesun_location_background (now);

    if (SunRise > now) return; // Existing data has not expired yet.

    if (now < LastQuery + 10) return; // Do not issue requests at high rate.
    LastQuery = now;

    if (!housesun_location_ready())
        return; // We need the GPS coordinates to qery the almanac data.

    housesun_query_almanach ("today");
    housesun_query_almanach ("tomorrow");
}

int main (int argc, const char **argv) {

    // These strange statements are to make sure that fds 0 to 2 are
    // reserved, since this application might output some errors.
    // 3 descriptors are wasted if 0, 1 and 2 are already open. No big deal.
    //
    open ("/dev/null", O_RDONLY);
    dup(open ("/dev/null", O_WRONLY));

    housesun_location_timezone ();

    echttp_default ("-http-service=dynamic");

    argc = echttp_open (argc, argv);
    if (echttp_dynamic_port())
        houseportal_initialize (argc, argv);

    housediscover_initialize (argc, argv);
    houselog_initialize ("sun", argc, argv);

    echttp_route_uri ("/sun/status", housesun_nextnight);
    echttp_route_uri ("/sun/nextnight", housesun_nextnight);
    echttp_static_route ("/", "/usr/local/share/house/public");
    echttp_background (&housesun_background);
    echttp_loop();
}

