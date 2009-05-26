#!/usr/bin/python

allowedcolors=["002","020","200","011","101","110"]
allowedsets=['kids','love','wisdom','literature','science','humorists']
command = "fortune -s %s" % " ".join(allowedsets)

import subprocess,random,sys,os

sys.stdout.write("<color%s><speed1>" % random.choice(allowedcolors))
sys.stdout.flush()

proc = subprocess.Popen(command,shell=True,stdout=subprocess.PIPE)
proc.wait()
print " ".join(proc.stdout.read().split())
sys.exit(0)

