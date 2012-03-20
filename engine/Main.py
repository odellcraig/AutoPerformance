'''
Created on Mar 6, 2012

@author: codell
'''
from AutoPerformanceClient import AutoPerformanceClient, \
    AutoPerformanceClientConfig
from AutoPerformanceServer import AutoPerformanceServer
import socket

        

def client(ip, port, message):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((ip, port))
    try:
        print "Now connected"
    finally:
        print "Close the socket..."
        sock.close()


if __name__ == "__main__":
    # Port 0 means to select an arbitrary unused port
    HOST, PORT = "localhost", 5003

    #Start the server
    server = AutoPerformanceServer(HOST,PORT)
    try:
        server.startServer() 
        clientConfig = AutoPerformanceClientConfig()
        clientConfig.duration = 2
        clientConfig.frameSize = 1518
        clientConfig.dscp = 0
        clientConfig.numStreams = 1
        clientConfig.udpRateString = "40M"
        clientConfig.host = 'localhost'
        clientConfig.port = 5003
        clientConfig.outputDirectory = "../data"
        client = AutoPerformanceClient(clientConfig)
        
        #Run the test
        client.run()
        print "Closing client connection."
        client.close()
    except:
        print "An exception has occurred."
    finally:
        print "Stopping the server."
        server.stopServer()
    
    print "Now ending all things."
    
    