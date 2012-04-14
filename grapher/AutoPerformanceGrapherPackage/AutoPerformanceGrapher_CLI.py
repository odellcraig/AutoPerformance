'''
Created on Mar 5, 2012

@author: cody
'''
from AutoPerformanceGrapher import dataset, grapher

if __name__ == '__main__':
    print "hello"
    
    d = dataset('udp','/home/cody/AutoPerformance/data/Examples/Upstream/Summary_Udp.csv')
    g = grapher(d)
    
    g.saveGraph('udp_mbps','/home/cody/test/Test_mbps')
    g.saveGraph('udp_jit','/home/cody/test/Test_jit')
    g.saveGraph('udp_dup','/home/cody/test/Test_dup')
    g.saveGraph('udp_reord','/home/cody/test/Test_reord')
    g.saveGraph('udp_losspercent','/home/cody/test/Test_loss')
    
    d2 = dataset('tcp','/home/cody/AutoPerformance/data/Examples/Upstream/Summary_Tcp.csv')
    g2 = grapher(d2)
    
    g2.saveGraph('tcp_mbps','/home/cody/test/TestTCP_mbps')
    g2.saveGraph('tcp_rtt','/home/cody/test/TestTCP_rtt')
    g2.saveGraph('tcp_jit','/home/cody/test/TestTCP_jit')


    
    print "Done"
    
