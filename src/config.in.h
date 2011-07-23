#ifndef __CONFIG_H__
#define __CONFIG_H__

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

#define VERSION_STRING "@bbusb_VERSION_MAJOR@.@bbusb_VERSION_MINOR@.@bbusb_VERSION_PATCH@"

#cmakedefine USE_LIBUSB_10
#cmakedefine USE_LIBUSB_01
#cmakedefine USE_NOUSB

#cmakedefine DEBUG

#ifdef USE_LIBUSB_10
#define USB_TYPE "libusb-1.0"
#endif
#ifdef USE_LIBUSB_01
#define USB_TYPE "libusb-0.1"
#endif
#ifdef USE_NOUSB
#define USB_TYPE "nousb"
#endif

#include <stdio.h>

extern int config_debug_enabled;
extern FILE *config_fout;
extern FILE *config_ferr;

void config_debug(const char* format, ...);//TODO a macro which is disabled in release builds
void config_debugnn(const char* format, ...);
void config_log(const char* format, ...);
void config_lognn(const char* format, ...);
void config_error(const char* format, ...);
void config_errornn(const char* format, ...);

#endif
