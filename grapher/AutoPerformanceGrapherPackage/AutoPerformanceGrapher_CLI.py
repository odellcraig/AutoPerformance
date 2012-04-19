'''
Created on Mar 5, 2012

@author: cody
'''
import sys
from AutoPerformanceGrapher import dataset, grapher

if __name__ == '__main__':
    usage = "Usage: AutoPerformanceGrapher.py -t tcpInputDataFile -u udpInputDataFile  outputDirectory"
    MIN_ARG_COUNT = 5
    
    if(len(sys.argv) < MIN_ARG_COUNT):
        print "AutoPerformance: too few arguments.\n"
        print usage
        sys.exit(1)
    
    if (sys.argv[1] == '-t'):
        #tcp input file
        tcpDataIn = sys.argv[2]
    elif (sys.argv[1] == '-u'):
        udpDataIn = sys.argv[2]
        
    if (sys.argv[3] == '-t'):
        #tcp input file
        tcpDataIn = sys.argv[4]
    elif (sys.argv[3] == '-u'):
        udpDataIn = sys.argv[4]
    
    outDir = sys.argv[5]
        
    d = dataset('tcp',tcpDataIn)
    g = grapher(d)
    
    g.saveGraph('tcp_mbps',outDir + 'tcp_throughput')
    g.saveGraph('tcp_rtt', outDir + 'tcp_rtt')
    g.saveGraph('tcp_jit', outDir + 'tcp_jit')
    
    #UDP time!
    d = dataset('udp',udpDataIn)
    g = grapher(d)
    
    g.saveGraph('udp_mbps', outDir + 'udp_mbps')
    g.saveGraph('udp_jit', outDir + 'udp_jit')
    g.saveGraph('udp_dup', outDir + 'udp_dup')
    g.saveGraph('udp_reord',outDir + 'udp_reord')
    g.saveGraph('udp_losspercent',outDir + 'udp_loss')
    
    #Generate Reports here.
    #use the output dir as source of data as well as where to put the report.html
    g.generateReport(outDir)
    print "Done"
    
