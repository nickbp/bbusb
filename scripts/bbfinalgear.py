#!/usr/bin/python

'''
  bbfinalgear.py - Retrieves countdown data from FinalGear.com
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

#avoid too-bright and too-dim colors:
allowedcolors=["002","020","200","011","101","110"]

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
    """Retrieves data from Final Gear. Creates two dictionaries with matching keysets: shownames and showtimes."""
    def reset(self):
        self.shownames = {}

        self.showtimes = {}
        self.time_mode = False
        self.time_id_temp = ""
        SGMLParser.reset(self)

    def start_meta(self, attrs):
        id = ""
        showname = ""
        for attr in attrs:
            if attr[0] == "scheme":
                id = attr[1]
            elif attr[0] == "content":
                head = "An episode of "
                foot = " is currently airing!"
                if (attr[1].startswith(head) and attr[1].endswith(foot)):
                    showname = attr[1][len(head):-len(foot)]
        if showname and id:
            self.shownames[id] = showname

    def end_meta(self):
        pass #required funct

    def start_span(self,attrs):
        self.time_mode = True
        for attr in attrs:
            if attr[0] == "id":
                self.time_id_temp = attr[1]
                break

    def end_span(self):
        self.time_id_temp = ""
        self.time_mode = False

    def handle_data(self, data):
        if self.time_mode and self.time_id_temp:
            self.showtimes[self.time_id_temp] = data

data = get_cached('www.finalgear.com','/_countdown_ajax.php',86400)

processor = HTMLProcessor()
processor.feed(data)
processor.close()

accu = ""
format = "%Y-%m-%d %H:%M:%S GMT+00:00"

divider = ", "
for (id,name) in processor.shownames.iteritems():
    if processor.showtimes.has_key(id):
        time_s = processor.showtimes[id]
        showsecs = calendar.timegm(time.strptime(time_s,format))
        cursecs = time.time()

        timediff = showsecs - cursecs
        hoursleft = timediff / 3600
        daysleft = hoursleft / 24

	if daysleft > -1 and daysleft < 1:
            timeleft = "%d hours" % hoursleft
        elif daysleft >= 1 and daysleft < 2:
            timeleft = "%d day" % daysleft
        else:
            timeleft = "%d days" % daysleft
        accu += "Next %s: <wide>%s</wide>%s" % (name,timeleft,divider)

sys.stdout.write("<color%s>" % random.choice(allowedcolors))
sys.stdout.flush()

if accu:
    #strip last divider before printing
    print accu[:-len(divider)]
else:
    print "No Final Gear Countdown data found."
