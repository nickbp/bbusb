/************************************************************************\

  bbusb - BetaBrite Prism LED Sign Communicator
  BetaBrite packet construction
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

#include "packet.h"

//available filename ranges, inclusive: see pg50
//static const char filenamepool_firsts[] = {0x20,0x36,0x40,0},
//    filenamepool_lasts[] = {0x2f,0x3e,0x7e,0};
//NOTE: my sign will fail if I use more than 46 filenames,
// so only allow that many filenames in the pool:
#define MAX_LABEL_COUNT 46
static const char filenamepool_firsts[] = {0x20,0x36,0x40,0},
    filenamepool_lasts[] = {0x2f,0x3e,0x54,0};//stop at "T" to enforce max 46 names

char packet_next_filename(char prev_filename) {
    if (prev_filename <= 0) {
        return filenamepool_firsts[0];
    }
    int i = 0;
    while (filenamepool_firsts[i] != 0) {
        if (prev_filename < filenamepool_lasts[i]) {
            if (prev_filename >= filenamepool_firsts[i]) {
                //return value within this range
                return prev_filename+1;
            } else {
                //last is before this range, return start of this range
                return filenamepool_firsts[i];
            }
        }
        //skip to next range
        ++i;
    }
    //reached end of pool, give up
    printerr("Too many messages in config file (ran out of message labels).\n");
    printerr("Max %d labels: cmds cost %d labels each, txts cost 1 label each.\n", MAX_LABEL_COUNT, MAX_STRINGFILE_GROUP_COUNT+1);
    printerr("In your config file, reduce the total number of messages, or convert some cmds to txts to save space.\n");
    return -1;
}

int packet_buildmemconf(char** outputptr, struct bb_frame* frames) {
    //MEMCONFIG packet format:
    //0x0 0x0 0x0 0x0 0x0 0x1 Z 0x0 0x0 0x2 E $ filespec [filespec ...] 0x4
    //11-byte filespec for TEXT: filename A L 0xsize(4char) F F 0 0
    //11-byte filespec for STRING: filename B L 0xsize(4char) 0 0 0 0
    char cmdcode = 'E', specfuncode = '$',
        txtflag = 'A', stringflag = 'B',
        lockflag = 'L';//must be L for STRINGs
    char txttail[] = {'F','F','0','0'},
        stringtail[] = {'0','0','0','0'};
    size_t sizelen = 4,
        pktsize = sizeof(cmdcode) + sizeof(specfuncode);
    struct bb_frame* curframe = frames;
    while (curframe != NULL) {
        pktsize += 11*sizeof(char);//all filespecs are 11-byte (see notes above)
        curframe = curframe->next;
    }

    char* data = (char*) calloc(pktsize,sizeof(char));

    size_t offset = 0;
    memcpy(&data[offset], &cmdcode, sizeof(cmdcode));
    offset = sizeof(cmdcode);
    memcpy(&data[offset], &specfuncode, sizeof(specfuncode));
    offset += sizeof(specfuncode);

    curframe = frames;
    while (curframe != NULL) {
        char flag;
        char* tail;
        int datasize;
        if (curframe->frame_type == TEXT_FRAME_TYPE) {
            flag = txtflag;
            tail = txttail;
            //alloc only the size of the (static) data itself:
            if (curframe->data == NULL) {
                datasize = 0;
            } else {
                datasize = strlen(curframe->data);
            }
            if (datasize < (int)MIN_TEXTFILE_DATA_SIZE) {
                datasize = MIN_TEXTFILE_DATA_SIZE;
            } else if (datasize > (int)MAX_TEXTFILE_DATA_SIZE) {
                datasize = MAX_TEXTFILE_DATA_SIZE;
            }
#ifdef DEBUGMSG
            printf("datasize=%d (0x%x) for %s\n",datasize,datasize,curframe->data);
#endif
        } else if (curframe->frame_type == STRING_FRAME_TYPE) {
            flag = stringflag;
            tail = stringtail;
            //always alloc full size, even if string is currently empty:
            datasize = MAX_STRINGFILE_DATA_SIZE;
        } else {
            printerr("Internal error: Unknown frame type %d\n",curframe->frame_type);
            return -1;
        }

        memcpy(&data[offset], &curframe->filename, sizeof(curframe->filename));
        offset += sizeof(curframe->filename);

        memcpy(&data[offset], &flag, sizeof(flag));
        offset += sizeof(flag);
        memcpy(&data[offset], &lockflag, sizeof(lockflag));
        offset += sizeof(lockflag);

        char textfile_size_hex[sizelen+1];
        snprintf(textfile_size_hex,sizelen+1,"%04X",datasize);//creates \0
        memcpy(&data[offset], textfile_size_hex, sizelen);//omits the \0
        offset += sizelen;
        memcpy(&data[offset], tail, 4*sizeof(char));
        offset += 4*sizeof(char);

        curframe = curframe->next;
    }

    *outputptr = data;
    return pktsize;
}

int packet_buildstring(char** outputptr, char filename, char* text) {
    //STRING packet format:
    //0x0 0x0 0x0 0x0 0x0 0x1 Z 0x0 0x0 0x2 G filename text 0x4
    char cmdcode = 'G';

    size_t textlen = 0;
    if (text != NULL) {
        textlen = strlen(text);
    }
    if (textlen > MAX_STRINGFILE_DATA_SIZE) {
        printerr("Warning: shrank a string frame to fit %d bytes.\n",MAX_STRINGFILE_DATA_SIZE);
        textlen = MAX_STRINGFILE_DATA_SIZE;
    }

    size_t pktsize = sizeof(cmdcode) + sizeof(filename) + textlen;

    char* data = (char*) calloc(pktsize,sizeof(char));

    size_t offset = 0;
    memcpy(&data[offset], &cmdcode, sizeof(cmdcode));
    offset = sizeof(cmdcode);
    memcpy(&data[offset], &filename, sizeof(filename));
    offset += sizeof(filename);

    if (textlen > 0) {
        memcpy(&data[offset], text, textlen);
    }

    *outputptr = data;
    return pktsize;//negative return: this is a fragment
}

int packet_buildtext(char** outputptr, char filename,
                     char mode, char special, char* text) {
    //TEXT packet format:
    //0x0 0x0 0x0 0x0 0x0 0x1 Z 0x0 0x0 0x2 A filename 0x1B 0x30 mode (special) text 0x4
    const char cmdcode = 'A', modedivider = 0x1b, position = 0x30;

    size_t textlen = 0;
    if (text != NULL) {
        textlen = strlen(text);
    }
    if (textlen > MAX_TEXTFILE_DATA_SIZE) {
        printerr("Warning: shrank a text frame to fit %d bytes.\n",
                 MAX_TEXTFILE_DATA_SIZE);
        textlen = MAX_TEXTFILE_DATA_SIZE;
    }

    size_t pktsize = sizeof(cmdcode) + sizeof(filename) +
        sizeof(modedivider) + sizeof(position) + sizeof(mode) + textlen;
    if (special != NO_SPECIAL) {
        pktsize += sizeof(special);
    }

    char* data = (char*) calloc(pktsize,sizeof(char));

    size_t offset = 0;
    memcpy(&data[offset], &cmdcode, sizeof(cmdcode));
    offset = sizeof(cmdcode);
    memcpy(&data[offset], &filename, sizeof(filename));
    offset += sizeof(filename);

    memcpy(&data[offset], &modedivider, sizeof(modedivider));
    offset += sizeof(modedivider);
    memcpy(&data[offset], &position, sizeof(position));
    offset += sizeof(position);
    memcpy(&data[offset], &mode, sizeof(mode));
    offset += sizeof(mode);
    if (special != NO_SPECIAL) {
        memcpy(&data[offset], &special, sizeof(special));
        offset += sizeof(special);
    }

    if (textlen > 0) {
        memcpy(&data[offset], text, textlen);
    }

    *outputptr = data;
    return pktsize;
}

int packet_buildrunseq(char** outputptr, struct bb_frame* frames) {
    //RUNSEQ packet format:
    //0x0 0x0 0x0 0x0 0x0 0x1 Z 0x0 0x0 0x2 E 0x2e T U filename [filename ...] 0x4
    const char cmdcode = 'E', runseqcode = 0x2E,
        runseqtype = 'S', lockflag = 'U';

    size_t pktsize = sizeof(cmdcode) + sizeof(runseqcode) + 
        sizeof(runseqtype) + sizeof(lockflag);
    struct bb_frame* curframe = frames;
    while (curframe != NULL) {
        //string frames are only referenced by text frames: don't add to runseq
        if (curframe->frame_type != STRING_FRAME_TYPE) {
            pktsize += sizeof(char);
        }
        curframe = curframe->next;
    }

    char* data = (char*) calloc(pktsize,sizeof(char));

    size_t offset = 0;
    memcpy(&data[offset], &cmdcode, sizeof(cmdcode));
    offset = sizeof(cmdcode);
    memcpy(&data[offset], &runseqcode, sizeof(runseqcode));
    offset += sizeof(runseqcode);

    memcpy(&data[offset], &runseqtype, sizeof(runseqtype));
    offset += sizeof(runseqtype);
    memcpy(&data[offset], &lockflag, sizeof(lockflag));
    offset += sizeof(lockflag);

    curframe = frames;
    while (curframe != NULL) {
        //string frames are only referenced by text frames: don't add to runseq
        if (curframe->frame_type != STRING_FRAME_TYPE) {
            memcpy(&data[offset], &curframe->filename, sizeof(curframe->filename));
            offset += sizeof(curframe->filename);
        }

        curframe = curframe->next;
    }

    *outputptr = data;
    return pktsize;
}

void packet_delete(struct bb_frame* delme) {
    if (delme->next != NULL) {
        packet_delete(delme->next);
        free(delme->next);
    }
    if (delme->data != NULL) {
        free(delme->data);
    }
}

