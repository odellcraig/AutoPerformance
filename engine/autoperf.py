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
import sys
import time


    


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
             
    MIN_ARG_COUNT = 3
    
    if(len(sys.argv) < MIN_ARG_COUNT):
        print "AutoPerformance: too few arguments.\n"
        print usage
        sys.exit()
    
    if(sys.argv[1] == "-s"):
        #server = getServerFromConfig(sys.argv[2])
        server = AutoPerformanceServer('localhost', 5003)
        server.startServer()
        
        #Daemonize
#        if os.fork():
#            sys.exit()
            
        while 1:
            time.sleep(60)
        
        
        server.stopServer()
    
    if(sys.argv[1] == '-c'):
        #client = getClientFromConfig(sys.argv[2])
        
        #Setup the "Cron" Job
        minute = allMatch
        hour = allMatch
        day = allMatch
        month = allMatch
        dayOfWeek = allMatch
            
        #TODO: Pass in client/server config from file(s)
        clientConfig = AutoPerformanceClientConfig()
        setupClient(clientConfig)
        client = AutoPerformanceClient(clientConfig)
    
        cron = CronTab(Event(client.go, minute,hour,day,month,dayOfWeek))
        
        #Daemonize
#        if os.fork():
#            sys.exit()
        
        cron.run()
    
    
    
    
    
    