#!/usr/bin/python

import time, httplib, os, os.path, sys
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
    """Retrieves data from grouphug.us. Returns quotes in a dictionary, where the keys are idnums."""
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
today = time.gmtime().tm_yday

divider = ", "
for (id,name) in processor.shownames.iteritems():
    if processor.showtimes.has_key(id):
        time_s = processor.showtimes[id]
        timef = time.strptime(time_s,format)
        daysleft = timef.tm_yday - today
        if daysleft == 1:
            s = ""
        else:
            s = "s"
        accu += "Next %s: <wide>%d day%s</wide>%s" % (name,daysleft,s,divider)
if accu:
    accu = accu[:-len(divider)]
    print accu
else:
    print "No Final Gear Countdown data found."
