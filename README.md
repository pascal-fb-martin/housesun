# HouseSun

An Almanac Service fed from sunset-sunrise.org

## Overview

This service implements the same web API as the [housealmanac](https://github.com/pascal-fb-martin/housealmanac) service, but feed its data from the free [sunrise-sunset.org](https://sunrise-sunset.org) web site.

This service uses the [houseclock](https://github.com/pascal-fb-martin/houseclock) service to obtain the GPS location. The houseclock service may run on another computer on the same local network, but it must be synchronized using a GPS receiver (houseclock is used here as a GPS location service). This service also use the default timezone of the local machine (i.e. the timezone named in /etc/timezone).

## Installation

This service depends on the House series environment:

* Install git, icoutils, openssl (libssl-dev).
* Install [echttp](https://github.com/pascal-fb-martin/echttp)
* Install [houseportal](https://github.com/pascal-fb-martin/houseportal)
* Install [houseclock](https://github.com/pascal-fb-martin/houseclock).
* Clone this repository.
* make rebuild
* sudo make install

## Configuration

This service does not need to be configured.

## Debian Packaging

The provided Makefile supports building private Debian packages. These are _not_ official packages:

- They do not follow all Debian policies.

- They are not built using Debian standard conventions and tools.

- The packaging is not separate from the upstream sources, and there is
  no source package.

To build a Debian package, use the `debian-package` target:

```
make debian-package
```

