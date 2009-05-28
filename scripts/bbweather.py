#!/usr/bin/python

'''
  bbweather.py - Retrieves forecasts from weather.gov's soap service
  (See http://www.weather.gov/forecasts/xml/SOAP_server/ndfdXMLserver.php)

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

import httplib, os, os.path, time, sys
from xml.dom import minidom

def help():
    print "Syntax: %s <zipcode>" % sys.argv[0]
    sys.exit(1)

try:
    zipcode = sys.argv[1]
except:
    help()

#number of days to forecast (incl today):
numdays = 4

#####

def getXmlData(soapaction, soapreqstring, 
               cachefilename, cachetimeout=0):
    host = "www.weather.gov"
    path = "/forecasts/xml/SOAP_server/ndfdXMLserver.php"
    headers = {
        'SOAPAction':'"http://www.weather.gov/forecasts/xml/DWMLgen/wsdl/ndfdXML.wsdl#%s"' % soapaction,
        'Content-Type':'text/xml; charset=utf-8'
        }
    soapreqheader = '''<SOAP-ENV:Envelope xmlns:SOAP-ENC="http://schemas.xmlsoap.org/soap/encoding/"
xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/"
xmlns:ZSI="http://www.zolera.com/schemas/ZSI/"
xmlns:xsd="http://www.w3.org/2001/XMLSchema"
xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
SOAP-ENV:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
<SOAP-ENV:Header></SOAP-ENV:Header>
<SOAP-ENV:Body xmlns:ns1="http://www.weather.gov/forecasts/xml/DWMLgen/wsdl/ndfdXML.wsdl">'''
    soapreqfooter = '</SOAP-ENV:Body></SOAP-ENV:Envelope>'

    def stripSoap(data):
        #simple replacement of some html entities:
        filter = {"&gt;":">", "&lt;":"<", "&quot;":'"',
                  "&apos;":"'", "&amp;":"&"}
        for (k,v) in filter.iteritems():
            data = data.replace(k,v)
        #strip the soap envelope (first and last lines, HACK):
        return "\n".join(data.split("\n")[1:-1])

    if (not os.path.exists(cachefilename) or \
            (cachetimeout and
             time.time() - os.stat(cachefilename).st_mtime > cachetimeout)):
        conn = httplib.HTTPConnection(host)
        #conn.set_debuglevel(999)
        conn.request('POST',path,soapreqheader + \
                         soapreqstring + soapreqfooter,headers)
        file(cachefilename,'w').write(stripSoap(conn.getresponse().read()))

    return file(cachefilename,'r').read()

#
# GET LONG/LAT FOR ZIPCODE
# (if no cache file)
#

soapzipreq = """<ns1:LatLonListZipCode>
<zipCodeList>%s</zipCodeList>
</ns1:LatLonListZipCode>""" % zipcode

latlondataxml = getXmlData("LatLonListZipCode",soapzipreq,
                        "%s.latlon.%s" % (sys.argv[0],zipcode)) #no timeout/permanent

try:
    latlonelement = minidom.parseString(latlondataxml).getElementsByTagName("latLonList")[0]
    if not latlonelement.childNodes:
        raise Exception()
    coords = latlonelement.childNodes[0].data.split(",")
    latitude = float(coords[0])
    longitude = float(coords[1])
except:
    print "Unable to find latitude/longitude. Bad zip code?"
    help()

#
# GET WEATHER DATA FOR LONG/LAT
# (if cache file is missing or old)
#

today = time.strftime("%Y-%m-%d")
soapweatherreq = """<ns1:NDFDgenByDay>
<latitude>%s</latitude><longitude>%s</longitude>
<startDate>%sZ</startDate><numDays>%s</numDays>
<format>12 hourly</format>
</ns1:NDFDgenByDay>""" % (latitude,longitude,today,numdays)

weatherdataxml = getXmlData("NDFDgenByDay",soapweatherreq,
                            "%s.cache.%s" % (sys.argv[0],zipcode), 3*3600) #3 hour timeout

#
# PARSE WEATHER DATA
#

#period-name attribute is inconsistent, so just calculate the name ourselves
def getTimeString(xmlrawtime):
    #2009-05-28T18:00:00-04:00
    night_boundary = 16 #technically could be 18 but just in case
    xmltime = time.strptime(xmlrawtime,"%Y-%m-%dT%H:%M:%S-04:00")
    curtime = time.localtime()
    if (xmltime.tm_year == curtime.tm_year and
        xmltime.tm_mon == curtime.tm_mon and
        xmltime.tm_mday == curtime.tm_mday):
        if xmltime.tm_hour >= night_boundary:
            return "Tonight"
        else:
            return "Today"
    else:
        weekday = time.strftime("%A",xmltime)
        if xmltime.tm_hour >= night_boundary:
            weekday = weekday.replace("day","Night")
        return weekday


weatherdata = minidom.parseString(weatherdataxml).getElementsByTagName("data")[0]

#time period names (used as keys by other dicts below)
perioddict = {} # layout-key : [timename1, timename2, ...]
for layoutset in weatherdata.getElementsByTagName("time-layout"):
    #<layout-key>KEY</layout-key>
    keyelem = layoutset.getElementsByTagName("layout-key")[0]
    if not keyelem.childNodes:
        continue
    key = keyelem.childNodes[0].data
    if not perioddict.has_key(key):
        perioddict[key] = []
    #<start-valid-time period-name="Today">
    for timeelem in layoutset.getElementsByTagName("start-valid-time"):
        if not timeelem.childNodes:
            continue
        timeval = timeelem.childNodes[0].data
        perioddict[key].append(getTimeString(timeval))

#max/min temps
maxtempdict = {} # timenameN : maxtempF
mintempdict = {} # timenameN : mintempF
for tempset in weatherdata.getElementsByTagName("temperature"):
    #<temperature type="maximum" units="Fahrenheit" time-layout="k-p24h-n3-1">
    temptype = tempset.getAttribute("type")
    layoutkey = tempset.getAttribute("time-layout")
    #<value>61</value>
    tempelems = tempset.getElementsByTagName("value")
    for i in range(0,len(tempelems)):
        tempelem = tempelems[i]
        #translate layout key + list position of value to period name:
        periodname = perioddict[layoutkey][i]
        if not tempelem.childNodes:
            continue
        temp = tempelem.childNodes[0].data
        if temptype == "maximum":
            maxtempdict[periodname] = temp
        else:
            mintempdict[periodname] = temp

#precip percentages
precipdict = {} # timenameN : probability%
timelabels = []
for precipset in weatherdata.getElementsByTagName("probability-of-precipitation"):
    #<probability-of-precipitation type="12 hour" units="percent" time-layout="k-p12h-n6-3">
    layoutkey = precipset.getAttribute("time-layout")
    timelabels = perioddict[layoutkey]#this happens to be the set of all time labels
    #<value>61</value>
    precipelems = precipset.getElementsByTagName("value")
    for i in range(0,len(precipelems)):
        precipelem = precipelems[i]
        #translate layout key + list position of value to period name:
        periodname = perioddict[layoutkey][i]
        if not precipelem.childNodes:
            continue
        precipdict[periodname] = precipelem.childNodes[0].data

#weather summaries
summarydict = {} # timenameN : summarystring
weather = weatherdata.getElementsByTagName("weather")[0]
#<weather time-layout="k-p12h-n6-3">
summarylayoutkey = weather.getAttribute("time-layout")
conditionsset = weather.getElementsByTagName("weather-conditions")
for i in range(0,len(conditionsset)):
    conditions = conditionsset[i]
    #<weather-conditions weather-summary="Rain Showers">
    summary = conditions.getAttribute("weather-summary")
    if not summary == "":
        #translate layout key + list position of value to period name:
        periodname = perioddict[summarylayoutkey][i]
        summarydict[periodname] = summary

#
# PRINT PARSED CONTENTS
#

bluecolor = "003"
redcolor = "300"
plaincolor = "101"
divider = ", "

printme = ""
for timelabel in timelabels:
    temp = ""
    if mintempdict.has_key(timelabel):
        temp = " <color%s>" % bluecolor + \
            mintempdict[timelabel] + \
            "<color%s>F" % plaincolor
    elif maxtempdict.has_key(timelabel):
        temp = " <color%s>" % redcolor + \
            maxtempdict[timelabel] + \
            "<color%s>F" % plaincolor

    conditions = ""
    #if precipdict.has_key(timelabel) and summarydict.has_key(timelabel):
    #    conditions = " ("+precipdict[timelabel]+"% "+summarydict[timelabel]+")"
    if precipdict.has_key(timelabel) and summarydict.has_key(timelabel):
        conditions = " ("+summarydict[timelabel]+")"

    if temp:
        printme += "%s:%s%s%s" % (timelabel,temp,conditions,divider)

print "<color%s>" % plaincolor + printme.strip(divider)
