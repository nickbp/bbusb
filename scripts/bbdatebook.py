#!/usr/bin/python

'''
  bbdatebook.py - Retrieves the Marketplace(.org) datebook
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
'''

import httplib, os, os.path, random, sys, time, calendar
from sgmllib import SGMLParser

def get_cached(host,hostpath,cachetimeout):
    data = ""
    cachename = sys.argv[0]+".cache"
    if (not os.path.exists(cachename) or \
            time.time() - os.stat(cachename).st_mtime > cachetimeout):
        conn = httplib.HTTPConnection(host)
        conn.request('GET',hostpath)
        resp = conn.getresponse()
        if resp.status == 200:
            data = resp.read()
            cachefile = file(cachename,'w')
            cachefile.write(data)
            cachefile.close()
        else:
            sys.stderr.write("Warning: Got status %s (%s) when retrieving %s%s" % \
                (resp.status,resp.reason,host,hostpath))

    if data == "":
        data = file(cachename).read()

    return data

class HTMLProcessor(SGMLParser):
    """Retrieves Datebook from Marketplace page. Saves the date for the datebook and the list of datebook items.
    Expected content: <div id=datebook><h3>Marketplace datebook for Wednesday, October 21, 2009</h3><ul><li>Item1</li><li>Item2</li><li>...</li></ul></div>"""
    def reset(self):
        self.title = ""
        self.items = []

        #internal flags:
        self._in_datebook = False
        self._in_datetitle = False
        self._in_dateitem = False

        SGMLParser.reset(self)

    def start_div(self, attrs):
        for attr in attrs:
            if attr[0] == "id" and attr[1] == "datebook":
                self._in_datebook = True
                break

    def end_div(self):
        if self._in_datebook:
            self._in_datebook = False

    def start_h3(self, attrs):
        if self._in_datebook:
            self._in_datetitle = True

    def end_h3(self):
        if self._in_datebook:
            self._in_datetitle = False

    def start_li(self, attrs):
        if self._in_datebook:
            self._in_dateitem = True

    def end_li(self):
        if self._in_datebook:
            self._in_dateitem = False

    def handle_data(self, data):
        if self._in_datetitle:
            self.title = data
        elif self._in_dateitem:
            self.items.append(data)

data = get_cached('marketplace.publicradio.org','/episodes/show_rundown.php?show_id=14',10800)

processor = HTMLProcessor()
processor.feed(data)
processor.close()

headlinecolors=["102","120","210","021","201","210"]
textcolors =   ["002","020","200","011","101","110"]
if not len(headlinecolors) == len(textcolors):
    raise Exception("length of headlinecolors must match length of textcolors")
randindex = random.randint(0, len(headlinecolors)-1)

headlinecolor = headlinecolors[randindex]
textcolor = textcolors[randindex]

entrytitle = processor.title
entrycontent = []
for index in range(0,len(processor.items)):
    data = processor.items[index]
    if data.lower().startswith("and "):
        data = data[4:]
    entrycontent.append(data[0].upper() + data[1:])

#avoid crashing on utf characters:
bullet = " &rightarrow; "
if len(entrycontent) > 0:
    out = "<color%s><scolor%s><shadow>%s</shadow><color%s>%s" % \
        (headlinecolor, textcolor, entrytitle, textcolor, bullet + bullet.join(entrycontent))
    print unicode(out).encode("utf-8")
