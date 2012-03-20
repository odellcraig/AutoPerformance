'''
Created on Mar 3, 2012

@author: codell
'''

from AutoPerformanceEngine import AutoPerformanceEngine, AutoPerformanceConfig
import SocketServer
import socket
import threading


class AutoPerformanceThreadHandler(SocketServer.BaseRequestHandler):

    def handle(self):
        '''
        Handle the incoming connection
        '''
        configStr = self.request.recv(1024)
        config = AutoPerformanceConfig()
        config.fromString(configStr)
        engine = AutoPerformanceEngine(config)
        
        #Start our server
        try:
            engine.startServer()
        
        
            runCmd = self.request.recv(10)
            if(str(runCmd) != 'tcp'):
                print "Error: unexpected request. Aborting"
                return
            data, summaryTcp = engine.runTcp()
            self.sendTcpResults(data, summaryTcp)
            
            runCmd = self.request.recv(10)
            if(str(runCmd) != 'udp'):
                print "Error: unexpected request. Aborting"
                return
            summaryUdp = engine.runUdp()
            self.sendUdpResults(summaryUdp)
        
        finally:
            #stop server
            print "Done on server side."
            self.request.close()
            engine.stopServer()
            self.finish()
            
    
    def sendTcpResults(self,data,summary):
        self.request.sendall(str(data))
        ack = self.request.recv(10)
        if(str(ack) != "ack"):
            print "Error: Did not receive data ack."
        print "Server Now Sending Summary: ", summary
        self.request.sendall(str(summary))
        print "Server Done Sending TCP Results"

    def sendUdpResults(self,summary):
        print "Server Now Sending UDP Results"
        self.request.sendall(str(summary))
        print "Server Done Sending UDP Results"

class ThreadedTCPServer(SocketServer.ThreadingMixIn, SocketServer.TCPServer):
    def __init__(self, one, two):
        self.allow_reuse_address = True
        SocketServer.TCPServer.__init__(self, one,two)


class AutoPerformanceServer(object):
    def __init__(self,host,port):
        self.host = host
        self.port = port
        self.server = ThreadedTCPServer((self.host, self.port), AutoPerformanceThreadHandler) 
    def startServer(self):
        # Start a thread with the server -- that thread will then start one
        # more thread for each request
        self.server_thread = threading.Thread(target=self.server.serve_forever)
        # Exit the server thread when the main thread terminates
        self.server_thread.daemon = True
        self.server_thread.start()
        print "Server loop running in thread:", self.server_thread.name
    def stopServer(self):
        self.server.shutdown()
        self.server_thread.join(10)
        
    


    