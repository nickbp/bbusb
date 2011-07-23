/************************************************************************\

  bbusb - BetaBrite Prism LED Sign Communicator
  libusb-1.0 implementation
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
    int ret = libusb_init(NULL);
    if (ret < 0) {
        config_error("Got error %d when initializing usb stack", ret);
        return ret;
    }

    *dev = libusb_open_device_with_vid_pid(NULL, vendorid, productid);
    if (*dev == NULL) {
        config_error("Could not find/open USB device with vid=0x%X pid=0x%X. Is the sign plugged in?",
                vendorid, productid);
        return -1;
    }

    ret = libusb_claim_interface(*dev, interface);
    if (ret < 0) {
        config_error("Could not claim device (%d)", ret);
        return ret;
    }
    return 0;
}

int usbsign_reset(int vendorid, int productid,
                  int interface, usbsign_handle** dev) {
    int ret = libusb_reset_device(*dev);
    if (ret == LIBUSB_ERROR_NOT_FOUND) {
        //need to close/reopen the device
        usbsign_close(*dev, interface);
        return usbsign_open(vendorid, productid, interface, dev);
    } else if (ret < 0) {
        config_error("Got error %d when resetting usb device", ret);
    }
    return ret;
}

void usbsign_close(usbsign_handle* dev, int interface) {
    libusb_release_interface(dev, interface);
    libusb_close(dev);
    dev = NULL;
    libusb_exit(NULL);
}

int usbsign_send(usbsign_handle* dev, int endpoint,
                 char* data, unsigned int size, int* sentcount) {
    if (dev == NULL) {
        config_error("Unable to send: Device handle is null");
        return -1;
    }

    return libusb_bulk_transfer(dev, (endpoint | LIBUSB_ENDPOINT_OUT),
                                (unsigned char*)data, size,
                                sentcount, 1000);
}
