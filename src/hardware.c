/************************************************************************\

  bbusb - BetaBrite Prism LED Sign Communicator
  USB communication layer
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

//Found via lsusb while my sign was plugged in (yes this is real):
const int vendorid = 0x8765, productid = 0x1234;

//fixes warnings when being -pedantic:
struct timespec {
    time_t tv_sec;        /* seconds */
    long   tv_nsec;       /* nanoseconds */
};
extern int nanosleep(const struct timespec *req, struct timespec *rem);

int hardware_init(libusb_device_handle** devh) {
#ifdef NOUSB
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

int hardware_close(libusb_device_handle* devh) {
#ifdef NOUSB
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

static void printpkt(char* data, unsigned int size) {
    unsigned int i;
    printf("%u: ",size);
    for (i = 0; i < size; i++) {
        printf("%X(%c) ", data[i], data[i]);
    }
    printf("\n");
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

static int hardware_sendraw(libusb_device_handle* devh, char* data, unsigned int size) {
#ifdef NOUSB
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
        printpkt(data, size);
#endif
    } else {
        printerr("Bad packet size %d or device %p\n",ret,(void*)devh);
        return -1;
    }

    return ret;
}

int hardware_seqstart(libusb_device_handle* devh) {
    return (hardware_sendraw(devh,(char*)sequence_header,
                             sizeof(sequence_header)) == sizeof(sequence_header));
}

int hardware_seqend(libusb_device_handle* devh) {
    return (hardware_sendraw(devh,(char*)sequence_footer,
                             sizeof(sequence_footer)) == sizeof(sequence_footer));
}

int hardware_sendpkt(libusb_device_handle* devh, char* data, unsigned int size) {
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
