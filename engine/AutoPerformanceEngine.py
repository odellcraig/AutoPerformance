'''
@author: codell
for CS526 Project
'''

import commands
import time
import re
import os

class AutoPerformanceEngine(object):
    '''
    classdocs
    '''

    def __init__(self, _time, _frameSize, _dscp, _numStreams, _host, outputFile, summaryFile):
        '''
        Constructor - pass in configuration
        '''
        self.time = _time
        self.frameSize = _frameSize
        self.dscp = _dscp
        self.numStreams = _numStreams
        self.host = _host
        self.outputUdp = outputFile+"_udp.csv"
        self.outputTcp = outputFile+"_tcp.csv"
        self.summaryUdp = summaryFile+"_udp.csv"
        self.summaryTcp = summaryFile+"_tcp.csv"
        self.pathToThrulay = 'thrulay-0.9/src/'
        pass
    
    
    def startServer(self):
        result,output = commands.getstatusoutput('ps -e |grep "thrulayd"')
        print "Results of grep was:",result
        if result:
            cmd = self.pathToThrulay + 'thrulayd'
            start,output = commands.getstatusoutput(cmd)
            if not start:
                print "Server was successfully started."
        else:
            print "Server already running"
            
            
            
    def stopServer(self):
        result,output = commands.getstatusoutput('killall lt-thrulayd')
        goodness = (result == 0)
        print "Result of stop server command success = ", goodness
    
    
    def runUdp(self, rateString):
        #Run the UDP version of the test
        output = commands.getoutput('ls');
        lines  = output.split("\n");
        for line in lines:
            print "Line = ", line
            
        print "Save to UDP file: ", self.outputUdp
    
    
    def runTcp(self):
        #Run the system command and grab output
        #Parse output
        tcpCommand = self.pathToThrulay + 'thrulay -t' + str(self.time)
        tcpCommand += ' -D' + str(self.dscp)
        tcpCommand += ' -m' + str(self.numStreams)
        tcpCommand += ' ' + self.host
        print "Running (", tcpCommand,")"
        output = commands.getoutput(tcpCommand);
        lines  = output.split("\n");
        outFile = open(self.outputTcp, 'w')
        outFile.write("Time, Mbps, RTT[ms], Jitter[ms]\n")
        
        #If summary does not exist, put headers in it
        if(not os.path.isfile(self.summaryTcp)):
            putHeaders = open(self.summaryTcp, 'a')
            putHeaders.write("Date, AverageMbps, AverageRtt, AverageJitter\n")
            putHeaders.close()
        sumFile = open(self.summaryTcp, 'a')    
        
        for line in lines:
            #If summary, then get summary info send to summary file
            if(re.match(r'^\s*#\(\*\*\)', line)):
                extract = re.search(r'^\s*#\(\*\*\)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s*$',line)
                if(extract):
                    #Write to file
                    #Date, AverageMbps, AverateRTT, AverageJitter
                    sumFile.write(time.strftime("%Y-%m-%d_%H:%M:%S") + ", " + #Date
                                  str(extract.group(3))+", " + #Mbps
                                  str(extract.group(4))+", " + #RTT
                                  str(extract.group(5))+"\n"); #Jitter
                    
            if(re.match(r'^\s*\([\s\d]+\)\s*', line)):
                extract = re.search(r'^\s*\(([\s\d]+)\)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s*$', line)
                if(extract):
                    #Write to file
                    #Time, Mbps, RTT, Jitter
                    outFile.write(str(extract.group(3))+", " + #Time
                                  str(extract.group(4))+", " + #Mbps
                                  str(extract.group(5))+", " + #RTT
                                  str(extract.group(6))+"\n"); #Jitter
                     
                    if(0):
                        print "Stream     = ", extract.group(1)
                        print "Time Start = ", extract.group(2)
                        print "Time End   = ", extract.group(3)
                        print "Mbps       = ", extract.group(4)
                        print "RTT delay  = ", extract.group(5)
                        print "Jitter     = ", extract.group(6)
           
            
        print "Save to TCP file: ", self.outputTcp
        print "Append to summary file: ", self.summaryTcp
        outFile.close()
        sumFile.close()
        
    
if __name__ == "__main__":
    engine = AutoPerformanceEngine(20,1518,0,1,"localhost",time.strftime("%Y-%m-%d_%H:%M:%S"), "summary")
    print "Now running TCP:"
    engine.startServer()
    engine.runTcp()
    engine.runUdp("40M")
    engine.stopServer()
    
    #Test of date and back
    # Use time.strftime to go to string and time.strptime to come back with "%Y-%m-%d_%H:%M:%S" as format
    #print "2012-03-05_23:30:30 == ", time.strptime("2012-03-05_23:30:30", "%Y-%m-%d_%H:%M:%S")
