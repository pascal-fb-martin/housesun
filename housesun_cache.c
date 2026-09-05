/* HouseSun - A service providing Almanac data from sunrise-sunset.org
 *
 * Copyright 2026, Pascal Martin
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
 * housesun_cache.c -- Reuse calculated sunraise and sunset times.
 *
 * const char *housesun_cache_refresh (time_t now);
 *
 *    Launch a cache update if necessary. This return null on success,
 *    or an error message on failure. Mostly useful to indicate when
 *    almanac data is not expected to br available.
 *
 *    Warning: the update is asynchronous and will only take effect later.
 *
 * void housesun_cache_yesterday (time_t *rise, time_t *set);
 * void housesun_cache_today (time_t *rise, time_t *set);
 * void housesun_cache_tomorrow (time_t *rise, time_t *set);
 *
 *    Get the sunrise and sunset times for yesterday, today or tomorrow.
 *    The cache must have been refreshed before these calls.
 *
 * void housesun_cache_tonight (time_t now, time_t *set, time_t *rise);
 *
 *    Get the sunset and sunrise times for the upcoming night.
 *    The cache must have been refreshed before this call.
 *
 * time_t housesun_cache_updated (void);
 *
 *    Return the time when the cache data was last updated.
 *
 * const char *housesun_cache_origin (void);
 *
 *    Return a static string describing the origin of the data.
 *
 * void housesun_cache_background (time_t now);
 *
 *    A periodic function to maintain the cache data up-to-date.
 */

#include <time.h>

#include "echttp_libc.h"

#include "housesun_cache.h"
#include "housesun_sunsetsunrise.h"

struct DayCache {
    const char *name;
    struct tm date;
    time_t sunrise;
    time_t sunset;
    time_t updated;
    time_t upcoming;
};

static struct DayCache AlmanacYesterday = {"yesterday", {0}, 0, 0, 0, 0};
static struct DayCache AlmanacToday     = {"today", {0}, 0, 0, 0, 0};
static struct DayCache AlmanacTomorrow  = {"tomorrow", {0}, 0, 0, 0, 0};

static int housesun_cache_same_day (const struct tm *day,
                                        const struct DayCache *cache) {

   return (day->tm_yday == cache->date.tm_yday) &&
          (day->tm_year == cache->date.tm_year) &&
          (day->tm_gmtoff == cache->date.tm_gmtoff);
}

const char *housesun_cache_refresh (time_t now) {

    const char *error;
    struct tm today = *localtime (&now);

    if (housesun_cache_same_day (&today, &AlmanacToday) &&
        AlmanacTomorrow.updated && AlmanacYesterday.updated) return 0; // OK

    if (housesun_cache_same_day (&today, &AlmanacTomorrow)) {
        // The day changed: shift the cache entry to avoid querying what
        // is already known. This way it is available immediately.
        AlmanacYesterday = AlmanacToday;
        AlmanacToday = AlmanacTomorrow;
    }

    time_t tomorrow = now + (24*60*60);
    error = housesun_sunsetsunrise_query (AlmanacTomorrow.name);
    if (error) return error;
    AlmanacTomorrow.upcoming = tomorrow;

    time_t yesterday = now - (24*60*60);
    struct tm date = *localtime (&yesterday);
    if (!housesun_cache_same_day (&date, &AlmanacYesterday)) {
        error = housesun_sunsetsunrise_query (AlmanacYesterday.name);
        if (error) return error;
        AlmanacYesterday.upcoming = yesterday;
    }

    if (!housesun_cache_same_day (&today, &AlmanacToday)) {
        error = housesun_sunsetsunrise_query (AlmanacToday.name);
        if (error) return error;
        AlmanacToday.upcoming = now;
    }
    return 0;
}

void housesun_cache_yesterday (time_t *rise, time_t *set) {
    *rise = AlmanacYesterday.sunrise;
    *set = AlmanacYesterday.sunset;
}

void housesun_cache_today (time_t *rise, time_t *set) {
    *rise = AlmanacToday.sunrise;
    *set = AlmanacToday.sunset;
}

void housesun_cache_tomorrow (time_t *rise, time_t *set) {
    *rise = AlmanacTomorrow.sunrise;
    *set = AlmanacTomorrow.sunset;
}

void housesun_cache_tonight (time_t now, time_t *set, time_t *rise) {

    if (now > AlmanacToday.sunrise) {
       // That night is over, look for the next night.
       *set = AlmanacToday.sunset;
       *rise = AlmanacTomorrow.sunrise;
    } else {
       *set = AlmanacYesterday.sunset;
       *rise = AlmanacToday.sunrise;
    }
}

time_t housesun_cache_updated (void) {
    // If any day is missing, there is no complete data. Otherwise
    // the 'tomorrow' part is the one that get queried all the time
    if (AlmanacYesterday.updated == 0) return 0;
    if (AlmanacToday.updated == 0) return 0;
    return AlmanacTomorrow.updated;
}

const char *housesun_cache_origin (void) {
    return housesun_sunsetsunrise_origin ();
}

static void housesun_cache_update (const char *day, int rise, int set) {

    struct DayCache *cache;
    if (strsame (day, AlmanacTomorrow.name)) cache = &AlmanacTomorrow;
    else if (strsame (day, AlmanacToday.name)) cache = &AlmanacToday;
    else if (strsame (day, AlmanacYesterday.name)) cache = &AlmanacYesterday;
    else return;

    struct tm date = *localtime (&(cache->upcoming));
    date.tm_hour = rise / 3600;
    date.tm_min = (rise / 60) % 60;
    date.tm_sec = rise % 60;
    cache->sunrise = mktime (&date);

    date.tm_hour = set / 3600;
    date.tm_min = (set / 60) % 60;
    date.tm_sec = set % 60;
    cache->sunset = mktime (&date);

    date.tm_hour = date.tm_min = date.tm_sec = 0;
    cache->date = date;
    cache->updated = time(0);
}

void housesun_cache_background (time_t now) {

    static time_t LastCall = 0;

    if (!LastCall) housesun_sunsetsunrise_register (housesun_cache_update);

    if (LastCall && (now % 60)) return;
    LastCall = now;

    housesun_cache_refresh (now);
}

