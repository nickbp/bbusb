/************************************************************************\

  bbusb - BetaBrite Prism LED Sign Communicator
  Copyright (C) 2009  Nicholas Parker <nickbp@gmail.com>

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

\************************************************************************/

#ifndef __MAIN_H__
#define __MAIN_H__


//Test without a sign attached:
//#define NOUSB
//Use libusb-0.1 instead of libusb-1.0:
//#define OLDUSB
//Print generated packets to stdout:
//#define DEBUGMSG

#define VERSION "0.1"

#ifdef NOUSB
#define USBTYPE "nousb"
#else
#ifdef OLDUSB
#define USBTYPE "libusb-0.1"
#else
#define USBTYPE "libusb-1.0"
#endif
#endif

#include <string.h>

#include "common.h"
#include "config.h"
#include "usbsign.h"
#include "packet.h"
#include "hardware.h"

#endif
