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
 * housesun_sunsetsunrise.h -- Sun set and rise times from sunset-sunrise.org
 */

void housesun_sunsetsunrise_location (const char *timezone,
                                      float latitude, float longitude);

typedef void SunResponseListener (const char *day, int rise, int set);
void housesun_sunsetsunrise_register (SunResponseListener *listener);

const char *housesun_sunsetsunrise_query (const char *day);

const char *housesun_sunsetsunrise_origin (void);

