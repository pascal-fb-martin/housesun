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
 * from sunrise-sunset.org, or from a local calculation. Both require
 * the location for which the times should be calculated.
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
#include "echttp_libc.h"
#include "echttp_static.h"
#include "echttp_json.h"
#include "echttp_xml.h"
#include "houseportalclient.h"

#include "housediscover.h"
#include "houselog.h"
#include "housesun_cache.h"
#include "housesun_location.h"
#include "housesun_sunsetsunrise.h"

#define DEBUG if (echttp_isdebug()) printf

static int housesun_head (ParserContext *context, time_t now) {

    static char host[256];
    static char pool[65537];
    static ParserToken token[1024];

    if (host[0] == 0) gethostname (host, sizeof(host));

    ParserContext local = echttp_json_start (token, 1024, pool, sizeof(pool));

    int root = echttp_json_add_object (local, 0, 0);
    echttp_json_add_string (local, root, "host", host);
    echttp_json_add_string (local, root, "proxy", houseportal_server());
    echttp_json_add_integer (local, root, "timestamp", now);

    // Extra information that can be used as status.
    //
    if (housesun_location_ready()) {
        int loc = echttp_json_add_object (local, root, "location");
        echttp_json_add_real (local, loc, "lat", housesun_location_lat());
        echttp_json_add_real (local, loc, "long", housesun_location_long());
        echttp_json_add_string (local, loc, "timezone", housesun_location_timezone());
    }
    *context = local;
    return root;
}

static int housesun_top (ParserContext context, int root) {

    const char *origin = housesun_cache_origin();
    time_t updated = housesun_cache_updated();

    int top = echttp_json_add_object (context, root, "almanac");
    echttp_json_add_integer (context, top, "priority", 10);
    echttp_json_add_integer (context, top, "updated", updated);
    echttp_json_add_string  (context, top, "origin", origin);
    return top;
}

static void housesun_subcontent (ParserContext context,
                                 int top, const char *id,
                                 time_t sunrise, time_t sunset) {

    int sub = echttp_json_add_object (context, top, id);
    echttp_json_add_integer (context, sub, "sunrise", sunrise);
    echttp_json_add_integer (context, sub, "sunset", sunset);
}

static const char *housesun_tail (ParserContext context) {

    static char buffer[65537];
    const char *error = echttp_json_export (context, buffer, sizeof(buffer));
    if (error) {
        echttp_error (500, error);
        return "";
    }
    echttp_content_type_json ();
    return buffer;
}

static const char *housesun_content (time_t now,
                                     time_t sunset, time_t sunrise) {

    ParserContext context;
    int root = housesun_head (&context, now);
    int top = housesun_top (context, root);
    echttp_json_add_integer (context, top, "sunset", sunset);
    echttp_json_add_integer (context, top, "sunrise", sunrise);
    return housesun_tail (context);
}

static time_t housesun_refresh (void) {

    time_t now = time(0);
    const char *error = housesun_cache_refresh (now);
    if (error) {
        echttp_error (500, error);
        return 0;
    }
    return now;
}

static const char *housesun_tonight (const char *method, const char *uri,
                                     const char *data, int length) {

    time_t now = housesun_refresh ();
    if (!now) return "";

    time_t sunset;
    time_t sunrise;
    housesun_cache_tonight (now, &sunset, &sunrise);
    return housesun_content (now, sunset, sunrise);
}

static const char *housesun_yesterday (const char *method, const char *uri,
                                       const char *data, int length) {

    time_t now = housesun_refresh ();
    if (!now) return "";

    time_t sunset;
    time_t sunrise;
    housesun_cache_yesterday (&sunrise, &sunset);
    return housesun_content (now, sunset, sunrise);
}

static const char *housesun_today (const char *method, const char *uri,
                                   const char *data, int length) {

    time_t now = housesun_refresh ();
    if (!now) return "";

    time_t sunset;
    time_t sunrise;
    housesun_cache_today (&sunrise, &sunset);
    return housesun_content (now, sunset, sunrise);
}

static const char *housesun_tomorrow (const char *method, const char *uri,
                                      const char *data, int length) {

    time_t now = housesun_refresh ();
    if (!now) return "";

    time_t sunset;
    time_t sunrise;
    housesun_cache_tomorrow (&sunrise, &sunset);
    return housesun_content (now, sunset, sunrise);
}

static const char *housesun_status (const char *method, const char *uri,
                                       const char *data, int length) {

    time_t sunset;
    time_t sunrise;
    time_t now = housesun_refresh();
    if (!now) return "";

    ParserContext context;
    int root = housesun_head (&context, now);
    int top = housesun_top (context, root);

    housesun_cache_yesterday (&sunrise, &sunset);
    housesun_subcontent (context, top, "yesterday", sunrise, sunset);

    housesun_cache_today (&sunrise, &sunset);
    housesun_subcontent (context, top, "today", sunrise, sunset);

    housesun_cache_tomorrow (&sunrise, &sunset);
    housesun_subcontent (context, top, "tomorrow", sunrise, sunset);

    housesun_cache_tonight (now, &sunset, &sunrise);
    housesun_subcontent (context, top, "tonight", sunrise, sunset);

    return housesun_tail (context);
}

static void housesun_background (int fd, int mode) {

    static time_t LastCall = 0;
    time_t now = time(0);

    if (now == LastCall) return;
    LastCall = now;

    houseportal_background (now);
    housediscover (now);
    houselog_background (now);
    housesun_location_background (now);

    if (!housesun_location_ready())
        return; // We need the GPS coordinates for the almanac data.

    housesun_sunsetsunrise_location (housesun_location_timezone(),
                                     housesun_location_lat(),
                                     housesun_location_long());

    housesun_cache_background (now);
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
    if (echttp_dynamic_port()) {
        static const char *path[] = {"almanac:/sun"};
        houseportal_initialize (argc, argv);
        houseportal_declare (echttp_port(4), path, 1);
    }

    housediscover_initialize (argc, argv);
    houselog_initialize ("sun", argc, argv);

    echttp_route_uri ("/sun/status", housesun_status);
    echttp_route_uri ("/sun/tonight", housesun_tonight);
    echttp_route_uri ("/sun/today", housesun_today);
    echttp_route_uri ("/sun/yesterday", housesun_yesterday);
    echttp_route_uri ("/sun/tomorrow", housesun_tomorrow);

    echttp_static_route ("/", "/usr/local/share/house/public");
    echttp_background (&housesun_background);
    houselog_event ("SERVICE", "sun", "STARTED", "ON %s", houselog_host());
    echttp_loop();
}

