#!/usr/bin/python
'''
Created on Mar 6, 2012

@author: codell
'''
from AutoPerformanceClient import AutoPerformanceClient, \
    AutoPerformanceClientConfig
from AutoPerformanceServer import AutoPerformanceServer
from AutoPerformanceTimer import CronTab, Event, allMatch
import os
import re
import sys
import time


def getServerFromConfig(fileName):
    config = open(fileName, 'r')
    host = None
    port = None
    for line in config:
        if(re.match(r'^\s*#', line) or re.match(r'^\s*$', line)):
            continue
        tag = None
        value = None
        extract = re.search(r'^\s*(\w+)=\s*(.+)\s*$',line)
        if(extract):
            tag   = extract.group(1)
            value = extract.group(2) 
        if(tag == "host"):
            host = value
            continue
        if(tag == "port"):
            port = int(value)
            continue
    return host,port

def getDayRange(strRange):
    dayRange = []
    extract = re.search(r'^\s*(\d)\s*-\s*(\d)\s*$', strRange)
    if(extract):
        numDays = int(extract.group(2))-int(extract.group(1));
        day = int(extract.group(1))
        for i in range(numDays+1):
            dayRange.append(day)
            day+=1
    else:
        dayRange = re.split(r'\s*,\s*', strRange)
        for i in range(len(dayRange)):
            dayRange[i] = int(dayRange[i])
    return dayRange    
        


def getClientFromConfig(fileName, clientConfig):
    config = open(fileName, 'r')
    cronTuple = [allMatch, allMatch, allMatch, allMatch, allMatch]
    for line in config:
        #Skip comments
        if(re.match(r'^\s*#', line) or re.match(r'^\s*$', line)):
            continue
        tag = None
        value = None
        extract = re.search(r'^\s*(\w+)=\s*(.+)\s*$',line)
        if(extract):
            tag   = extract.group(1)
            value = extract.group(2) 
        if(tag == "host"):
            clientConfig.host = value
            continue
        if(tag == "port"):
            clientConfig.port = value
            continue
        if(tag == "duration"):
            clientConfig.duration = value
            continue
        if(tag == "frameSize"):
            clientConfig.frameSize = value
            continue
        if(tag == "dscp"):
            clientConfig.dscp = value
            continue
        if(tag == "numStreams"):
            clientConfig.numStreams = value
            continue
        if(tag == "udpRateString"):
            clientConfig.udpRateString = value
            continue
        if(tag == "outputDirectory"):
            clientConfig.outputDirectory = value
            continue
        if(tag == "minute"):
            if(value=="allMatch"):
                cronTuple[0]=allMatch
                continue
            cronTuple[0]=int(value)
            continue
        if(tag == "hour"):
            if(value=="allMatch"):
                cronTuple[1]=allMatch
                continue
            cronTuple[1]=int(value)
            continue
        if(tag == "day"):
            if(value=="allMatch"):
                cronTuple[2]=allMatch
                continue
            cronTuple[2]=int(value)
            continue
        if(tag == "month"):
            if(value=="allMatch"):
                cronTuple[3]=allMatch
                continue
            cronTuple[3]=int(value)
            continue
        if(tag == "dayOfWeek"):
            if(value=="allMatch"):
                cronTuple[4]=allMatch
                continue
            cronTuple[4]=getDayRange(value)
            continue
    return cronTuple


def setupClient(clientConfig):
    clientConfig.duration = 2
    clientConfig.frameSize = 1518
    clientConfig.dscp = 0
    
    clientConfig.numStreams = 1
    clientConfig.udpRateString = "40M"
    clientConfig.host = 'localhost'
    clientConfig.port = 5003
    clientConfig.outputDirectory = "../data"


if __name__ == "__main__":
    #TODO: open up schedule file (from command line)
    usage = "Server:\nautoperf.py -s <serverConfigFile>\n \
             Client:\nautoperf.py -c -d <clientConfigFile\n"
             
    MIN_ARG_COUNT = 2
    DEFAULT_SERVER_CONFIG = "server.conf"
    DEFAULT_CLIENT_CONFIG = "client.conf"
    
    if(len(sys.argv) < MIN_ARG_COUNT):
        print "AutoPerformance: too few arguments.\n"
        print usage
        sys.exit()
    
    if(sys.argv[1] == "-s"):
        serverConfigFile = DEFAULT_SERVER_CONFIG
        if(len(sys.argv) > MIN_ARG_COUNT+1):
            serverConfigFile = sys.argv[2]
        host,port = getServerFromConfig(serverConfigFile)
        print "Starting server on",host,port
        server = AutoPerformanceServer(host,port)
        server.startServer()
        #Daemonize
        #if os.fork():
        #    sys.exit()
        while 1:
            time.sleep(60)
        server.stopServer()
    
    if(sys.argv[1] == '-c'):
        clientConfig = AutoPerformanceClientConfig()
        clientConfigFile = DEFAULT_CLIENT_CONFIG
        if(len(sys.argv) > MIN_ARG_COUNT+1):
            clientConfigFile = sys.argv[2]
        cronList = getClientFromConfig(clientConfigFile, clientConfig)
        
        print "Using config:\n", clientConfig
        print "Using cron:\n", cronList
        
        #TODO: Pass in client/server config from file(s)
        clientConfig = AutoPerformanceClientConfig()
        setupClient(clientConfig)
        client = AutoPerformanceClient(clientConfig)
        cron = CronTab(Event(client.go,cronList[0],cronList[1],cronList[2],cronList[3],cronList[4]))
        #Daemonize
#        if os.fork():
#            sys.exit()
        
        cron.run()
        #Never reach here, unless badness
    
    
    
    
    
    