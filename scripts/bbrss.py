#!/usr/bin/python
# -*- coding: utf-8 -*-

'''
  bbrss.py - Prints a single random article from a specified RSS feed
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

paths = {
    'reuters':'http://feeds.reuters.com/reuters/topNews?format=xml',
    'cnn':'http://rss.cnn.com/rss/cnn_topstories.rss',
    'msnbc':'http://rss.msnbc.msn.com/id/3032091/device/rss/rss.xml'
}

import feedparser, cPickle, sys, os.path, time, re, urlparse, random

def get_feed_cached(webpath,cachekey,cachetimeout):
    cachename = sys.argv[0]+".cache."+cachekey
    if (not os.path.exists(cachename) or \
            time.time() - os.stat(cachename).st_mtime > cachetimeout):
        feed = feedparser.parse(webpath)
        if feed.has_key('bozo_exception'):
            sys.stderr.write("Unable to retrieve '%s': " % webpath)
            if feed.has_key('bozo_exception'):
                sys.stderr.write(str(feed['bozo_exception'])+"\n")
            else:
                sys.stderr.write("Unknown error.\n")
            sys.exit(1)
        cPickle.dump(feed,file(cachename,'w'))
        return feed
    else:
        return cPickle.load(file(cachename))

def help(myname):
    sys.stderr.write("""Syntax: %s <source/rssurl>
Sources: %s (Or use your own custom url)\n""" % \
                         (myname,", ".join(paths.keys())))
    sys.exit(1)

if len(sys.argv) < 2:
    help(sys.argv[0])

if paths.has_key(sys.argv[1]):
    #predefined feed from dict
    feed = get_feed_cached(paths[sys.argv[1]], sys.argv[1], 1800)
else:
    #custom url
    urlobj = urlparse.urlparse(sys.argv[1])
    if not urlobj.netloc:
        #assume bad url
        print "Bad url: %s" % sys.argv[1]
        help(sys.argv[0])
    feed = get_feed_cached(sys.argv[1], urlobj.netloc, 1800)

if not feed or not feed['feed']:
    sys.stderr.write("Invalid feed content: %s\n" % feed)
    sys.exit(1)

striphtmltags = re.compile(r'<[^>]*?>')
entry = random.choice(feed['entries'])

#pick from list of preferred colors (with matching between shadow/text):

#avoid too-bright and too-dim colors:
headlinecolors=["102","120","210","021","201","210"]
textcolors =   ["002","020","200","011","101","110"]
if not len(headlinecolors) == len(textcolors):
    raise Exception("length of headlinecolors must match length of textcolors")
randindex = random.randint(0, len(headlinecolors)-1)

headlinecolor = headlinecolors[randindex]
textcolor = textcolors[randindex]

#if paths.has_key(sys.argv[1]):
#    sitetitle = sys.argv[1].upper()
#else:
#    sitetitle = feed['feed']['title']
entrytitle = entry['title']
entrycontent = striphtmltags.sub('', entry['description']).strip()

#separate print avoids crashing if weird chars encountered (dunno why?)
out = "<color%s><scolor%s><shadow>%s</shadow>: <color%s>%s" % \
    (headlinecolor, textcolor, entrytitle, textcolor, entrycontent)
print out
