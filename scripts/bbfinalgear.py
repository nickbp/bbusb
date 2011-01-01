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

import httplib, os, os.path, random, sys, time, calendar, re
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

def timediff(time1_sec,time2_sec,shorten=True):
    if abs(int(time2_sec) - int(time1_sec)) < 1:
        return "Now"
    elif time2_sec > time1_sec:
        returnme = ""
        diff = time2_sec - time1_sec
    else:
        returnme = "-"
        diff = time1_sec - time2_sec
    min = 60
    hr = min * 60
    day = hr * 24
    year = day * 365
    startingorder = 0
    if diff > year:
        returnme += "%dy" % (diff / year)
        diff = diff % year
        if shorten:
            startingorder = 4
    if diff > day:
        returnme += "%dd" % (diff / day)
        diff = diff % day
        if not startingorder and shorten:
            startingorder = 3
    if diff > hr and startingorder < 4:
        returnme += "%dh" % (diff / hr)
        diff = diff % hr
        if not startingorder and shorten:
            startingorder = 2
    if diff > min and startingorder < 3:
        returnme += "%dm" % (diff / min)
        diff = diff % min
        if not startingorder and shorten:
            startingorder = 1
    if (int(diff) > 0 or len(returnme) <= 1) and startingorder < 2:
        returnme += "%ds" % diff
    return returnme

class FinalGearProcessor(SGMLParser):
    """Retrieves data from Final Gear. Creates two dictionaries with matching keysets: shownames and showtimes."""
    def reset(self):
        self.__shownames = {}
        self.__showtimes = {}
        self.__time_mode = False
        self.__time_id_temp = ""
        SGMLParser.reset(self)
    def start_meta(self, attrs):
        id = ""
        showname = ""
        for attr in attrs:
            if attr[0] == "scheme":
                id = attr[1]
            elif attr[0] == "content":
                searchme = ["The latest episode of (.+) finished airing ",
                            "An episode of (.+) is currently airing!"]
                for searchtext in searchme:
                    match = re.search(searchtext,attr[1])
                    if match:
                        showname = match.groups()[0]
                        break
        if showname and id:
            self.__shownames[id] = showname
    def end_meta(self):
        pass #required funct
    def start_span(self,attrs):
        self.__time_mode = True
        for attr in attrs:
            if attr[0] == "id":
                self.__time_id_temp = attr[1]
                break
    def end_span(self):
        self.__time_id_temp = ""
        self.__time_mode = False
    def handle_data(self, data):
        if self.__time_mode and self.__time_id_temp:
            format = "%Y-%m-%d %H:%M:%S GMT+00:00"
            numsecs = calendar.timegm(time.strptime(data,format))
            self.__showtimes[self.__time_id_temp] = numsecs
            return

        match = re.search("An episode of (.+) is currently airing!",data)
        if match:
            #find id by showname, set time to now:
            showname = match.groups()[0]
            for (id,itername) in self.__shownames.iteritems():
                if showname == itername:
                    self.__showtimes[id] = "Now!"
                    return
        match = re.search("(.+) is currently on break.* in (.+)?[^\w]",data)
        if match:
            #get the time and the show name:
            showname = match.groups()[0]
            #just need an arbitrary matching key across the two dicts:
            self.__shownames[showname] = showname
            self.__showtimes[showname] = match.groups()[1].capitalize()
            return
    def get_episodes(self):
        returnme = []
        curtime = time.time()
        for (id,name) in self.__shownames.iteritems():
            if self.__showtimes.has_key(id):
                returnme.append((name,self.__showtimes[id]))
        return returnme

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

processor = FinalGearProcessor()
processor.feed(data)
processor.close()

showdata = processor.get_episodes()

headlinecolors=["102","120","210","021","201","210"]
textcolors =   ["002","020","200","011","101","110"]
if not len(headlinecolors) == len(textcolors):
    raise Exception("length of headlinecolors must match length of textcolors")
randindex = random.randint(0, len(headlinecolors)-1)

headlinecolor = headlinecolors[randindex]
textcolor = textcolors[randindex]

if showdata:
    ret = []
    curtime = time.time()
    for (name,showtimeraw) in showdata:
        if type(showtimeraw) == int:
            showtime = timediff(curtime,showtimeraw)
        else:
            showtime = showtimeraw
        ret.append("<color%s><scolor%s><shadow>%s</shadow>: <color%s>%s" % \
                       (headlinecolor, textcolor, name, textcolor, showtime))
    print ", ".join(ret)
else:
    print "No Final Gear Countdown data found."
