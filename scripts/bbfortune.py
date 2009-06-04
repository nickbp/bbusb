#!/usr/bin/python

'''
  bbfortune.py - Prints random fortunes (via fortune_mod)
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

#safe for work (CHANGE AS DESIRED):
allowedsets=['kids','love','wisdom','literature','science','humorists']
ommand = "fortune -s %s" % " ".join(allowedsets)

#avoid too-bright and too-dim colors:
allowedcolors=["002","020","200","011","101","110"]

import subprocess,random,sys,os

sys.stdout.write("<color%s>" % random.choice(allowedcolors))
sys.stdout.flush()

proc = subprocess.Popen(command,shell=True,stdout=subprocess.PIPE)
proc.wait()
print " ".join(proc.stdout.read().split())
sys.exit(0)

