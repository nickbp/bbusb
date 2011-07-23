/************************************************************************\

  bbusb - BetaBrite Prism LED Sign Communicator
  libusb-0.1 implementation
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
    usb_init();
    usb_find_busses();
    usb_find_devices();

    struct usb_bus* bus;
    struct usb_device* usbdev;
    int found = 0;

    for (bus = usb_get_busses(); bus; bus = bus->next) {
        config_debug("BUS: %s",bus->dirname);
        for (usbdev = bus->devices; usbdev; usbdev = usbdev->next) {
            struct usb_device_descriptor *desc = &(usbdev->descriptor);
            config_debug("    Device: %s", usbdev->filename);
            config_debug("        %x:%x",desc->idVendor,desc->idProduct);
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

    if (*dev == NULL) {
        config_error("Could not find/open USB device with vid=0x%X pid=0x%X. Is the sign plugged in?",
                vendorid, productid);
        return -1;
    }

    int ret = usb_claim_interface(*dev, interface);
    if (ret < 0) {
        config_error("Could not claim device (%d)", ret);
        return ret;
    }
    return 0;
}

int usbsign_reset(int vendorid, int productid,
                  int interface, usbsign_handle** dev) {
    int ret = usb_reset(*dev);
    if (ret < 0) {
        config_error("Got error %d when resetting usb device", ret);
        return ret;
    }
    return usbsign_open(vendorid, productid, interface, dev);
}

void usbsign_close(usbsign_handle* dev, int interface) {
    usb_release_interface(dev, interface);
    usb_close(dev);
    dev = NULL;
}

int usbsign_send(usbsign_handle* dev, int endpoint,
                 char* data, unsigned int size, int* sentcount) {
    if (dev == NULL) {
        config_error("Unable to send: Device handle is null");
        return -1;
    }

    int ret = usb_bulk_write(dev, endpoint, data, size, 1000);
    if (ret >= 0) {
        *sentcount = ret;
        return 0;
    } else {
        return ret;
    }
}
