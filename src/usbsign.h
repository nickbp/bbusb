#ifndef __USBSIGN_H__
#define __USBSIGN_H__

/************************************************************************\

  bbusb - BetaBrite Prism LED Sign Communicator
  Copyright (C) 2009-2011  Nicholas Parker <nickbp@gmail.com>

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

#include "config.h"

#ifdef USE_LIBUSB_10
#include <libusb-1.0/libusb.h>
typedef libusb_device_handle usbsign_handle;
#endif

#ifdef USE_LIBUSB_01
#include <usb.h>
typedef usb_dev_handle usbsign_handle;
#endif

#ifdef USE_NOUSB
typedef void usbsign_handle;
#endif

int usbsign_open(int vendorid, int productid,
        int interface, usbsign_handle** devh);
int usbsign_reset(int vendorid, int productid,
        int interface, usbsign_handle** devh);
void usbsign_close(usbsign_handle* dev, int interface);
int usbsign_send(usbsign_handle* dev, int endpoint,
        char* data, unsigned int size, int* sentcount);

#endif
