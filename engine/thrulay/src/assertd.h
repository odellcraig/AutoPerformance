/*
 * assertd.h -- assert() for daemons, headers.
 * 
 * Written by Stanislav Shalunov, http://www.internet2.edu/~shalunov/
 * 
 * @(#) $Id: assertd.h,v 1.1.1.1 2005/10/02 01:18:09 hliu Exp $
 * 
 * Copyright 2003, Internet2.
 * Legal conditions are in file LICENSE
 * (MD5 = ecfa50d1b0bfbb81b658c810d0476a52).
 */

/* This file can be usefully included multiple times, with or without
   NDEBUG defined. */

#undef assert
#ifdef NDEBUG
#define	assert(e)	0
#else
#define	assert(e)	((e)? (void)0: assertd_failure(__FILE__, __LINE__, #e))
#endif

void
assertd_failure(const char *, int, const char *);
