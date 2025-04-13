/* HouseSun - A service providing Almanac data from sunset-sunrise.org
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
 * housesun_location.h - Retrieve the location of the house.
 */
int         housesun_location_ready (void);
double      housesun_location_lat (void);
double      housesun_location_long (void);
const char *housesun_location_timezone (void);
void        housesun_location_background (time_t now);

