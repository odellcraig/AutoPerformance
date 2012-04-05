'''
@author: codell
for CS526 Project
'''

from UserString import MutableString
import commands
import time
import re
import os

class AutoPerformanceConfig(object):
    def __init__(self):
        self.time = 10
        self.frameSize = 1518
        self.dscp = 0
        self.numStreams = 1
        self.udpRateString = "10M"
        self.host = ""
        self.port = 5004
        self.serverPort = 5004
        
    def fromString(self, stringConfig):
        '''
        Pass in string format and set members
        '''
        items = stringConfig.split()
        self.time           = items[0]
        self.frameSize      = items[1]
        self.dscp           = items[2]
        self.numStreams     = items[3]
        self.udpRateString  = items[4] 
        self.host           = items[5] 
        self.port           = items[6]
        self.serverPort     = items[7]
    
    def toString(self):
        '''
        This will create a string the members 
        '''
        strMe = MutableString()
        strMe += self.time + " "
        strMe += self.frameSize + " "
        strMe += self.dscp + " "
        strMe += self.numStreams + " " 
        strMe += self.udpRateString + " "
        strMe += self.host + " "
        strMe += self.port + " "
        strMe += self.serverPort
        return str(strMe)
        

class AutoPerformanceEngine(object):
    '''
    classdocs
    '''
    def __init__(self, _config):
        '''
        Constructor - pass in configuration
        '''
        self.config = _config
        self.pathToThrulay = 'thrulay-0.9/src/'
        pass
    
    
    def startServer(self):
        print "AutoPerformance::Engine::Checking if server is running..."
        result,output = commands.getstatusoutput('ps -e |grep "thrulayd"')
        
        #Start the server either way...
        cmd = MutableString()
        cmd += self.pathToThrulay + 'thrulayd -p'
        cmd += self.config.serverPort
        start,output = commands.getstatusoutput(str(cmd))
        if not start:
            print "Server was successfully started."
        else:
            print "Server already running"
            
            
            
    def stopServer(self):
        result,output = commands.getstatusoutput('killall lt-thrulayd 2>/dev/null')
        goodness = (result == 0)
        print "AutoPerformance::Engine::Result of stop server command success = ", goodness
    
    
    def runUdp(self):
        '''
        Returns the summary line to be added to the UDP summary 
        (since UDP doesn't provide incremental results, only summary
        line is needed)
        '''
        #Run the UDP version of the test
        udpCommand = self.pathToThrulay + 'thrulay -t' + str(int(self.config.time))
        udpCommand += ' -D' + str(self.config.dscp)
        udpCommand += ' -u' + self.config.udpRateString
        udpCommand += ' -p' + str(self.config.port)
        udpCommand += ' ' + self.config.host
        print "AutoPerformance::Engine::Running (", udpCommand,")"
        output = commands.getoutput(udpCommand);
        lines  = output.split("\n");
        
        #If summary does not exist, put headers in it
        loss        = 0.0
        jitter      = 0.0
        duplicate   = 0.0
        reorder     = 0.0
               
        for line in lines:
            #If summary, then get summary info send to summary file
            if(re.match(r'^Loss:', line)):
                extract = re.search(r'\s+(.+)%\s*$',line)
                if(extract):
                    loss = extract.group(1)
            elif(re.match(r'^Jitter:', line)):
                extract = re.search(r'\s+(.+)\s*$',line)
                if(extract):
                    jitter = extract.group(1)
            elif(re.match(r'^Duplication:', line)):
                extract = re.search(r'\s+(.+)%\s*$',line)
                if(extract):
                    duplicate = extract.group(1)
            elif(re.match(r'^Reordering:', line)):
                extract = re.search(r'\s+(.+)%\s*$',line)
                if(extract):
                    reorder = extract.group(1)
          
           
        summaryLine = MutableString()
        summaryLine += time.strftime("%Y-%m-%d_%H:%M:%S") + ", "
        summaryLine += self.config.udpRateString + ", "
        summaryLine += loss       + ", "
        summaryLine += jitter     + ", "
        summaryLine += duplicate  + ", "
        summaryLine += reorder    + "\n"
        return summaryLine
    
    def runTcp(self):
        '''
        Returns a tuple of (data, summaryLine) where data are lines
        that should be outputed to the csv file (or transfered over tcp)
        and summaryLine is the summary of the run to be sent or appended to 
        the summary file
        '''
        #Run the system command and grab output
        #Parse output
        tcpCommand = MutableString()
        tcpCommand += self.pathToThrulay + 'thrulay -t' + str(self.config.time)
        tcpCommand += ' -D' + str(self.config.dscp)
        tcpCommand += ' -m' + str(self.config.numStreams)
        tcpCommand += ' -p' + str(self.config.port)
        tcpCommand += ' ' + self.config.host
        print "AutoPerformance::Engine::Running (", tcpCommand,")"
        output = commands.getoutput(str(tcpCommand));
        lines  = output.split("\n");
        
        summaryData = MutableString()
        data        = MutableString()
        data += "Time, Mbps, RTT[ms], Jitter[ms]\n"
        
          
        for line in lines:
            #If summary, then get summary info send to summary file
            if(re.match(r'^\s*#\(\*\*\)', line)):
                extract = re.search(r'^\s*#\(\*\*\)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s*$',line)
                if(extract):
                    #Write to file
                    #Date, AverageMbps, AverateRTT, AverageJitter
                    summaryData += time.strftime("%Y-%m-%d_%H-%M-%S") + ", "
                    summaryData += str(extract.group(3))+", "           #Mbps
                    summaryData += str(extract.group(4))+", "           #RTT
                    summaryData += str(extract.group(5))+"\n"           #Jitter
                    
                    #Also add it to the end of the data line so we have it in that file
                    data += "Total: " #Totals to be added 
                    data += str(extract.group(3))+", " # Mbps
                    data += str(extract.group(4))+", " # RTT
                    data += str(extract.group(5))+"\n" # Jitter
                    
            if(re.match(r'^\s*\([\s\d]+\)\s*', line)):
                extract = re.search(r'^\s*\(([\s\d]+)\)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s*$', line)
                if(extract):
                    #Write to output
                    #Time, Mbps, RTT, Jitter
                    data += str(extract.group(3))+", " # Time
                    data += str(extract.group(4))+", " # Mbps
                    data += str(extract.group(5))+", " # RTT
                    data += str(extract.group(6))+"\n" # Jitter
                    
                    if(0):
                        print "Stream     = ", extract.group(1)
                        print "Time Start = ", extract.group(2)
                        print "Time End   = ", extract.group(3)
                        print "Mbps       = ", extract.group(4)
                        print "RTT delay  = ", extract.group(5)
                        print "Jitter     = ", extract.group(6)
        return data,summaryData
           
            
        
    
if __name__ == "__main__":
    config = AutoPerformanceConfig()
    config.time = 20
    config.frameSize = 1518
    config.dscp = 0
    config.numStreams = 1
    config.host = "localhost" 
    
    engine = AutoPerformanceEngine(config)
    print "Now running TCP:"
    engine.startServer()
    tcpData,tcpSummary = engine.runTcp()
    udpData = engine.runUdp("40M")
    engine.stopServer()
    
    #Test of date and back
    # Use time.strftime to go to string and time.strptime to come back with "%Y-%m-%d_%H:%M:%S" as format
    #print "2012-03-05_23:30:30 == ", time.strptime("2012-03-05_23:30:30", "%Y-%m-%d_%H:%M:%S")
