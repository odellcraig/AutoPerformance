'''
Created on Mar 5, 2012

@author: cody
'''
import matplotlib.pyplot as plt


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
        days = 1
        for dateStr in thrulayDates:
            convertedList.append(days)
            days += 1
            
        return convertedList           
    
    def saveGraph(self,graphType,outfilename):
        yAxisData = []
        xLabel = 'time'
        yLabel = None
        graphTitle = None
        
        if (graphType == "tcp_mbps"):
            yAxisData = self.dataset.tcp_mbps
            graphTitle = "TCP Megabits per second VS time"
            yLabel = 'Megabits'
        elif (graphType == "tcp_rtt"):
            yAxisData = self.dataset.tcp_rtt
            graphTitle = "TCP Round Trip Time"
            yLabel = 'whatever rtt unit is'
        elif (graphType == "tcp_jit"):
            yAxisData = self.dataset.tcp_jit
            graphTitle = "TCP Jitter"
            yLabel = 'whatever jitter unit is'
        elif (graphType == "udp_mbps"):
            yAxisData = self.dataset.udp_mbps
            graphTitle = "UDP Megabits per second VS time"
            yLabel = 'Megabits'
        elif (graphType == "udp_jit"):
            yAxisData = self.dataset.udp_jit
            graphTitle = "UDP Jitter"
            yLabel = 'whatever Jitter unit is'
        elif (graphType == "udp_dup"):
            yAxisData = self.dataset.udp_dup
            graphTitle = "UDP Duplicates"
            yLabel = 'Dupe count units'
        elif (graphType == "udp_reord"):
            yAxisData = self.dataset.udp_reord
            graphTitle = "UDP Reorder Percentage"
            yLabel = 'Reorder whatever units'
        elif (graphType == "udp_losspercent"):
            yAxisData = self.dataset.udp_losspercent
            graphTitle = "UDP Loss Percentage"
            yLabel = 'Loss Percent'
        else:
            print "Bad graph type"
            return
        
        #Yaxis data set, as well as title and axes.
        
        timeData = self.convertTimeList(self.dataset.dates)
        
        #fig = plt.figure()
        #plot = fig.add_subplot(111,aspect='equal')
        #plot.xlabel(xLabel)
        #plot.ylabel(yLabel)
        #plot.title(graphTitle)
        #plot.plot(timeData,yAxisData)
        #plot.savefig(outfilename)
        plt.xlabel(xLabel)
        plt.ylabel(yLabel)
        plt.title(graphTitle)
        plt.axis([min(timeData),max(timeData),min(yAxisData),max(yAxisData)])
        plt.plot(timeData, yAxisData)
        plt.savefig(outfilename)
        print outfilename," saved"
        #clear the plot
        
    
   
        