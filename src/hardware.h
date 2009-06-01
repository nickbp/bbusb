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

#include "main.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef NOUSB
typedef void libusb_device_handle;
#else
#include <libusb-1.0/libusb.h>
#endif

#define MIN_TEXTFILE_DATA_SIZE 128
#define MAX_TEXTFILE_DATA_SIZE 4096 //arbitrary tested-safe limits
#define MAX_STRINGFILE_DATA_SIZE 125 //need to partition text into multiple STRINGs :(
#define MAX_STRINGFILE_GROUP_SIZE 4*125//sign doesnt want to display more than 4 STRINGs in a TEXT

int hardware_init(libusb_device_handle** devh);
int hardware_close(libusb_device_handle* devh);
int hardware_sendpkt(libusb_device_handle* devh, char* data, unsigned int size);
int hardware_seqstart(libusb_device_handle* devh);
int hardware_seqend(libusb_device_handle* devh);
#endif
