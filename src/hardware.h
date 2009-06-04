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

#ifndef __HARDWARE_H__
#define __HARDWARE_H__

#include <string.h>
#include <time.h>

#include "usbsign.h"
#include "common.h"

int hardware_init(usbsign_handle** devh);
int hardware_close(usbsign_handle* devh);
int hardware_sendpkt(usbsign_handle* devh, char* data, unsigned int size);
int hardware_seqstart(usbsign_handle* devh);
int hardware_seqend(usbsign_handle* devh);

#endif
