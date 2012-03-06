/*
 * server.h -- thrulay library, server API.
 * 
 * Written by Stanislav Shalunov, http://www.internet2.edu/~shalunov/
 *            Bernhard Lutzmann, belu@users.sf.net
 *            Federico Montesino Pouzols, fedemp@altern.org
 *
 * @(#) $Id: server.h,v 1.1.2.9 2006/08/20 18:06:19 fedemp Exp $
 * 
 * Copyright 2003, 2006, Internet2.
 * Legal conditions are in file LICENSE
 * (MD5 = ecfa50d1b0bfbb81b658c810d0476a52).
 */

/**
 * @file server.h
 * @ingroup server
 * @short thrulay server API.
 **/

#ifndef THRULAY_SERVER_H_INCLUDED
#define THRULAY_SERVER_H_INCLUDED

/**
 * @defgroup server thrulay server library
 *
 * @{
 **/

#include <stdint.h>

#define THRULAY_VERSION		"thrulay/2"
#define THRULAY_DEFAULT_WINDOW  4194304
#define THRULAY_DEFAULT_SERVER_TCP_PORT		5003

/**
 * Logging alternatives
 **/
typedef enum {
	LOGTYPE_SYSLOG,
	LOGTYPE_STDERR
} thrulay_server_logtype_t;

/**
 * Initialize thrulay server.
 * 
 * @param log_type Where to output logs to.
 * @param reporting_verbosity Verbosity of metrics reports.
 *
 * @return Return code.
 * @retval 0 On success.
 * @retval <0 If an error occurred (use thrulay_server_strerror to get
 * a description).
 **/
int
thrulay_server_init(thrulay_server_logtype_t log_type, int reporting_verbosity);

/**
 * Put the thrulay server to listen on a port.
 *
 * @param port TCP port to listen for incoming thrulay clients.
 * @param window Server window size. The THRULAY_DEFAULT_WINDOW value can be
 * used.
 *
 * @return Return code.
 * @retval 0 On success.
 * @retval <0 If an error occurred (use thrulay_server_strerror to get
 * a description).
 **/
int
thrulay_server_listen(int port, int window);

/**
 * Run main service loop for incoming clients. This is intended as a
 * high level function that will serve as many clients as
 * specified. For each client, a fork() is done and the children runs
 * thrulay_server_process_client().
 *
 * thrulay_server_start(0, NULL) will take care of clients forever.
 *
 * @param max_clients number of clients to serve. After max_clients
 * have been accepted, the server will stop.
 * @param mcast_address address of a multicast group to join to.
 *
 *
 * @return Return code.
 * @retval 0 On success.
 * @retval <0 If an error occurred (use thrulay_server_strerror to get
 * a description).
 **/
int
thrulay_server_start(uint32_t max_clients, const char *const mcast_address);

/**
 * Process a thrulay connection accepted on socket fd. This is a lower
 * level function that attends a single thrulay client request on
 * socket fd.
 *
 * @param fd socket where the connection has been accepted.
 *
 * @return Return code.
 * @retval 0 On success.
 * @retval <0 If an error occurred (use thrulay_server_strerror to get
 * a description).
 **/
int
thrulay_server_process_client(int fd);

/**
 * Add an address to the list of allowed hosts.
 *
 * @param address Network address to add.
 *
 * @return Return code.
 * @retval 0 On success.
 * @retval <0 If an error occurred (use thrulay_server_strerror to get
 * a description).
 **/
int 
acl_allow_add (const char *const address);

/**
 * Get a description of an error code returned by any of the thrulay
 * server functions.
 *
 * @return Description of last error in the thrulay server.
 * @retval NULL if an error occurrs, the error is unknown or a wrong
 * error code is given.
 **/
const char *
thrulay_server_strerror(int errorcode);

/** @} */

#endif /* #ifndef THRULAY_SERVER_H_INCLUDED */
