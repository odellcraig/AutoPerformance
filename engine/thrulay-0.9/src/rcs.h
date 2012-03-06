/*
 * rcs.h -- a way to put RCS IDs into executables.
 *
 * Written by Stanislav Shalunov, http://www.internet2.edu/~shalunov/
 * 
 * @(#) $Id: rcs.h,v 1.1.1.1 2005/10/02 01:18:09 hliu Exp $ 
 *
 * Copyright 2003, Internet2.
 * Legal conditions are in file LICENSE
 * (MD5 = ecfa50d1b0bfbb81b658c810d0476a52).
 */

#ifndef RCS_H_INCLUDED
#define RCS_H_INCLUDED

#define RCS_ID(id) static const char *rcs_id = id;	\
	static const char *				\
	f_rcs_id(const char *s)				\
	{						\
		if (s) return s;			\
		else return f_rcs_id(rcs_id);		\
	}

#endif /* ! RCS_H_INCLUDED */
