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

//Use this for testing without a sign attached:
//#define NOUSB
#define DEBUGMSG

#include "config.h"
#include "hardware.h"
#include "packet.h"

#include <stdio.h>
#include <string.h>

#define printerr(...) fprintf(stderr,__VA_ARGS__)

#endif
