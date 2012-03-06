/*
 * reporting.h -- performance metrics reporting, headers.
 *
 * Written by Stanislav Shalunov, http://www.internet2.edu/~shalunov/
 *            Bernhard Lutzmann, belu@users.sf.net
 *            Federico Montesino Pouzols, fedemp@altern.org
 *
 * @(#) $Id: reporting.h,v 1.1.2.5 2006/08/20 18:06:19 fedemp Exp $
 *
 * Copyright 2005, 2006, Internet2.
 * Legal conditions are in file LICENSE
 * (MD5 = ecfa50d1b0bfbb81b658c810d0476a52).
 */

#ifndef THRULAY_REPORTING_H_INCLUDED
#define THRULAY_REPORTING_H_INCLUDED

/**
 * @file reporting.h
 *
 * @short metrics computation and reporting (header).
 **/

/**
 * Initialize reordering.
 *
 *
 * @return
 * @retval 0 on success.
 * @retval -1 on error (failed calloc()).
 */
int
reordering_init(uint64_t max);

/**
 * Update reordering accounting for next received packet.
 *
 * @param packet_sqn packet sequence number.
 *
 * @return
 * @retval 0 on success.
 */
int
reordering_checkin(uint64_t packet_sqn);

/**
 * Get j-1 reordering.
 *
 * @return j-1 reordering.
 */
double
reordering_output(uint64_t j);

/**
 * Deinitialize reordering.
 */
void
reordering_exit (void);

/**
 * Initialize duplication.
 *
 * @return
 * @retval 0 on success.
 * @retval -1 on error (failed malloc()).
 */
int
duplication_init(uint64_t npackets);

/**
 * Update duplication accounting for next received packet and check
 * whether it is duplicated.
 *
 * @param packet_sqn packet sequence number.
 *
 * @return 
 * @retval 0 if not duplicated.
 * @retval 1 if duplicated.
 */
int
duplication_check(uint64_t packet_sqn);

/**
 * Deinitialize duplication.
 */
void
duplication_exit(void);

/*
 * Quantiles
 *
 * References:
 * [1] Manku, Rajagopalan, Lindsay: "Approximate Medians and other Quantiles
 *     in One Pass and with Limited Memory",
 *     http://www-db.stanford.edu/~manku/papers/98sigmod-quantiles.pdf
 */

#define QUANTILE_EPS	0.005

/**
 * Initialize quantiles. This sets values for `quantile_b' and `quantile_k'.
 * Then allocates memory for quantile buffers and input buffer.
 *
 * @param max_seq Maximum number of sequences that should be handled
 * in parallel.
 *
 * @return 
 * @retval 0 on success.
 * @retval -1 on error (failed malloc()).
 *
 * See section 4.5 "The New Algorithm" of paper [1] (see above). 
 **/
int
quantile_init(uint16_t max_seq, double eps, uint64_t N);

int 
quantile_init_seq(uint16_t seq);

/**
 * Update quantiles for next value.
 *
 * @param seq Sequence index.
 *
 * @return
 * @retval 0 on success.
 * @retval -1 quantiles not initialized.
 * @retval -2 need an empty buffer for new operation.
 * @retval -3 bad input sequence length in new operation.
 * @retval -4 not enough buffers for collapse operation.
 * @retval -5 bad sequence index.
 */
int
quantile_value_checkin(uint16_t seq, double value);

/*
 * Perform last algorithm operation with a possibly not full
 * input buffer.
 *
 * @param seq Sequence index.
 *
 * @return
 * @retval 0 on success.
 * @retval -1 quantiles not initialized.
 * @retval -2 need an empty buffer for new operation.
 * @retval -3 bad input sequence length in new operation.
 * @retval -4 not enough buffers for collapse operation.
 */
int
quantile_finish(uint16_t seq);

/* Implementation of OUTPUT operation from section 3.3 of paper [1].
 *
 * Takes c>= full input buffers in linked list. Fourth parameter is set
 * to phi-quantile.
 *
 * @param seq Sequence index
 * 
 * @return
 * @retval 0 on success.
 * @retval -1 bad number of full buffers.
 */
int
quantile_output(uint16_t seq, uint64_t npackets, double phi, double *result);

/**
 * Deinitializes a quantile sequence. Free memory of quantile buffers
 * and input buffer.
 *
 * @param seq Sequence index.
 **/
void
quantile_exit_seq(uint16_t seq);

/**
 * Deinitialize quantiles. Deinitializes all sequences and frees all
 * structures.
 **/
void
quantile_exit(void);

#endif /* ifndef THRULAY_REPORTING_H_INCLUDED */
