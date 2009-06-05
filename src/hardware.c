/************************************************************************\

  bbusb - BetaBrite Prism LED Sign Communicator
  Hardware logic layer
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

#include "hardware.h"

//Found via lsusb while my Prism sign was plugged in:
#define SIGN_VENDOR_ID 0x8765
#define SIGN_PRODUCT_ID 0x1234
#define SIGN_INTERFACE_NUM 0
#define SIGN_ENDPOINT_NUM 2

//fixes warnings when being -pedantic:
struct timespec {
    time_t tv_sec;        // seconds
    long   tv_nsec;       // nanoseconds
};
extern int nanosleep(const struct timespec *req, struct timespec *rem);

int hardware_init(usbsign_handle** devhp) {
    if (devhp == NULL) {
        printerr("Internal error: Bad pointer to init\n");
        return -1;
    } else if (*devhp != NULL) {
        printerr("Internal error: Can't init twice\n");
        return -1;
    }

    return usbsign_open(SIGN_VENDOR_ID, SIGN_PRODUCT_ID,
                        SIGN_INTERFACE_NUM, devhp);
}

int hardware_close(usbsign_handle* devh) {
    if (devh != NULL) {
        usbsign_close(devh, SIGN_INTERFACE_NUM);
    }
    return 0;
}

static void sleep_ms(int ms) {
    struct timespec hundredms;
    hundredms.tv_sec = 0;
    hundredms.tv_nsec = ms*1000000;
    nanosleep(&hundredms, NULL);
}

//A sequence is a series of packets, and this is what begins and ends them (pg13)
const char sequence_header[] = {0,0,0,0,0,1,'Z','0','0'},
    sequence_footer[] = {4},
    packet_header[] = {2}, packet_footer[] = {3};

static int hardware_sendraw(usbsign_handle* devh, char* data, unsigned int size) {
    int sent;
    int ret = usbsign_send(devh, SIGN_ENDPOINT_NUM, data, size, &sent);

    if (ret < 0) {
        printerr("Got USB error %d when sending %d bytes\n", ret, size);
        return -1;
    } else {
        ret = sent;
    }

    if (ret > 0) {
#ifdef DEBUGMSG
        //print packet contents:
        unsigned int i;
        printf("%u: ",size);
        for (i = 0; i < size; i++) {
            printf("%X(%c) ", data[i], data[i]);
        }
        printf("\n");
#endif
    } else {
        printerr("Bad packet size %d or device %p\n",ret,(void*)devh);
        return -1;
    }

    return ret;
}

int hardware_seqstart(usbsign_handle* devh) {
    return (hardware_sendraw(devh,(char*)sequence_header,
                             sizeof(sequence_header)) == sizeof(sequence_header));
}

int hardware_seqend(usbsign_handle* devh) {
    return (hardware_sendraw(devh,(char*)sequence_footer,
                             sizeof(sequence_footer)) == sizeof(sequence_footer));
}

int hardware_sendpkt(usbsign_handle* devh, char* data, unsigned int size) {
    //create packet with header/footer chars added:
    char packet[size+sizeof(packet_footer)];
    memcpy(&packet[0],data,size);
    memcpy(&packet[size],packet_footer,sizeof(packet_footer));

    if (hardware_sendraw(devh,(char*)packet_header,
                   sizeof(packet_header)) != sizeof(packet_header)) {
        return -1;
    }
    sleep_ms(100);//"100 millisecond delay after the [pkt header]" (pg14)
    return hardware_sendraw(devh,packet,
                      size+sizeof(packet_footer))-sizeof(packet_footer);
}
