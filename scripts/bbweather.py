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

debug = False

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
	resp = conn.getresponse().read()
        respcontent = stripSoap(resp)
	if respcontent:
            file(cachefilename,'w').write(respcontent)
        else:
            raise Exception("No/bad data returned for query '" + soapaction + "': " + resp)

    return file(cachefilename,'r').read()

#
# GET LONG/LAT FOR ZIPCODE
# (if no cache file)
#

soapzipreq = """<ns1:LatLonListZipCode>
<zipCodeList>%s</zipCodeList>
</ns1:LatLonListZipCode>""" % zipcode

latlondataxml = getXmlData("LatLonListZipCode",soapzipreq,
                        "%s.cache.latlon.%s" % (sys.argv[0],zipcode)) #no timeout/permanent

try:
    latlonelement = minidom.parseString(latlondataxml).getElementsByTagName("latLonList")[0]
    if not latlonelement.childNodes:
        raise Exception()
    coords = latlonelement.childNodes[0].data.split(",")
    latitude = float(coords[0])
    longitude = float(coords[1])
except:
    print sys.exc_info()
    print "Unable to find latitude/longitude. Bad zip code?"
    help()

#
# GET WEATHER DATA FOR LONG/LAT
# (if cached weather is missing or old)
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

def timeIsNight(timeobj):
    night_boundary = 16 #technically could be 18h but just in case
    return timeobj.tm_hour >= night_boundary

weatherdata = minidom.parseString(weatherdataxml).getElementsByTagName("data")[0]

#time period names (used as keys by other dicts below)
perioddict = {} # ordered combination of day and night dicts
for layoutset in weatherdata.getElementsByTagName("time-layout"):
    #<layout-key>KEY</layout-key>
    keyelem = layoutset.getElementsByTagName("layout-key")[0]
    if not keyelem.childNodes:
        continue
    layoutkey = keyelem.childNodes[0].data
    #<start-valid-time period-name="Today">
    for timeelem in layoutset.getElementsByTagName("start-valid-time"):
        if not timeelem.childNodes:
            continue
        rawtimeval = timeelem.childNodes[0].data
        #2009-05-28T18:00:00-04:00
        timeobj = time.strptime(rawtimeval,"%Y-%m-%dT%H:%M:%S-04:00")

        curtime = time.localtime()
        timelabel = ""
        if (timeobj.tm_year == curtime.tm_year and
            timeobj.tm_mon == curtime.tm_mon and
            timeobj.tm_mday == curtime.tm_mday):
            timelabel = "Today"
        else:
            timelabel = time.strftime("%A",timeobj)
        
        if not perioddict.has_key(layoutkey):
            perioddict[layoutkey] = []
        perioddict[layoutkey].append((timelabel,
                                      timeIsNight(timeobj)))

if debug:
    print "period:",perioddict

#max(day)/min(night) temps
tempdict = {} # timeN : tempF
for tempset in weatherdata.getElementsByTagName("temperature"):
    #<temperature type="maximum" units="Fahrenheit" time-layout="k-p24h-n3-1">
    temptype = tempset.getAttribute("type")
    layoutkey = tempset.getAttribute("time-layout")
    #<value>61</value>
    tempelems = tempset.getElementsByTagName("value")
    for i in range(0,len(tempelems)):
        tempelem = tempelems[i]
        if not tempelem.childNodes:
            continue
        temp = tempelem.childNodes[0].data

        period = perioddict[layoutkey][i]
        tempdict[period] = temp

if debug:
    print "temp:",tempdict

#precip percentages
precipdict = {} # timeN : probability%
timelabels = [] # in-order list of "Today", "Wednesday", etc
for precipset in weatherdata.getElementsByTagName("probability-of-precipitation"):
    #<probability-of-precipitation type="12 hour" units="percent" time-layout="k-p12h-n6-3">
    layoutkey = precipset.getAttribute("time-layout")
    #the period for precip happens to include all labels, so use it:
    for period in perioddict[layoutkey]:
        #skip consecutive duplicates:
        if not timelabels or timelabels[-1] != period[0]:
            timelabels.append(period[0])
    #<value>61</value>
    precipelems = precipset.getElementsByTagName("value")
    for i in range(0,len(precipelems)):
        precipelem = precipelems[i]
        #translate layout key + list position of value to period name:
        period = perioddict[layoutkey][i]
        if not precipelem.childNodes:
            continue
        precipdict[period] = precipelem.childNodes[0].data

if debug:
    print "precip:", precipdict

#weather summaries
summarydict = {} # timenameN : (summarystring, isFromValueTag)
for weatherset in  weatherdata.getElementsByTagName("weather"):
    #<weather time-layout="k-p12h-n6-3">
    summarylayoutkey = weatherset.getAttribute("time-layout")
    conditionsset = weatherset.getElementsByTagName("weather-conditions")
    for i in range(0,len(conditionsset)):
        conditions = conditionsset[i]
        #<weather-conditions weather-summary="Chance Rain Showers">
        #  <value coverage="chance" intensity="light" weather-type="rain showers" qualifier="none"/>
        #  <value coverage="isolated" intensity="none" additive="and" weather-type="thunderstorms" qualifier="none"/>
        #</weather-conditions>
        summaryset = conditions.getElementsByTagName("value")
        if summaryset and summaryset[0].hasAttribute("weather-type"):
            summarystr = summaryset[0].getAttribute("weather-type")
            summary = (" ".join([word.capitalize() for word in summarystr.split()]), True)
        else:
            summary = (conditions.getAttribute("weather-summary"), False)
        if not summary[0] == "":
            #translate layout key + list position of value to period name:
            periodname = perioddict[summarylayoutkey][i]
            summarydict[periodname] = summary

if debug:
    print "summary:", summarydict

#
# PRINT PARSED CONTENTS
#

bluecolor = "002"
redcolor = "200"
plaincolor = "120"
divider = ", "

def getConditionStr(period):
    returnme = " "
    if summarydict.has_key(period):
        #returnme += "<small>("
        if precipdict.has_key(period) and summarydict[period][1]:
            returnme += precipdict[period]+"% "
        #returnme += summarydict[period][0]+")<normal>"
        returnme += summarydict[period][0]
    return returnme

printme = ""
for timelabel in timelabels:
    dayperiod = (timelabel,False)
    nightperiod = (timelabel,True)

    weatherstr = " "

    if tempdict.has_key(dayperiod):
        weatherstr += "<color%s>" % redcolor + \
            tempdict[dayperiod] + \
            "F<color%s>" % plaincolor

    if tempdict.has_key(nightperiod):
        if tempdict.has_key(dayperiod):
            weatherstr += "/"
        weatherstr += "<color%s>" % bluecolor + \
            tempdict[nightperiod] + \
            "F<color%s>" % plaincolor

    # only display the daytime weather conditions, ignore nighttime conditions
    weatherstr += getConditionStr(dayperiod)
        
    if temp:
        printme += "%s:%s%s" % (timelabel,weatherstr,divider)

print "<color%s>" % plaincolor + printme.strip(divider)
