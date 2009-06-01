/************************************************************************\

  bbusb - BetaBrite Prism LED Sign Communicator
  Config file parsing
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

#include "config.h"

//fixes warnings when being -pedantic:
extern FILE* popen(const char* command, const char* modes);
extern int pclose(FILE *stream);
extern char *strtok_r(char *str, const char *delim, char **saveptr);

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
