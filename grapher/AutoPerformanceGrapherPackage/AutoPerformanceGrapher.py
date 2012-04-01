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
                self.udp_mbps.append(tokens[1])
                self.udp_losspercent.append(tokens[2])
                self.udp_jit.append(tokens[3])
                self.udp_dup.append(tokens[4])
                self.udp_reord.append(tokens[5])            

            elif self.type == 'tcp':
                self.tcp_mbps.append(tokens[1])
                self.tcp_rtt.append(tokens[2])
                self.tcp_jit.append(tokens[3])
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





'''
Loads and allows access to records from a UDP test summary file format
'''
class UDPdataset(dataset):
    
    pass

'''
Loads and allows access to records from a TCP test summary file format
'''
class TCPdataset(dataset):
    
    pass



class grapher(object):
    '''
    classdocs
    '''


    def __init__(self,_dataset):
        '''
        Constructor
        '''
        self.dataset = _dataset 
        self.title = "Default Graph Title"
        self.xlabel = "X axis"
        self.ylabel = "y axis"
        
        #read in the file specified and parse the data

        pass
    
    
    def saveThroughput(self,_OutputFilename):
        
        pass
    
    def saveJitter(self,_OutputFilename):
        
        pass
    
    
    def setTitle(self,title):
        self.title
        pass
    
    def setAxisX(self,_label):
        self.ylabel = _label
        pass
    
    def setXisY(self,_label):
        self.ylabel = _label
        pass