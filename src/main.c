/************************************************************************\

  bbusb - BetaBrite Prism LED Sign Communicator
  Commandline parsing and main
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

#include "main.h"

static void help(char* appname) {
    printerr("Usage: %s <-i/-u> [configfile]\n",appname);
    printerr("\n  -i:\tre-initialize the sign (required when config file is new or modified since last update)\n");
    printerr("  -u:\tupdate the sign contents without init (avoids display interruption)\n");
    printerr("  configfile:\tPath to a config file (see syntax below).\n");
    printerr("             \tIf configfile is unspecified, the configuration will be read from stdin.\n");
    printerr("\n Config Syntax:\n");
    printerr("  #comment\n");
    printerr("  //comment\n");
    printerr("  txt <mode> [text (optional if mode=nX)]\n");
    printerr("  cmd <mode> <shell command>\n");
    printerr("\n Available Mode Codes (pg89-90)\n");
    printerr(" Note: Some \"nX\" modes don't work for \"cmd\"s.\n");
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

static void version(void) {
    printerr("bbusb %s (%s)\n",VERSION,USBTYPE);
    printerr("Copyright (C) 2009 Nicholas Parker <nickbp@gmail.com>\n");
    printerr("License GPLv3+: GNU GPL version 3 or later\n");
    printerr("This program comes with ABSOLUTELY NO WARRANTY.\n");
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
        } else if (strcmp(argv[1],"-v") == 0 ||
                   strcmp(argv[1],"--version") == 0) {
            version();
            return -1;
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


    usbsign_handle* devh = NULL;
    if (hardware_init(&devh) < 0) {
        printerr("USB init failed: Exiting\n");
        goto end_noclose;
    }

    char* packet = NULL;
    int pktsize;

    //send sequence header before we start sending packets
    if (!hardware_seqstart(devh)) {
        goto end;
    }

    if (do_init) {
        //this packet allocates sign memory for messages:
        if ((pktsize = packet_buildmemconf(&packet,startframe)) < 0) {
            goto end;
        }
        if (hardware_sendpkt(devh,packet,pktsize) != pktsize) {
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
            pktsize = packet_buildstring(&packet,curframe->filename,
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
            pktsize = packet_buildtext(&packet,curframe->filename,
                                         curframe->mode,curframe->mode_special,
                                         curframe->data);
        } else {
            printerr("Internal error: Unknown frame type %d\n",curframe->frame_type);
            goto end;
        }

        if (hardware_sendpkt(devh,packet,pktsize) != pktsize) {
            goto end;
        }
        free(packet);
        packet = NULL;

        curframe = curframe->next;
    }

    if (do_init) {
        //set display order for the messages:
        pktsize = packet_buildrunseq(&packet,startframe);
        if (hardware_sendpkt(devh,packet,pktsize) != pktsize) {
            goto end;
        }
        free(packet);
        packet = NULL;
    }

    //finish it off with a sequence footer
    if (!hardware_seqend(devh)) {
        goto end;
    }

    error = 0;
 end:
    hardware_close(devh);
 end_noclose:
    if (startframe != NULL) {
        packet_delete(startframe);
        free(startframe);
    }
    return error;
}
