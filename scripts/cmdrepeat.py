#!/usr/bin/python

'''
  cmdrepeat.py - Runs a command every few minutes, useful for updating
                 a sign periodically without needing a cron job.
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


import time, os, sys, subprocess

timeformat = "%T"

def help(myname):
    print "%s <delay (minutes)> <command arg arg ...>" % myname

if (len(sys.argv) > 2):
    try:
        delay = int(sys.argv[1])
    except:
        help(sys.argv[0])
        sys.exit(1)
    cmd = sys.argv[2:]
else:
    help(sys.argv[0])
    sys.exit(1)

print "Running \"%s\" every %s minutes, starting NOW" % (" ".join(cmd),delay)

while (True):
    print "\n%s Executing %s" % (time.strftime(timeformat)," ".join(cmd))
    try:
        ret = subprocess.call(cmd)
    except:
        print "Got exception: %s" % sys.exc_info()[1]
        print "Aborting!"
        sys.exit(2)
    if ret == 0:
        print "%s Command successful (return code %s)" % (time.strftime(timeformat),ret)
    else:
        print "%s Command failed (return code %s)" % (time.strftime(timeformat),ret)
        break
    print "%s Waiting %s minutes..." % (time.strftime(timeformat),delay)
    time.sleep(60*delay)
