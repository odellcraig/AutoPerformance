'''
Created on Mar 3, 2012

@author: codell
'''
from AutoPerformanceEngine import AutoPerformanceConfig, AutoPerformanceEngine
from UserString import MutableString
import os
import socket
import time

class AutoPerformanceClientConfig(object):
    def __init__(self):
        self.duration  = 10
        self.frameSize = 1518
        self.dscp = 0
        self.numStreams = 1
        self.host = ""
        self.port = 5002
        self.udpRateString = "10M"
        self.outputDirectory = ""
        
    def __str__(self):
        mString = MutableString()
        mString += "host=            " + str(self.host) + "\n"
        mString += "port=            " + str(self.port) + "\n"
        mString += "duration=        " + str(self.duration) + "\n"
        mString += "frameSize=       " + str(self.frameSize) + "\n"
        mString += "dscp=            " + str(self.dscp) + "\n"
        mString += "numStreams=      " + str(self.numStreams) + "\n"
        mString += "udpRateString=   " + str(self.udpRateString) + "\n"
        mString += "outputDirectory= " + str(self.outputDirectory) + "\n"
        return str(mString)
        


class AutoPerformanceClient(object):
    '''
    classdocs
    '''
    def __init__(self, config):
        '''
        Constructor
        '''
        self.config = config
        self.sock = None
        

    def go(self):
        print "AutoPerformance::Client::Opening connection..."
        self.open()
        print "AutoPerformance::Client::Starting test..."
        self.run()
        print "AutoPerformance::Client::Stopping test..."
        self.close()
    
    def open(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            self.sock.connect((self.config.host, self.config.port))
        except:
            print "AutoPerformance::Client::Error connecting to ", self.config.host, "on ", self.config.port
      
    def close(self):
        self.sock.close()
        
    def run(self):
        #Create Configurations
        near,far = self.getConfigurations()
        engine = AutoPerformanceEngine(near)

        #Send Configuration
        self.sendConfiguration(far.toString())
        
        try:
            #Start the server
            engine.startServer()
            
            #Send Run TCP
            self.sendTcpStart()
            dataNear,summaryNear = engine.runTcp()
            dataFar ,summaryFar  = self.receiveTcpResupts()
            self.ouputTcpToFile(dataNear, dataFar, summaryNear, summaryFar)
            
            #Send Run Udp
            self.sendUdpStart()
            summaryNear = engine.runUdp()
            summaryFar  = self.receiveUdpResults()
            self.ouputUdpToFile(summaryNear, summaryFar)
            
        except:
            print "AutoPerformance::Client::Error: an exception occured in client::run()"
        finally:
            #stop the server
            engine.stopServer()
            print "AutoPerformance::Client::Done on client side."
        
        print "Client is finishing now."
        
          
    def getConfigurations(self):
        '''
        Creates the near and far side configurations, returns near,far
        '''
        configNear = AutoPerformanceConfig()
        configNear.time = self.config.duration
        configNear.frameSize = self.config.frameSize
        configNear.dscp = self.config.dscp
        configNear.numStreams = self.config.numStreams
        configNear.udpRateString = self.config.udpRateString
        configNear.host = self.config.host
        configNear.port = self.config.port+1
        configNear.serverPort = self.config.port+2

        configFar = AutoPerformanceConfig()
        configFar.time = self.config.duration
        configFar.frameSize = self.config.frameSize
        configFar.dscp = self.config.dscp
        configFar.numStreams = self.config.numStreams
        configFar.udpRateString = self.config.udpRateString
        configFar.host = 'lookMeUp' #Get the local IP address on far side
        configFar.port = self.config.port+2 #Get the local port +1
        configFar.serverPort = self.config.port+1
        return configNear,configFar

    def sendConfiguration(self, farString):
        self.sock.sendall(farString)
            
    def sendTcpStart(self):
        self.sock.sendall('tcp')
    def sendUdpStart(self):
        self.sock.sendall('udp')
        
    def receiveTcpResupts(self):
        '''
        First receive the data file for the run, then
        receive the summary line for the run - send 
        ack to separate (not sure if that will work)
        '''
        print "AutoPerformance::Client::Receiving TCP Results (Data Run)"
        data = self.sock.recv(8192)
        print "AutoPerformance::Client::Sending Acknowledgement"
        self.sock.sendall('ack')
        print "AutoPerformance::Client::Receiving TCP Summary Results"
        summary = self.sock.recv(1024)
        print "AutoPerformance::Client::Done Receiving TCP Results"
        return data,summary
        
    def receiveUdpResults(self):
        print "AutoPerformance::Client::Receiving UDP Results."
        data = self.sock.recv(1024);
        print "AutoPerformance::Client::Done Receiving UDP Results."
        return data
        
            
    def ouputTcpToFile(self, dataNear, dataFar, summaryNear, summaryFar):
        #If the summay file does not exist, create it and add headers
        summaryDownPath = self.config.outputDirectory+"/Downstream/Summary_Tcp.csv"
        summaryUpPath   = self.config.outputDirectory+"/Upstream/Summary_Tcp.csv"
        fileDate = time.strftime("%Y-%m-%d_%H:%M:%S")
        dataFileDownPath   = self.config.outputDirectory+"/Downstream/"+fileDate+"_Tcp.csv"
        dataFileUpPath     = self.config.outputDirectory+"/Upstream/"+fileDate+"_Tcp.csv"
        if(not os.path.isfile(summaryDownPath)):
            putHeaders = open(summaryDownPath, 'a')
            putHeaders.write("Date, AverageMbps, AverageRtt, AverageJitter\n")
            putHeaders.close()
        if(not os.path.isfile(summaryUpPath)):
            putHeaders = open(summaryUpPath, 'a')
            putHeaders.write("Date, AverageMbps, AverageRtt, AverageJitter\n")
            putHeaders.close()
        
        dataDownFile = open(dataFileDownPath, 'w')
        dataUpFile   = open(dataFileUpPath, 'w')
        
        #Open the summary files for appending
        summaryDownFile = open(summaryDownPath, 'a')
        summaryUpFile   = open(summaryUpPath, 'a')
        
        #Push the data into the data files
        dataUpFile.write(str(dataNear)) #dataNear is Near sending to Far (upstream)
        dataDownFile.write(str(dataFar)) #dataFar is Far sending to Near (downstream)
    
        #Push data into summary files
        summaryUpFile.write(str(summaryNear)) 
        summaryDownFile.write(str(summaryFar))
        
    def ouputUdpToFile(self, summaryNear, summaryFar):
        #If the summay file does not exist, create it and add headers
        summaryDownPath = self.config.outputDirectory+"/Downstream/Summary_Udp.csv"
        summaryUpPath   = self.config.outputDirectory+"/Upstream/Summary_Udp.csv"
        if(not os.path.isfile(summaryDownPath)):
            putHeaders = open(summaryDownPath, 'a')
            putHeaders.write("Date, Mbps, LossPercent, Jitter, DuplicationPercent, ReorderingPercent\n")
            putHeaders.close()
        if(not os.path.isfile(summaryUpPath)):
            putHeaders = open(summaryUpPath, 'a')
            putHeaders.write("Date, Mbps, LossPercent, Jitter, DuplicationPercent, ReorderingPercent\n")
            putHeaders.close()
        
        #Open the summary files for appending
        summaryDownFile = open(summaryDownPath, 'a')
        summaryUpFile   = open(summaryUpPath, 'a')
    
        #Push data into summary files
        summaryUpFile.write(str(summaryNear)) 
        summaryDownFile.write(str(summaryFar))
        
        
        
        
        
            

#       if(not os.path.isfile(self.summaryUdp)):
#            putHeaders = open(self.summaryUdp, 'a')
#            putHeaders.write("Date,  Mbps, LossPercent, Jitter, DuplicationPercent, ReorderingPercent\n")
#            putHeaders.close()
#        sumFile = open(self.summaryUdp, 'a')    