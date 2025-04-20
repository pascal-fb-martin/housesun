# HouseSun - A service providing Almanac data from sunrise-sunset.org
#
# Copyright 2025, Pascal Martin
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA  02110-1301, USA.

HAPP=housesun
HROOT=/usr/local
SHARE=$(HROOT)/share/house

# Application build. --------------------------------------------

OBJS=housesun.o housesun_location.o
LIBOJS=

all: housesun

main: housesun.o

clean:
	rm -f *.o *.a housesun

rebuild: clean all

%.o: %.c
	gcc -c -Wall -g -Os -o $@ $<

housesun: $(OBJS)
	gcc -Os -o housesun $(OBJS) -lhouseportal -lechttp -lssl -lcrypto -lrt

# Application files installation --------------------------------

install-ui:
	mkdir -p $(SHARE)/public/sun
	chmod 755 $(SHARE) $(SHARE)/public $(SHARE)/public/sun
	cp public/* $(SHARE)/public/sun
	chown root:root $(SHARE)/public/sun/*
	chmod 644 $(SHARE)/public/sun/*

install-app: install-ui
	mkdir -p $(HROOT)/bin
	mkdir -p /var/lib/house
	mkdir -p /etc/house
	rm -f $(HROOT)/bin/housesun
	cp housesun $(HROOT)/bin
	chown root:root $(HROOT)/bin/housesun
	chmod 755 $(HROOT)/bin/housesun
	touch /etc/default/housesun

uninstall-app:
	rm -rf $(SHARE)/public/housesun
	rm -f $(HROOT)/bin/housesun

purge-app:

purge-config:
	rm -rf /etc/default/housesun

# System installation. ------------------------------------------

include $(SHARE)/install.mak

# Docker installation -------------------------------------------

docker: all
	rm -rf build
	mkdir -p build
	cp Dockerfile build
	mkdir -p build$(HROOT)/bin
	cp housesun build$(HROOT)/bin
	chmod 755 build$(HROOT)/bin/housesun
	cd build ; docker build -t housesun .
	rm -rf build

