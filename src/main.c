/************************************************************************\

  bbusb - BetaBrite Prism LED Sign Communicator
  Commandline parsing and main
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

#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "config.h"
#include "usbsign.h"
#include "packet.h"
#include "hardware.h"

static void version(void) {
    config_error("bbusb %s (%s)",VERSION_STRING,USB_TYPE);
    config_error("Copyright (C) 2009-2011 Nicholas Parker <nickbp@gmail.com>");
    config_error("License GPLv3+: GNU GPL version 3 or later");
    config_error("This program comes with ABSOLUTELY NO WARRANTY.");
}

static void help(char* appname) {
    version();
    config_error("");
    config_error("Usage: %s <mode> [options] [configfile]", appname);
    config_error("");
    config_error("Modes:");
    config_error("  -i/--init:\tre-initialize the sign (required when configfile changes)");
    config_error("  -u/--update:\tupdate the sign contents without init (smoother than --init)");
    config_error("");
    config_error("Options:");
    config_error("  configfile:\tPath to a Config File (see syntax below).");
    config_error("             \tIf configfile is unspecified, stdin will be used.");
    config_error("  -h/--help        This help text.");
    config_error("  -v/--verbose     Show verbose output.");
    config_error("  --log <file>     Append any output to <file>.");
    config_error("");
    config_error("Config File Syntax:");
    config_error("  #comment");
    config_error("  //comment");
    config_error("  txt <mode> [text (optional if mode=nX)]");
    config_error("  cmd <mode> <shell command>");
    config_error("");
    config_error("Available Mode Codes (spec pg89-90)");
    config_error("  Note: Some \"nX\" modes don't work for \"cmd\" commands.");
    config_error("        Get around this by prefixing with an empty \"txt\" (ex: txt nx\\ncmd ...)");
    config_error("  a rotate \tp rollin \tn8 welcome");
    config_error("  b hold \tq rollout \tn9 sale");
    config_error("  c flash \tr wipein \tna newsflash");
    config_error("  e rollup \ts wipeout \tnb happy4th");
    config_error("  f rolldn \tt comprotate \tnc cyclecolor");
    config_error("  g rolleft \tn0 twinkle \tns thankyou");
    config_error("  h rollright \tn1 sparkle \tnu nosmoking");
    config_error("  i wipeup \tn2 snow \tnv dontdrink");
    config_error("  j wipedn \tn3 interlock \tnw fish");
    config_error("  k wipeleft \tn4 switch \tnx fireworks");
    config_error("  l wiperight \tn5 cyclecolor \tny xmas");
    config_error("  m scroll \tn6 spray \tnz smile");
    config_error("  o automode \tn7 starburst");
    config_error("");
    config_error("Inline Text Format Syntax (spec pg81-82):");
    config_error("  <left> -- Left-align the text in this frame.");
    config_error("            Only works in some frame modes (eg \"hold\")");
    config_error("  <speedN> -- Set frame display speed. speed1 = slowest, speed6 = fastest.");
    config_error("  <br> -- Start of next display frame (allows multiple frames in one command).");
    config_error("  <blink>,</blink> -- Enable/disable blinking text.");
    config_error("                      Only works in some frame modes (eg \"hold\").");
    config_error("  <small> -- Switch to a smaller font.");
    config_error("  <normal> -- Switch back to normal size.");
    config_error("  <wide>,</wide> -- Widen the text.");
    config_error("  <dblwide>,</dblwide> -- Widen the text more.");
    config_error("  <serif>,</serif> -- Switch to a serif font.");
    config_error("  <shadow>,</shadow> -- Apply a shadow to the text.");
    config_error("  <colorRGB> -- Change the text foreground color. R,G,B may each be any number between 0 and 3");
    config_error("                Examples: <color303> for bright purple, <color101> for dim purple.");
    config_error("  <scolorRGB> -- Change the text shadow color (for text with <shadow> applied).");
    config_error("                 Uses same RGB codes as <colorRGB>.");
    config_error("");
    config_error("Some Special Character Entities (pg84-87):");
    config_error("  &uparrow; &downarrow; &leftarrow; &rightarrow;");
    config_error("  &cent; &gbp; &yen; &euro;");
    config_error("  &disk; &printer; &phone; &satdish;");
    config_error("  &car; &boat; &male; &female;");
    config_error("  &heart; &pacman; &ball; &note;");
    config_error("  &mug; &bottle; &handicap; &copy;");
    config_error("  &rhino; &infinity;");
    config_error("");
}

static void mini_help(char* appname) {
    config_error("Run \"%s -h\" for help.",appname);
}

int main(int argc, char* argv[]) {
    config_fout = stdout;
    config_ferr = stderr;
    if (argc == 1) {
        help(argv[0]);
        return -1;
    }

    int mode_specified = 0, do_init = 0;
    char* configpath = NULL;
    FILE* configfile;

    int c;
    while (1) {
        static struct option long_options[] = {
            {"help", 0, NULL, 'h'},
            {"verbose", 0, NULL, 'v'},
            {"log", required_argument, NULL, 'l'},
            {"init", 0, NULL, 'i'},
            {"update", 0, NULL, 'u'},
            {0,0,0,0}
        };

        int option_index = 0;
        c = getopt_long(argc, argv, "hvl:iu",
                long_options, &option_index);
        if (c == -1) {//unknown arg (doesnt match -x/--x format)
            if (optind >= argc) {
                //at end of successful parse
                break;
            }
            //getopt refuses to continue, so handle infile manually:
            int i = optind;
            for (; i < argc; ++i) {
                configpath = argv[i];
                //debug("%d %d %s", argc, i, arg);
                configfile = fopen(configpath,"r");
                if (configfile == NULL) {
                    config_error("Unable to open config file %s: %s", configpath, strerror(errno));
                    mini_help(argv[0]);
                    return -1;
                }
            }
            break;
        }

        switch (c) {
        case 'h':
            help(argv[0]);
            return -1;
        case 'v':
            config_debug_enabled = 1;
            break;
        case 'l':
            {
                FILE* logfile = fopen(optarg, "a");
                if (logfile == NULL) {
                    config_error("Unable to open log file %s: %s", optarg, strerror(errno));
                    return -1;
                }
                config_fout = logfile;
                config_ferr = logfile;
            }
            break;
        case 'i':
            mode_specified = 1;
            do_init = 1;
            break;
        case 'u':
            mode_specified = 1;
            do_init = 0;
            break;
        default:
            mini_help(argv[0]);
            return -1;
        }
    }
    if (!mode_specified) {
        config_error("-i/-u mode argument required.");
        mini_help(argv[0]);
    }

    int error = -1;
    if (configpath == NULL) {
        configpath = "<stdin>";
        configfile = stdin;
    }
    config_log("Parsing %s",configpath);

    //Get and parse bb_frames (both STRINGs and TEXTs) from config:
    struct bb_frame* startframe = NULL;
    if (parsefile(&startframe,configfile) < 0) {
        fclose(configfile);
        config_error("Error encountered when parsing config file. ");
        mini_help(argv[0]);
        goto end_noclose;
    }
    fclose(configfile);
    if (startframe == NULL) {
        config_error("Empty config file, nothing to do. ");
        mini_help(argv[0]);
        goto end_noclose;
    }


    usbsign_handle* devh = NULL;
    if (hardware_init(&devh) < 0) {
        config_error("USB init failed: Exiting. ");
        mini_help(argv[0]);
        goto end_noclose;
    }

    char* packet = NULL;
    int pktsize;

    config_log("Writing to sign");

    //send sequence header before we start sending packets
    if (!hardware_seqstart(devh)) {
        //try resetting device once
        config_error("Initial write failed, attempting reset.");
        if (!hardware_reset(&devh)) {
            config_error("Reset failed: Exiting.");
            goto end;
        }
        if (!hardware_seqstart(devh)) {
            config_error("Initial write retry failed, giving up.");
            goto end;
        }
        config_log("Reset successful, continuing");
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
        config_debug("result: data=%s",curframe->data);
        if (curframe->frame_type == STRING_FRAME_TYPE) {
            //data will be updated often, store in a STRING file
            pktsize = packet_buildstring(&packet,curframe->filename,
                                           curframe->data);
        } else if (curframe->frame_type == TEXT_FRAME_TYPE) {
            if (!do_init) {
                curframe = curframe->next;
                config_debug(" ^-- SKIPPING: init-only packet");
                continue;
            }
            //data wont be updated often, use a TEXT file
            pktsize = packet_buildtext(&packet,curframe->filename,
                                         curframe->mode,curframe->mode_special,
                                         curframe->data);
        } else {
            config_error("Internal error: Unknown frame type %d",curframe->frame_type);
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
