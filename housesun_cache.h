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
 * housesun_cache.h -- Reuse calculated sunrise and sunset times.
 */

const char *housesun_cache_refresh (time_t now);

void housesun_cache_yesterday (time_t *rise, time_t *set);
void housesun_cache_today (time_t *rise, time_t *set);
void housesun_cache_tomorrow (time_t *rise, time_t *set);

void housesun_cache_tonight (time_t now, time_t *set, time_t *rise);

time_t housesun_cache_updated (void);
const char *housesun_cache_origin (void);
void housesun_cache_background (time_t now);
