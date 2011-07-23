/************************************************************************\

  bbusb - BetaBrite Prism LED Sign Communicator
  printf implementation
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
    config_log("USB Open %X:%X %p:%d",vendorid,productid,(void*)dev,interface);
    return 0;
}

int usbsign_reset(int vendorid, int productid,
                  int interface, usbsign_handle** dev) {
    config_log("USB Reset %X:%X %p:%d",vendorid,productid,(void*)dev,interface);
    return 0;
}

void usbsign_close(usbsign_handle* dev, int interface) {
    config_log("USB Close %p:%d",dev, interface);
}

int usbsign_send(usbsign_handle* dev, int endpoint,
                 char* data, unsigned int size, int* sentcount) {
    *sentcount = size;
    config_log("USB Send %d bytes of data *%p to device *%p:%d",
           size,data,dev,endpoint);
    return 0;
}
