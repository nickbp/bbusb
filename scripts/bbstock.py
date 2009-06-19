#!/usr/bin/python

'''
  bbstock.py - Retrieves ticker data from Yahoo Finance
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


import csv, httplib, sys, os, time

#each set has the name for the set,
#followed by the set's symbols and their labels:
allstocks = {
    "index":[
        "US Indices",
        ["^GSPC","S&P500"],
        ["^W5000","Wilshire5000"],
        ["^DJI","Dow Ind."],
        ["^IXIC","NASDAQ"],
        ],
    "intlindex":[
        "Intl Indices",
        ["^HSI","HangSeng<small>(HK)<normal>"],
        ["^N225","Nikkei225<small>(JP)<normal>"],
        ["^FTSE","FTSE100<small>(UK)<normal>"],
        ["^GDAXI","DAX<small>(DE)<normal>"]
        ],
    "treasuries":[
        "Treasuries",
        ["^IRX","13wk T-Bill"],
        ["^FVX","5yr T-Bill"],
        ["^TNX","10yr T-Bill"],
        ["^TYX","30yr T-Bill"]
        ],
    "bignames":[
        "Software",
        ["AVID","Avid"],
        ["GOOG","Google"],
        ["IBM","IBM"],
        ["MSFT","Microsoft"],
        ["AAPL","Apple"],
        ["ADBE","Adobe"]
        ],
    "neighbors":[
        "Neighbors",
        ["NTAP","NetApp"],
        ["ISLN","Isilon"],
        ["JNPR","Juniper"],
        ["AKAM","Akamai"],
        ["ORCL","Oracle"],
        ["EMC","EMC"]
        ]
}

#available yahoo GET fields:
fields = {
    "1 yr Target Price":"t8",
    "200-day Moving Average":"m4",
    "50-day Moving Average":"m3",
    "52-week High":"k",
    "52-week Low":"j",
    "52-week Range":"w",
    "After Hours Change (Real-time)":"c8",
    "Annualized Gain":"g3",
    "Ask":"a",
    "Ask (Real-time)":"b2",
    "Ask Size":"a5",
    "Average Daily Volume":"a2",
    "Bid":"b",
    "Bid (Real-time)":"b3",
    "Bid Size":"b6",
    "Book Value":"b4",
    "Change":"c1",
    "Change & Percent Change":"c",
    "Change (Real-time)":"c6",
    "Change From 200-day Moving Average":"m5",
    "Change From 50-day Moving Average":"m7",
    "Change From 52-week High":"k4",
    "Change From 52-week Low":"j5",
    "Change Percent (Real-time)":"k2",
    "Change in Percent":"p2",
    "Commission":"c3",
    "Day's High":"h",
    "Day's Low":"g",
    "Day's Range":"m",
    "Day's Range (Real-time)":"m2",
    "Day's Value Change":"w1",
    "Day's Value Change (Real-time)":"w4",
    "Dividend Pay Date":"r1",
    "Dividend Yield":"y",
    "Dividend/Share":"d",
    "EBITDA":"j4",
    "EPS Estimate Current Year":"e7",
    "EPS Estimate Next Quarter":"e9",
    "EPS Estimate Next Year":"e8",
    "Earnings/Share":"e",
    "Error Indication (returned for symbol changed / invalid)":"e1",
    "Ex-Dividend Date":"q",
    "Float Shares":"f6",
    "High Limit":"l2",
    "Holdings Gain":"g4",
    "Holdings Gain (Real-time)":"g6",
    "Holdings Gain Percent":"g1",
    "Holdings Gain Percent (Real-time)":"g5",
    "Holdings Value":"v1",
    "Holdings Value (Real-time)":"v7",
    "Last Trade (Price Only)":"l1",
    "Last Trade (Real-time) With Time":"k1",
    "Last Trade (With Time)":"l",
    "Last Trade Date":"d1",
    "Last Trade Size":"k3",
    "Last Trade Time":"t1",
    "Low Limit":"l3",
    "Market Cap (Real-time)":"j3",
    "Market Capitalization":"j1",
    "More Info":"i",
    "Name":"n",
    "Notes":"n4",
    "Open":"o",
    "Order Book (Real-time)":"i5",
    "P/E Ratio":"r",
    "P/E Ratio (Real-time)":"r2",
    "PEG Ratio":"r5",
    "Percent Change From 200-day Moving Average":"m6",
    "Percent Change From 50-day Moving Average":"m8",
    "Percent Change From 52-week High":"k5",
    "Percent Change From 52-week Low":"j6",
    "Previous Close":"p",
    "Price Paid":"p1",
    "Price/Book":"p6",
    "Price/EPS Estimate Current Year":"r6",
    "Price/EPS Estimate Next Year":"r7",
    "Price/Sales":"p5",
    "Shares Owned":"s1",
    "Short Ratio":"s7",
    "Stock Exchange":"x",
    "Symbol":"s",
    "Ticker Trend":"t7",
    "Trade Date":"d2",
    "Trade Links":"t6",
    "Volume":"v"
}

# Choose symbols:

if len(sys.argv) < 2 or \
        not allstocks.has_key(sys.argv[1]):
    sys.stderr.write("""Syntax: %s <type> 
Types: %s\n""" % (sys.argv[0],", ".join(allstocks.keys())))
    sys.exit(1)

key = " ".join(sys.argv[1:])
stocksymbols = [stock[0] for stock in allstocks[key][1:]]
setname = allstocks[key][0]
stocknames = {}
for stock in allstocks[key][1:]:
    stocknames[stock[0]] = stock[1]

# Request data:

def get_cached(host,hostpath,cachekey,cachetimeout):
    cachename = sys.argv[0]+".cache."+cachekey
    if (not os.path.exists(cachename) or \
            time.time() - os.stat(cachename).st_mtime > cachetimeout):
        conn = httplib.HTTPConnection(host)
        conn.request('GET',hostpath)
        resp = conn.getresponse()
        if resp.status == 200:
            cachefile = file(cachename,'w')
            cachefile.write(resp.read())
            cachefile.close()
        else:
            sys.stderr.write("Warning: Got status %s (%s) when retrieving %s%s" % \
                (resp.status,resp.reason,host,hostpath))

    return file(cachename)

path="/d/quotes.csv?f=%s&e=.csv&s=%s"
querycols=["Symbol","Name","Change",
       "52-week High","52-week Low",
       "Last Trade (Price Only)"]
querychars="".join([fields[col] for col in querycols])

query = path % (querychars,",".join(stocksymbols))
datafile = get_cached("download.finance.yahoo.com",query,key,1800)

# Print output:

reader = csv.DictReader(datafile,
                        fieldnames=querycols,
                        restval="N/A")

greencolor = "030"
redcolor = "300"
plaincolor = "110"
divider = ", "

def stockpricelowhigh(lowval,highval,curval):
    if curval == 0 or lowval == 0 or highval == 0:
        return ""
    lowpctchange = curval/lowval - 1
    highpctchange = curval/highval - 1
    lowout = "<color%s>%+.0f%%" % (redcolor, lowpctchange*100)
    highout = "<color%s>%+.0f%%" % (greencolor, highpctchange*100)
    return "(52W:%s%s<color%s>)" % (lowout, highout, plaincolor)

def stockprice(stock,showminmax=False):
    name = stocknames.get(stock["Symbol"],stock["Symbol"])
    price = float(stock["Last Trade (Price Only)"])
    changeval = float(stock["Change"])
    #if changeval < 0:
    #    change = "<color%s>&downarrow;<color%s>%.02f" % (redcolor,plaincolor,abs(changeval))
    #else:
    #    change = "<color%s>&uparrow;<color%s>%.02f" % (greencolor,plaincolor,changeval)
    changepct = ((price+changeval)/price - 1) * 100
    if changepct < 0:
        change = "<color%s>&downarrow;<color%s>%.01f%%" % (redcolor,plaincolor,abs(changepct))
    else:
        change = "<color%s>&uparrow;<color%s>%.01f%%" % (greencolor,plaincolor,changepct)
    if showminmax:
        change += stockpricelowhigh(float(stock["52-week Low"]),
                                    float(stock["52-week High"]),
                                    price)
    return "%s %.02f%s" % (name, price, change)

vals = {}
for line in reader:
    vals[line["Symbol"]] = line
out = ""
for stocksymbol in stocksymbols:
    out += stockprice(vals[stocksymbol],True) + divider
print "<color%s>%s: %s" % (plaincolor, setname, out[:-len(divider)])
