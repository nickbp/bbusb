#!/usr/bin/python

'''
  bbcc.py - Prints a random Charlie Chan saying from cc.txt.
  Charlie Chan quotes were originally retrieved from:
      http://charliechanfamily.tripod.com/id6.html
  This idea is ripped from ribo's ".cc" command from #linux on GameSurge
  
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

import random, sys, os.path

quotefilename = "cclist.txt"
quotefile = os.path.join(os.path.dirname(sys.argv[0]),quotefilename)

try:
    lines = open(quotefile).readlines()
except:
    sys.stderr.write("Error when opening %s. Unable to retrieve Charlie Chan quote.\n" % quotefile)
    sys.exit(1)

#pick from list of preferred colors (with matching between shadow/text):
headlinecolors=["102","120","210","021","201","210"]
textcolors =   ["002","020","200","011","101","110"]
if not len(headlinecolors) == len(textcolors):
    raise Exception("length of headlinecolors must match length of textcolors")
randindex = random.randint(0, len(headlinecolors)-1)

headlinecolor = headlinecolors[randindex]
textcolor = textcolors[randindex]

message = random.choice(lines).strip()

print "<color%s>Charlie Chan say, <color%s>%s" % \
    (headlinecolor, textcolor, message)
