'''
Created on Mar 5, 2012

@author: cody
'''
from AutoPerformanceGrapher import dataset
import matplotlib.pyplot as plt

if __name__ == '__main__':
    print "hello"
    
    d = dataset('udp','/home/cody/AutoPerformance/data/Examples/Upstream/Summary_Udp.csv')
    udpdata = d.getUdpLists()
    
    plt.plot(udpdata['dup'])
    plt.show()
    pass