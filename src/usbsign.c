/************************************************************************\

  bbusb - BetaBrite Prism LED Sign Communicator
  libusb abstraction layer
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

#include "usbsign.h"

int usbsign_open(int vendorid, int productid,
                 int interface, usbsign_handle** dev) {
#ifdef NOUSB
    printf("USB Open %X:%X %p:%d\n",vendorid,productid,(void*)dev,interface);
    return 0;
#else

#ifdef OLDUSB
    usb_init();
    usb_find_busses();
    usb_find_devices();

    struct usb_bus* bus;
    struct usb_device* usbdev;
    int found = 0;

    for (bus = usb_get_busses(); bus; bus = bus->next) {
        //printf("BUS: %s\n",bus->dirname);
        for (usbdev = bus->devices; usbdev; usbdev = usbdev->next) {
            struct usb_device_descriptor *desc = &(usbdev->descriptor);
            //printf("    Device: %s\n", usbdev->filename);
            //printf("        %x:%x\n",desc->idVendor,desc->idProduct);
            if (usbdev->descriptor.idVendor == vendorid &&
                usbdev->descriptor.idProduct == productid) {
                found = 1;
                break;
            }
        }
        if (found > 0) {
            break;
        }
    }
    if (found > 0) {
        *dev = usb_open(usbdev);
    } else {
        *dev = NULL;
    }
#else
    int ret = libusb_init(NULL);
    if (ret < 0) {
        printerr("Got error %d when initializing usb stack\n", ret);
        return ret;
    }

    *dev = libusb_open_device_with_vid_pid(NULL, vendorid, productid);
#endif
    if (*dev == NULL) {
        printerr("Could not find/open USB device with vid=0x%X pid=0x%X. Is the sign plugged in?\n",
                vendorid, productid);
        return -1;
    }

#ifdef OLDUSB
    int ret = usb_claim_interface(*dev, interface);
#else
    ret = libusb_claim_interface(*dev, interface);
#endif
    if (ret < 0) {
        printerr("Could not claim device (%d)\n", ret);
        return ret;
    }
    return 0;

#endif
}

void usbsign_close(usbsign_handle* dev, int interface) {
#ifdef NOUSB
    printf("USB Close %p:%d\n",dev, interface);
#else
#ifdef OLDUSB
    usb_release_interface(dev, interface);
    usb_close(dev);
    dev = NULL;
#else
    libusb_release_interface(dev, interface);
    libusb_close(dev);
    dev = NULL;
    libusb_exit(NULL);
#endif
#endif
}

int usbsign_send(usbsign_handle* dev, int endpoint,
                 char* data, unsigned int size, int* sentcount) {
#ifdef NOUSB
    *sentcount = size;
    printf("USB Send %d bytes of data *%p to device *%p:%d\n",
           size,data,dev,endpoint);
    return 0;
#else
    if (dev == NULL) {
        printerr("Unable to send: Device handle is null\n");
        return -1;
    }
#ifdef OLDUSB
    int ret = usb_bulk_write(dev, endpoint, data, size, 1000);
    if (ret >= 0) {
        *sentcount = ret;
        return 0;
    } else {
        return ret;
    }   
#else
    return libusb_bulk_transfer(dev, (endpoint | LIBUSB_ENDPOINT_OUT),
                                (unsigned char*)data, size,
                                sentcount, 1000);
#endif
#endif
}
