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

  Build (with libusb-1.0 installed):
   gcc -L/usr/lib64 -lusb-1.0 --std=c99 
       -Wall -Wextra -pedantic -g -o bbusb bbusb.c
  Last updated:
   20090506

\************************************************************************/

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <time.h>

//Use this for testing without a sign attached:
//#define BB_NOUSB
#define DEBUGMSG

#ifdef BB_NOUSB
typedef void libusb_device_handle;
#else
#include <libusb-1.0/libusb.h>
#endif

#define printerr(...) fprintf(stderr,__VA_ARGS__)

//Found via lsusb while my sign was plugged in (yes this is real):
const int vendorid = 0x8765, productid = 0x1234;
const unsigned int MIN_TEXTFILE_DATA_SIZE = 128,
    MAX_TEXTFILE_DATA_SIZE = 4096, //arbitrary tested-safe limits
    MAX_STRINGFILE_DATA_SIZE = 125, //need to partition text into multiple STRINGs :(
    MAX_STRINGFILE_GROUP_SIZE = 4*125;//sign doesnt want to display more than 4 STRINGs in a TEXT

//fixes popen/pclose warnings when being -pedantic:
extern FILE* popen(const char* command, const char* modes);
extern int pclose(FILE *stream);
extern char *strtok_r(char *str, const char *delim, char **saveptr);

struct timespec {
    time_t tv_sec;        /* seconds */
    long   tv_nsec;       /* nanoseconds */
};
extern int nanosleep(const struct timespec *req, struct timespec *rem);

enum frame_type_t { STRING_FRAME_TYPE=1, TEXT_FRAME_TYPE };
struct bb_frame {
    char mode;//TEXT-only
    char mode_special;//TEXT-only
    char filename;
    enum frame_type_t frame_type;
    char* data;
    struct bb_frame* next;
};

//----------------------------------------
// USB communication layer
//----------------------------------------

int bb_usb_init(libusb_device_handle** devh) {
#ifdef BB_NOUSB
    printf("USB Init %p\n",(void*)*devh);
    return 0;
#else
    if (devh == NULL) {
        printerr("Internal error: Bad pointer to init\n");
        return -1;
    } else if (*devh != NULL) {
        printerr("Internal error: Can't init twice\n");
        return -1;
    }

    int ret = libusb_init(NULL);
    if (ret < 0) {
        printerr("Got error %d when initializing libusb\n", ret);
        return ret;
    }

    *devh = libusb_open_device_with_vid_pid(NULL, vendorid, productid);
    if (*devh == NULL) {
        printerr("Could not find/open USB device with vid=0x%X pid=0x%X. Is the sign plugged in?\n",
                vendorid, productid);
        return -1;
    }

    ret = libusb_claim_interface(*devh,0);
    if (ret < 0) {
        printerr("Could not claim device (%d)\n", ret);
        return ret;
    }

    return 0;
#endif
}

int bb_usb_close(libusb_device_handle* devh) {
#ifdef BB_NOUSB
    printf("USB Close %p\n",devh);
    return 0;
#else
    if (devh != NULL) {
        libusb_release_interface(devh, 0);
        libusb_close(devh);
        devh = NULL;
    }
    libusb_exit(NULL);

    return 0;
#endif
}

void bb_printpkt(char* data, unsigned int size) {
    unsigned int i;
    printf("%u: ",size);
    for (i = 0; i < size; i++) {
        printf("%X(%c) ", data[i], data[i]);
    }
    printf("\n");
}

void sleep_ms(int ms) {
    struct timespec hundredms;
    hundredms.tv_sec = 0;
    hundredms.tv_nsec = ms*1000000;
    nanosleep(&hundredms, NULL);
}

//A sequence is a series of packets, and this is what begins and ends them (pg13)
const char sequence_header[] = {0,0,0,0,0,1,'Z','0','0'},
    sequence_footer[] = {4},
    packet_header[] = {2}, packet_footer[] = {3};

