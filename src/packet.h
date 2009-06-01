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

#ifndef __PACKET_H__
#define __PACKET_H__

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>

enum frame_type_t { STRING_FRAME_TYPE=1, TEXT_FRAME_TYPE };
struct bb_frame {
    char mode;//TEXT-only
    char mode_special;//TEXT-only
    char filename;
    enum frame_type_t frame_type;
    char* data;
    struct bb_frame* next;
};

#include "hardware.h"

#define NO_SPECIAL 0

int packet_buildrunseq(char** outputptr, struct bb_frame* frames);
int packet_buildtext(char** outputptr, char filename,
                     char mode, char special, char* text);
int packet_buildstring(char** outputptr, char filename, char* text);
int packet_buildmemconf(char** outputptr, struct bb_frame* frames);

void packet_delete(struct bb_frame* delme);

#endif
