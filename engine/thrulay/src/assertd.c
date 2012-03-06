/*
 * assertd.c -- assert() for daemons.
 * 
 * Written by Stanislav Shalunov, http://www.internet2.edu/~shalunov/
 * 
 * Copyright 2003, Internet2.
 * Legal conditions are in file LICENSE
 * (MD5 = ecfa50d1b0bfbb81b658c810d0476a52).
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#include <stdlib.h>
#include "assertd.h"
#include "util.h"
#include "rcs.h"

RCS_ID("@(#) $Id: assertd.c,v 1.2.2.3 2006/08/15 18:54:43 fedemp Exp $")

void __attribute__((noreturn))
assertd_failure(const char *file, int line, const char *e)
{
	syslog(LOG_ALERT,
	       "assertion \"%s\" failed: file \"%s\", line %d, aborting",
	       e, file, line);
	fprintf(stderr,
		"assertion \"%s\" failed: file \"%s\", line %d\n",
		e, file, line);
	abort();
	/* NOTREACHED */
}