int bb_sendraw(libusb_device_handle* devh, char* data, unsigned int size) {
#ifdef BB_NOUSB
    int ret = size;
#else
    if (devh == NULL) {
        printerr("Unable to send: Device handle is null\n");
        return -1;
    }

    int sent;
    int ret = libusb_bulk_transfer(devh, (2 | LIBUSB_ENDPOINT_OUT),
                                   (unsigned char*)data, size,
                                   &sent, 1000);

    if (ret < 0) {
        printerr("Got USB error %d when sending %d bytes\n", ret, size);
        return -1;
    } else {
        ret = sent;
    }
#endif

    if (ret > 0 || devh != NULL) {
        //devh test is just to hide 'unused' warning
#ifdef DEBUGMSG
        bb_printpkt(data, size);
#endif
    } else {
        printerr("Bad packet size %d or device %p\n",ret,(void*)devh);
        return -1;
    }

    return ret;
}

int bb_sendpkt(libusb_device_handle* devh, char* data, unsigned int size) {
    //create packet with header/footer chars added:
    char packet[size+sizeof(packet_footer)];
    memcpy(&packet[0],data,size);
    memcpy(&packet[size],packet_footer,sizeof(packet_footer));

    if (bb_sendraw(devh,(char*)packet_header,
                   sizeof(packet_header)) != sizeof(packet_header)) {
        return -1;
    }
    sleep_ms(100);//"100 millisecond delay after the [pkt header]" (pg14)
    return bb_sendraw(devh,packet,
                      size+sizeof(packet_footer))-sizeof(packet_footer);
}
    

//----------------------------------------
// BetaBrite packet construction
//----------------------------------------

const char NO_SPECIAL = 0;

