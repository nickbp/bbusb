#!/usr/bin/python

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
        


