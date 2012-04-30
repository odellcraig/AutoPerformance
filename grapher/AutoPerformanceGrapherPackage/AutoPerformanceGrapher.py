'''
Created on Mar 5, 2012

@author: cody
'''
import matplotlib.pyplot as plt
import time
import datetime
import matplotlib.dates as md

'''
base class for both datasets
'''
class dataset(object):
    
    def __init__(self,_type, _filename):
        self.type = _type
        #type should be 'udp', 'tcp'
        
        self.dates = []
        
        #'udp' type lists
        self.udp_mbps = []
        self.udp_losspercent = []
        self.udp_jit = []
        self.udp_dup = []
        self.udp_reord = []
        
        #'tcp' type lists
        self.tcp_mbps = []
        self.tcp_rtt = []
        self.tcp_jit = []
        
        self.filename = _filename
        #read in the file line by line
        fileIn = open(self.filename, 'r')
        #if not fileIn:
        #   print "Unable to open ", self.filename
        first = True
        for line in fileIn:
            #skip the first line
            if first == True:
                first = False
                continue
            
            tokens = line.split(',')
            
            self.dates.append(tokens[0])
            
            if self.type == 'udp':
                
                #strip extra char off the end for mbps
                self.udp_mbps.append(float(tokens[1][0:-1]))
                
                self.udp_losspercent.append(float(tokens[2]))

                #strip extra char off the end for jitter 
                self.udp_jit.append(float(tokens[3][0:-2]))
                
                self.udp_dup.append(float(tokens[4]))
                
                #strip the newline off reorder
                self.udp_reord.append(float(tokens[5][0:-1]))            

            elif self.type == 'tcp':
                self.tcp_mbps.append(float(tokens[1]))
                self.tcp_rtt.append(float(tokens[2]))
                self.tcp_jit.append(float(tokens[3]))
            else: #error
                print 'bad type'
                
    def getUdpLists(self):
        udpMap = {}
        udpMap['time'] = self.dates
        udpMap['mbps'] = self.udp_mbps
        udpMap['jit'] = self.udp_jit
        udpMap['dup'] = self.udp_dup
        udpMap['reord'] = self.udp_reord
        udpMap['loss'] = self.udp_losspercent
        return udpMap



class grapher(object):
    '''
    classdocs
    '''


    def __init__(self,_dataset):
        '''
        Constructor
        '''
        self.dataset = _dataset 
        
        
        
   
     
    #converts the thrulay date format, into a format
    #compatible with the MatPlotlib 'plot_date' function   
    def convertTimeList(self,thrulayDates):
        #2012-03-18_21:25:07
        convertedList = []
        for dateStr in thrulayDates:
            pdt =  time.strptime(dateStr, "%Y-%m-%d_%H-%M-%S")
            #convertedList.append(md.date2num(datetime.datetime(pdt.tm_year,pdt.tm_mon,pdt.tm_mday,pdt.tm_hour,pdt.tm_min,pdt.tm_sec)))
            convertedList.append(datetime.datetime(pdt.tm_year,pdt.tm_mon,pdt.tm_mday,pdt.tm_hour,pdt.tm_min,pdt.tm_sec))

        return convertedList           
    
    def saveGraph(self,graphType,outfilename):
        yAxisData = []
        xLabel = 'time'
        yLabel = None
        graphTitle = None
        
        #use to set sane axis limits
        yAxisUpper = None
        yAxisLower = 0 #will always be 0
        
        if (graphType == "tcp_mbps"):
            yAxisData = self.dataset.tcp_mbps
            graphTitle = "TCP bits per second VS time"
            yLabel = 'Throughput (Megabits per second)'
            yAxisUpper = 40
        elif (graphType == "tcp_rtt"):
            yAxisData = self.dataset.tcp_rtt
            graphTitle = "TCP Round Trip Time"
            yLabel = 'Round Trip Time (Seconds)'
            yAxisUpper = 2*max(yAxisData)
        elif (graphType == "tcp_jit"):
            yAxisData = self.dataset.tcp_jit
            graphTitle = "TCP Jitter"
            yLabel = 'Jitter (Seconds)'
            yAxisUpper = 2*max(yAxisData)
        elif (graphType == "udp_mbps"):
            yAxisData = self.dataset.udp_mbps
            graphTitle = "UDP Megabits per second VS time"
            yLabel = 'Throughput (Megabits per second)'
            yAxisUpper = 60
        elif (graphType == "udp_jit"):
            yAxisData = self.dataset.udp_jit
            graphTitle = "UDP Jitter"
            yLabel = 'Jitter (Seconds)'
            yAxisUpper = 2*max(yAxisData)
        elif (graphType == "udp_dup"):
            yAxisData = self.dataset.udp_dup
            graphTitle = "UDP Duplicates"
            yLabel = 'Duplicate Percentage'
            yAxisUpper = 100
        elif (graphType == "udp_reord"):
            yAxisData = self.dataset.udp_reord
            graphTitle = "UDP Reorder Percentage"
            yLabel = 'Reorder Percentage'
            yAxisUpper = 100
        elif (graphType == "udp_losspercent"):
            yAxisData = self.dataset.udp_losspercent
            graphTitle = "UDP Loss Percentage"
            yLabel = 'Loss Percentage'
            yAxisUpper = 100
            
        else:
            print "Bad graph type"
            return
        
        #Yaxis data set, as well as title and axes.
        
        timeData = self.convertTimeList(self.dataset.dates)
        plt.xlabel(xLabel)
        plt.ylabel(yLabel)
        plt.title(graphTitle)
        plt.fmt_xdata = md.DateFormatter("%Y-%m-%d_%H-%M-%S")
        plt.plot_date(timeData, yAxisData,'bD-',xdate=True,ydate=False)
        plt.axis([timeData[0],timeData[-1],yAxisLower,yAxisUpper])
        

        fig = plt.gcf()
        fig.autofmt_xdate()

        plt.savefig(outfilename)
        print outfilename," saved"
        #clear the plot
        plt.clf()
        
    def generateReport(self,outDir):
        timeStr = str(datetime.datetime.now())
        header = '<html>'
        footer = '</body></html>'
        