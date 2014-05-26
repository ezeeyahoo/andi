/**
 * @file
 * @brief This header contains the declarations for functions in esa.c.
 *
 */
#ifndef _ESA_H_
#define _ESA_H_

#include <divsufsort.h>
#include <RMQ.hpp>


/** 
 * @brief The ESA type.
 * 
 * This structure holds arrays and objects associated with an enhanced
 * suffix array (ESA).
 */
typedef struct {
	/** The base string from which the ESA was generated. */
	const char *S;
	/** The actual suffix array with indices into S. */
	saidx_t *SA;
	/** The inverse suffix array holds at position `i` the index at
		which the suffix `S[i]` is positioned in the SA. */
	saidx_t *ISA;
	/** The LCP holds the number of letters up to which a suffix `S[SA[i]]`
		equals `S[SA[i-1]]`. Hence the name longest common prefix. For `i = 0`
		and `i = len` the LCP value is -1. */
	saidx_t *LCP;
	/** The length of the string S. */
	saidx_t len;
	/** A reference to an object for range minimum queries. */
	RMQ *rmq_lcp;
} esa_t;

/**
 * @brief Represents intervals on natural numbers.
 *
 * This struct can be used to represent intervals on natural numbers. The
 * member `i` should coincide with the lower bound whereas `j` is the upper
 * bound. Both bounds are inclusive. So if `i == j` the interval contains
 * exactly one element, namely `i`. To represent an empty interval please
 * use `i == j == -1`. Other variants, such as `i == j == -2` can be used
 * to indicate an error.
 * Variables of this type are often called `ij`.
 */
typedef struct {
	saidx_t i;
	saidx_t j;
} interval;

/**
 * @brief Represents LCP-Intervals.
 *
 * This struct is used to represent LCP-intervals. In addition to the rules
 * in ::interval regarding `i` and `j`, l should always be non-negative. It
 * can be used to hold the length of the commen prefix in an interval.
 */
typedef struct {
	saidx_t l, i, j;
} lcp_inter_t;


int compute_SA( esa_t *c);
int compute_LCP( esa_t *c);
int compute_LCP_PHI( esa_t *c);
saidx_t longestMatch( const esa_t *C, const char *query, int qlen);
interval  exactMatch( const esa_t *C, const char *query);

interval getInterval( const esa_t *C, const interval ij, char a);
lcp_inter_t getLCPInterval( const esa_t *C, const char *query, size_t qlen);

#endif