int bb_buildmemconfpacket(char** outputptr, struct bb_frame* frames) {
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

int bb_buildstringpacket(char** outputptr, char filename, char* text) {
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

int bb_buildtextpacket(char** outputptr, char filename,
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

int bb_buildrunseqpacket(char** outputptr, struct bb_frame* frames) {
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

void delchildren(struct bb_frame* delme) {
    if (delme->next != NULL) {
        delchildren(delme->next);
        free(delme->next);
    }
    if (delme->data != NULL) {
        free(delme->data);
    }
}

//----------------------------------------
// Config file parsing
//----------------------------------------

int readline(char** lineptr, FILE* config) {
    size_t buflen = 128;
    char* line = malloc(buflen);
    if (line == NULL) {
        return -1;
    }

    char c;
    size_t i = 0;
    while ((c = fgetc(config)) != EOF) {
        if (c == '\r') {
            continue;
        } else if (c == '\n') {
            if (i == 0) {
                continue;//its an empty line, keep going to next line
            } else {
                break;//end of line
            }
        } else {
            if (i == buflen) {
                buflen *= 2;
                line = realloc(line,buflen);
                if (line == NULL) {
                    return -1;
                }
            }
            line[i++] = c;
        }
    }
    //append \0 to line, shrink buffer to match length of line:
    if (i > 0) {
        line = realloc(line,i+1);
        line[i] = '\0';
    }
    *lineptr = line;
    return i;
}

int checkmode(char* mode, char* opt, int linenum) {
    if (mode == NULL || strlen(mode) == 0) {
        printerr("Syntax error, line %d: Mode field isn't specified.\n",linenum);
        return -1;
    } else if (strlen(mode) == 1) {
        if (opt == NULL) {
            printerr("Syntax error, line %d: 2-char special mode required to omit content field.\n",linenum);
            return -1;
        } else if (mode[0] == 'n') {
            printerr("Syntax error, line %d: Mode 'n' must be accompanied with 2nd char specifying a special mode.\n",linenum);
            return -1;
        }
    } else if (strlen(mode) == 2 && mode[0] != 'n') {
        printerr("Syntax error, line %d: Mode may only be 2 chars long if the first char is \"n\".\n",linenum);
        return -1;
    } else if (strlen(mode) > 2) {
        printerr("Syntax error, line %d: Mode may only be 1 to 2 chars long.\n",linenum);
        return -1;
    }
    
    return 0;
}

int runcmd(char** output, char* command) {
    FILE* result_stream = (FILE*)popen(command,"r");
    if (result_stream == NULL) {
        printerr("Unable to open stream to command \"%s\".\n",command);
        return -1;
    }
    size_t buflen = 128, i = 0;
    char* result = malloc(buflen);

    char c;
    while ((c = fgetc(result_stream)) != EOF) {
        if (i == buflen) {
            buflen *= 2;
            result = realloc(result,buflen);
            if (result == NULL) {
                printerr("Memory allocation error!\n");
                return -1;
            }
        }
        result[i++] = c;
    }

    if (pclose(result_stream) != 0) {
        printerr("Error: Command \"%s\" returned an error. Aborting.\n",command);
        return -1;
    }

    //append \0 to result, shrink buffer to match length of result:
    result = realloc(result,i+1);
    result[i] = '\0';

    *output = result;

    return 0;
}

//Replacement table for most stuff (excludes color,scolor,speed)
char* replacesrc[] = {
    "<left>","<br>","<blink>","</blink>",
    "<small>","<normal>",
    "<wide>","</wide>","<dblwide>","</dblwide>",
    "<serif>","</serif>","<shadow>","</shadow>",
    "&uparrow;","&downarrow;","&leftarrow;","&rightarrow;",
    "&cent;","&gbp;","&yen;","&euro;",
    "&pacman;","&boat;","&ball;","&phone;",
    "&heart;","&car;","&handicap;","&rhino;",
    "&mug;","&satdish;","&copy;","&female;",
    "&male;","&bottle;","&disk;","&printer;",
    "&note;","&infinity;",0
};
char* replacedst[] = {
    "\x1e" "1","\x0c","\x07" "1","\x07" "0",
    "\x1a" "1","\x1a" "3",
    "\x1d" "01","\x1d" "00","\x1d" "11","\x1d" "10",
    "\x1d" "51","\x1d" "50","\x1d" "71","\x1d" "70",
    "\xc4","\xc5","\xc6","\xc7",
    "\x9b","\x9c","\x9d","\xc2",
    "\xc8","\xc9","\xca","\xcb",
    "\xcc","\xcd","\xce","\xcf",
    "\xd0","\xd1","\xd2","\xd3",
    "\xd4","\xd5","\xd6","\xd7",
    "\xd8","\xd9",0
};

int COLOR_LEN = 6;
int parse_color_code(char* out, char* in) {
    //color mapping: '0'->'00', '1'->'40', '2'->'80', '3'->'C0'
    //ex: '123' -> '4080C0'
    int i;
    for (i = 0; i < 3; i++) {
        out[2*i+1] = '0';
        switch (in[i]) {
        case '3':
            out[2*i] = 'C';
            break;
        case '2':
            out[2*i] = '8';
            break;
        case '1':
            out[2*i] = '4';
            break;
        case '0':
            out[2*i] = '0';
            break;
        default:
            printerr("Found invalid color \"%c\" in <scolorRGB>/<colorRGB> (R,G,B=0-3).\n",
                     in[i]);
            return -1;
        }
    }
    return 0;
}

int parse_inline_cmds(char** outptr, int* output_is_trimmed, char* in, unsigned int maxout) {
#ifdef DEBUGMSG
    printf("orig: %s\n",in);
#endif
    size_t iin = 0, iout = 0;
    char* out = malloc(maxout);
    while (iin < strlen(in)) {

        if (iout == maxout) {
            //stop!: reached max output size
            *output_is_trimmed = 1;
            break;
        }

        if (in[iin] == '\n' || in[iin] == '\t') {
            //replace newlines and tabs with spaces:
            out[iout++] = ' ';
            ++iin;
        } else if (in[iin] < 0x20) {
            //ignore all other special chars <0x20:
            ++iin;
        } else if (in[iin] == '<' || in[iin] == '&') {
            int i = 0, parsedlen = 0;
            char addme[32];
            while (replacesrc[i] != 0) {
                //search replacement table:
                if (strncmp(replacesrc[i],&in[iin],
                            strlen(replacesrc[i])) == 0) {
                    strncpy(addme,replacedst[i],strlen(replacedst[i])+1);
                    parsedlen = strlen(replacesrc[i]);
                    break;
                }
                ++i;
            }

            if (parsedlen == 0) {
                //special tags: colorRGB/scolorRGB(0-3) speedN(1-6)
                if (strncmp("<color",&in[iin],6) == 0 &&
                    index(&in[iin+6],'>') == &in[iin+9]) {//<colorRGB>
                    if (parse_color_code(&addme[2],&in[iin+6]) >= 0) {
                        addme[0] = 0x1c;
                        addme[1] = 'Z';
                        addme[COLOR_LEN+2] = 0;

                        parsedlen = 10;
                    }
                } else if (strncmp("<scolor",&in[iin],7) == 0 &&
                           index(&in[iin+7],'>') == &in[iin+10]) {//<colorRGB>
                    if (parse_color_code(&addme[2],&in[iin+7]) >= 0) {
                        addme[0] = 0x1c;
                        addme[1] = 'Y';
                        addme[COLOR_LEN+2] = 0;

                        parsedlen = 11;
                    }
                } else if (strncmp("<speed",&in[iin],6) == 0 &&
                           index(&in[iin+6],'>') == &in[iin+7]) {//<speedN>
                    char speedin = in[iin+6];
                    if (speedin >= '1' && speedin <= '5') {
                        //speeds 1(0x31)-5(0x35) -> char 0x15-0x19
                        addme[0] = speedin-'0'+0x14;
                        addme[1] = 0;

                        parsedlen = 8;
                    } else if (speedin == '6') {
                        //speed 6 -> char 0x9 (nohold)
                        addme[0] = 0x9;
                        addme[1] = 0;

                        parsedlen = 8;
                    } else {
                        printerr("Found invalid speed \"%c\" in <speedN> (N=1-6).\n",
                                 speedin);
                    }
                        
                }
            }

            if (parsedlen != 0) {
                int addme_size = strlen(addme);
                if (addme_size + iout > maxout) {
                    //stop!: too big to fit in buffer
                    *output_is_trimmed = 1;
                    break;
                } else {
                    memcpy(&out[iout],addme,addme_size);
                    iout += addme_size;
                    iin += parsedlen;
                }
            } else {
                out[iout++] = in[iin++];//nothing found, pass thru the '<'
            }
        } else {
            out[iout++] = in[iin++];
        }
    }

    //append \0 to result, shrink buffer to match length of result:
    out = realloc(out,iout+1);
    out[iout] = '\0';

    *outptr = out;
#ifdef DEBUGMSG
    printf("new (%d): %s\n",(int)strlen(out),out);
#endif
    return iin;//tell caller how far we got through their data
}

//available filename ranges, inclusive: see pg50
const char filenamepool_firsts[] = {0x20,0x36,0x40,0},
    filenamepool_lasts[] = {0x2f,0x3e,0x7e,0};
char get_next_filename(char prev_filename) {
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
    printerr("Ran out of message labels in available pool.\n");
    printerr("In your config, reduce the total number of messages or the size allocated for cmd messages.\n");
    return -1;
}

int parsecfg(struct bb_frame** output, FILE* config) {
    int error = 0, linenum = 0;
    char filename = 0;
    char* line = NULL;
    ssize_t line_len;

    struct bb_frame* head = NULL;
    struct bb_frame* curframe = NULL;
    struct bb_frame** nextframeptr = &curframe;

    while ((line_len = readline(&line,config)) > 0) {
        ++linenum;

#ifdef DEBUGMSG
        printf("%s\n",line);
#endif

        char delim[] = {' ','\0'};
        char delim_endline[] = {'\n','\0'};
        char* tmp;
        char* cmd = strtok_r(line,delim,&tmp);

        if (strcmp(cmd,"txt") == 0) {

            char* mode = strtok_r(NULL,delim,&tmp);
            char* text = strtok_r(NULL,delim_endline,&tmp);
            if (checkmode(mode,text,linenum) < 0) {
                error = 1;
                break;
            }

            *nextframeptr = malloc(sizeof(struct bb_frame));
            curframe = *nextframeptr;

            curframe->mode = tolower(mode[0]);
            if (strlen(mode) > 1) {
                curframe->mode_special = toupper(mode[1]);
            } else {
                curframe->mode_special = NO_SPECIAL;
            }

            if (text == NULL) {
                curframe->data = NULL;
            } else {
                int is_trimmed = 0;

                int charsparsed = parse_inline_cmds(&curframe->data,&is_trimmed,
                                                    text,MAX_TEXTFILE_DATA_SIZE);

                if (is_trimmed) {
                    printerr("Warning, line %d: Data has been truncated at input index %d to fit %d available output bytes.\n",
                             linenum,charsparsed,MAX_TEXTFILE_DATA_SIZE);
                    printerr("Input vs output bytecount can vary if you used inline commands in your input.\n");
                }
            }

            curframe->frame_type = TEXT_FRAME_TYPE;

            filename = get_next_filename(filename);
            if (filename <= 0) {
                error = 1;
                break;
            }
            curframe->filename = filename;

            curframe->next = NULL;
            nextframeptr = &curframe->next;

        } else if (strcmp(cmd,"cmd") == 0) {

            char* mode = strtok_r(NULL,delim,&tmp);

            //get number of STRINGs to be made:
            char* maxsize_str = strtok_r(NULL,delim,&tmp);
            char* maxsize_str2;
            errno = 0;
            int maxsize = strtol(maxsize_str,&maxsize_str2,10);
            if ((errno != 0 && maxsize == 0) || maxsize_str2 == maxsize_str) {
                printerr("Error, line %d: Allocation size \"%s\" is not a valid integer.\n",
                         linenum,maxsize_str);
                error = 1;
                break;
            }
            if (maxsize > (int)MAX_STRINGFILE_GROUP_SIZE) {
                printerr("Error, line %d: Allocation size \"%d\" exceeds the maximum size of %d.\n",
                         linenum,maxsize,MAX_STRINGFILE_GROUP_SIZE);
                error = 1;
                break;
            }
            int stringcount = maxsize / MAX_STRINGFILE_DATA_SIZE;
            if (maxsize % MAX_STRINGFILE_DATA_SIZE != 0) {
                ++stringcount;
            }

            char* command = strtok_r(NULL,delim_endline,&tmp);
            if (checkmode(mode,command,linenum) < 0) {
                error = 1;
                break;
            }

            char* raw_result;
            if (runcmd(&raw_result,command) < 0) {
                error = 1;
                break;
            }
            
            //data for the TEXT frame which will reference these STRING frames:
            char refchar = 0x10;//format for reference is "0x10, filename" (pg55)
            char* textrefs = (char*)calloc(2*stringcount+1,sizeof(char));
            textrefs[2*stringcount] = 0;//include \0

            //Create and append STRING frames:
            int i, cumulative_parsed = 0;
            for (i = 0; i < stringcount; i++) {
                *nextframeptr = malloc(sizeof(struct bb_frame));
                curframe = *nextframeptr;
                if (head == NULL) {
                    head = curframe;
                }

                int is_trimmed = 0;
                char* parsed_result;
                int charsparsed = parse_inline_cmds((char**)&parsed_result,&is_trimmed,
                                                    &raw_result[cumulative_parsed],
                                                    MAX_STRINGFILE_DATA_SIZE);
                cumulative_parsed += charsparsed;
                if (is_trimmed && i+1 == stringcount) {
                    printerr("Warning, line %d: Data has been truncated at input index %d to fit %d available output bytes.\n",
                             linenum,cumulative_parsed,stringcount*MAX_STRINGFILE_DATA_SIZE);
                    printerr("Input vs output bytecount can vary if you used inline commands in your input.\n");
                }

                curframe->data = malloc(MAX_STRINGFILE_DATA_SIZE+1);
                strncpy(curframe->data,parsed_result,
                        MAX_STRINGFILE_DATA_SIZE);
                curframe->data[MAX_STRINGFILE_DATA_SIZE] = 0;
                free(parsed_result);
#ifdef DEBUGMSG
                printf(">%d %s\n",i,curframe->data);
#endif

                filename = get_next_filename(filename);
                if (filename <= 0) {
                    error = 1;
                    break;
                }
                curframe->filename = filename;
                //add a reference for ourselves to the TEXT frame:
                textrefs[2*i] = refchar;
                textrefs[2*i+1] = filename;

                curframe->frame_type = STRING_FRAME_TYPE;

                curframe->next = NULL;
                nextframeptr = &curframe->next;
            }
            free(raw_result);
            if (error == 1) {
                break;
            }

            //Append TEXT frame containing references to those STRINGs:
            *nextframeptr = malloc(sizeof(struct bb_frame));
            curframe = *nextframeptr;

            curframe->mode = mode[0];
            if (strlen(mode) > 1) {
                curframe->mode_special = mode[1];
            } else {
                curframe->mode_special = NO_SPECIAL;
            }

            curframe->data = textrefs;

            curframe->frame_type = TEXT_FRAME_TYPE;

            filename = get_next_filename(filename);
            if (filename <= 0) {
                error = 1;
                break;
            }
            curframe->filename = filename;

            curframe->next = NULL;
            nextframeptr = &curframe->next;

        } else if ((strlen(cmd) >= 2 && cmd[0] == '/' && cmd[1] == '/') ||
                   (strlen(cmd) >= 1 && cmd[0] == '#')) {

            //do nothing

        } else {

            printerr("Syntax error, line %d: Unknown command.\n",linenum);
            error = 1;
            break;

        }

        if (head == NULL) {
            head = curframe;
        }

        free(line);
        line = NULL;
    }

    if (line != NULL) {
        free(line);
        line = NULL;
    }

    *output = head;

    return (error == 0) ? 0 : -1;
}

//----------------------------------------
// Commandline parsing and main
//----------------------------------------

void help(char* appname) {
    printerr("Usage: %s <-i/-u> [configfile]\n",appname);
    printerr("\n  -i:\tre-initialize the sign (required when config file is new or modified since last update)\n");
    printerr("  -u:\tupdate the sign contents without init (avoids display interruption)\n");
    printerr("  configfile:\tPath to a config file (see syntax below).\n");
    printerr("             \tIf configfile is unspecified, the configuration will be read from stdin.\n");
    printerr("\n Config Syntax:\n");
    printerr("  #comment\n");
    printerr("  //comment\n");
    printerr("  txt <transition> [text (opt if transition=nX)]\n");
    printerr("  cmd <transition> <alloc size (max=%d)> <shell command>\n",
             MAX_STRINGFILE_GROUP_SIZE);
    printerr("\n Available Transition Codes (pg89-90)\n");
    printerr(" Note: Some \"nX\" transitions don't work for \"cmd\"s.\n");
    printerr("       Get around this by prefixing with an empty \"txt\" (ex: txt nx\\ncmd ...)\n");
    printerr("  a rotate \tp rollin \tn8 welcome\n");
    printerr("  b hold \tq rollout \tn9 sale\n");
    printerr("  c flash \tr wipein \tna newsflash\n");
    printerr("  e rollup \ts wipeout \tnb happy4th\n");
    printerr("  f rolldn \tt comprotate \tnc cyclecolor\n");
    printerr("  g rolleft \tn0 twinkle \tns thankyou\n");
    printerr("  h rollright \tn1 sparkle \tnu nosmoking\n");
    printerr("  i wipeup \tn2 snow \tnv dontdrink\n");
    printerr("  j wipedn \tn3 interlock \tnw fish\n");
    printerr("  k wipeleft \tn4 switch \tnx fireworks\n");
    printerr("  l wiperight \tn5 cyclecolor \tny xmas\n");
    printerr("  m scroll \tn6 spray \tnz smile\n");
    printerr("  o automode \tn7 starburst\n");
    printerr("\n Inline Text Format Syntax (pg81-82):\n");
    printerr("  <left> -- Left-align the text in this frame. Only works in some frame modes (eg \"hold\")\n");
    printerr("  <speedN> -- Set frame display speed. speed1 = slowest, speed6 = fastest.\n");
    printerr("  <br> -- Start of next display frame (allows multiple frames in one line).\n");
    printerr("  <blink>,</blink> -- Enable/disable blinking text. Only works in some frame modes (eg \"hold\").\n");
    printerr("  <small> -- Switch to a smaller font.\n");
    printerr("  <normal> -- Switch back to normal size.\n");
    printerr("  <wide>,</wide> -- Widen the text.\n");
    printerr("  <dblwide>,</dblwide> -- Widen the text more.\n");
    printerr("  <serif>,</serif> -- Switch to a serif font.\n");
    printerr("  <shadow>,</shadow> -- Apply a shadow to the text.\n");
    printerr("  <colorRGB> -- Change the text foreground color. R,G,B may each be any number between 0 and 3 (ex: \"color303\" for purple)\n");
    printerr("  <scolorRGB> -- Change the text shadow color (for text with <shadow> applied). Uses same RGB codes as <colorRGB>.\n");
    printerr("\n Some Special Character Entities (pg84-87):\n");
    printerr("  &uparrow; &downarrow; &leftarrow; &rightarrow;\n");
    printerr("  &cent; &gbp; &yen; &euro;\n");
    printerr("  &pacman; &boat; &ball; &phone;\n");
    printerr("  &heart; &car; &handicap; &rhino;\n");
    printerr("  &mug; &satdish; &copy; &male;\n");
    printerr("  &female; &bottle; &disk; &printer;\n");
    printerr("  &note; &infinity;\n");
}

int main(int argc, char* argv[]) {
    //Parse commandline for initflag/config:
    int do_init, error = -1;
    FILE* config;
    if (argc > 1) {
        if (strcmp(argv[1],"-i") == 0) {
            do_init = 1;
        } else if (strcmp(argv[1],"-u") == 0) {
            do_init = 0;
        } else {
            help(argv[0]);
            return -1;
        }

        if (argc > 2) {
            config = fopen(argv[2],"r");
            if (config == NULL) {
                printerr("Config file \"%s\" not found.\n",argv[2]);
                help(argv[0]);
                return -1;
            }
        } else {
            config = stdin;
        }
    } else {
        help(argv[0]);
        return -1;
    }

    //Get and parse bb_frames (both STRINGs and TEXTs) from config:
    struct bb_frame* startframe = NULL;
    if (parsecfg(&startframe,config) < 0) {
        fclose(config);
        help(argv[0]);
        goto end_noclose;
    }
    fclose(config);
    if (startframe == NULL) {
        printerr("Empty config file, nothing to do.\n");
        goto end_noclose;
    }


    libusb_device_handle* devh = NULL;
    if (bb_usb_init(&devh) < 0) {
        printerr("USB init failed: Exiting\n");
        goto end_noclose;
    }

    char* packet = NULL;
    int pktsize;

    //send sequence header before we start sending packets
    if (bb_sendraw(devh,(char*)sequence_header,
                   sizeof(sequence_header)) != sizeof(sequence_header)) {
        goto end;
    }

    if (do_init) {
        //this packet allocates sign memory for messages:
        if ((pktsize = bb_buildmemconfpacket(&packet,startframe)) < 0) {
            goto end;
        }
        if (bb_sendpkt(devh,packet,pktsize) != pktsize) {
            goto end;
        }
        free(packet);
        packet = NULL;
    }

    //now on to the real messages:
    struct bb_frame* curframe = startframe;
    while (curframe != NULL) {
#ifdef DEBUGMSG
        printf("result: data=%s\n",curframe->data);
#endif
        if (curframe->frame_type == STRING_FRAME_TYPE) {
            //data will be updated often, store in a STRING file
            pktsize = bb_buildstringpacket(&packet,curframe->filename,
                                           curframe->data);
        } else if (curframe->frame_type == TEXT_FRAME_TYPE) {
            if (!do_init) {
                curframe = curframe->next;
#ifdef DEBUGMSG
                printf(" ^-- SKIPPING: init-only packet\n");
#endif
                continue;
            }
            //data wont be updated often, use a TEXT file
            pktsize = bb_buildtextpacket(&packet,curframe->filename,
                                         curframe->mode,curframe->mode_special,
                                         curframe->data);
        } else {
            printerr("Internal error: Unknown frame type %d\n",curframe->frame_type);
            goto end;
        }

        if (bb_sendpkt(devh,packet,pktsize) != pktsize) {
            goto end;
        }
        free(packet);
        packet = NULL;

        curframe = curframe->next;
    }

    if (do_init) {
        //set display order for the messages:
        pktsize = bb_buildrunseqpacket(&packet,startframe);
        if (bb_sendpkt(devh,packet,pktsize) != pktsize) {
            goto end;
        }
        free(packet);
        packet = NULL;
    }

    //finish it off with a sequence footer
    if (bb_sendraw(devh,(char*)sequence_footer,
                   sizeof(sequence_footer)) != sizeof(sequence_footer)) {
        goto end;
    }

    error = 0;
 end:
    bb_usb_close(devh);
 end_noclose:
    if (startframe != NULL) {
        delchildren(startframe);
        free(startframe);
    }
    return error;
}
