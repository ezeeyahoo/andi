/**
 * @file
 * @brief This file contains various distance methods.
 */
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include "esa.h"
#include "divergence.h"
#include "global.h"
#include "process.h"

#include <RMQ_1_n.hpp>
#include <RMQ_succinct.hpp>

/**
 * This is a neat hack for dealing with matrices.
 */
#define D( X, Y) (D[ (X)*n + (Y) ])

/**
 * The dist function computes the distance between a subject and a query
 * @return Number of jumps which is related to the number of mutations
 * @param C - The enhanced suffix array of the subject.
 * @param query - The actual query string.
 * @param ql - The length of the query string. Needed for speed reasons.
 */
double dist( const esa_t *C, char *query, size_t ql){
	size_t jumps = 0;
	size_t idx = 0;
	saidx_t l;
	lcp_inter_t inter;
	
	while( idx < ql ){
		inter = getLCPInterval( C, query + idx, ql - idx);
		//saidx_t l = longestMatch( C, query + idx, ql - idx);
		l = inter.l;
		if( l == 0 ) break;
		
		// TODO: remove this from production code.
		if( FLAGS & F_EXTRA_VERBOSE ){
			fprintf( stderr, "idx: %ld, l: %d\n", idx, l);
		}
		
		jumps++;
		idx += l + 1; // skip the mutation
	}

	// avoid NaN
	if( jumps <= 1){
		jumps = 2;
	}
	
	// TODO: remove this from production code.
	if( FLAGS & F_VERBOSE ){
		fprintf( stderr, "jumps: %ld, homol: %ld\n", jumps, ql);
	}
	
	return (double)(jumps-1)/(double)ql;
}

/**
 * @brief Calculates the log_2 of a given integer.
 */
static inline size_t log2( size_t num){
	size_t res = 0;
	while( num >>= 1){
		res++;
	}
	return res;
}

/**
 * @brief Calculates the log_4 of a given integer.
 */
static inline size_t log4( size_t num){
	return log2( num) >> 1;
}

/**
 * @brief Divergence estimation using the anchor technique.
 *
 * The dist_anchor() function estimates the divergence between two
 * DNA sequences. The subject is given as an ESA, whereas the query
 * is a sinple string. This function then looks for *anchors* -- long
 * substring that exist in both sequences. Then it manually checks for
 * mutations between those anchors.
 * 
 * @return An estimate for the number of mutations within homologous regions.
 * @param C - The enhanced suffix array of the subject.
 * @param query - The actual query string.
 * @param ql - The length of the query string. Needed for speed reasons.
 */
double dist_anchor( const esa_t *C, const char *query, size_t query_length){
	size_t snps = 0; // Total number of found SNPs
	size_t homo = 0; // Total number of homologous nucleotides.
	
	lcp_inter_t inter;
	
	size_t last_pos_Q = 0;
	size_t last_pos_S = 0;
	size_t last_length = 0;
	// This variable indicates that the last anchor was the right anchor of a pair.
	size_t last_was_right_anchor = 0;
	
	size_t this_pos_Q = 0;
	size_t this_pos_S;
	size_t this_length;
	
	// TODO: remove this from production code.
	size_t num_right_anchors = 0;
	
	// Iterate over the complete query.
	while( this_pos_Q < query_length){
		inter = getLCPInterval( C, query + this_pos_Q, query_length - this_pos_Q);
		this_length = inter.l;
		if( this_length == 0) break;
		
		/* TODO: evaluate the result of different conditions */
		if( inter.i == inter.j  
			&& this_length >= 2 * log4(query_length) )
		{
			// We have reached a new anchor.
			this_pos_S = C->SA[ inter.i];
			
			// Check if this can be a right anchor to the last one.
			if( this_pos_Q - last_pos_Q == this_pos_S - last_pos_S ){
				num_right_anchors++;
			
				// Count the SNPs in between.
				size_t i;
				for( i= 0; i< this_pos_Q - last_pos_Q; i++){
					if( C->S[ last_pos_S + i] != query[ last_pos_Q + i] ){
						snps++;
					}
				}
				homo += this_pos_Q - last_pos_Q;
				last_was_right_anchor = 1;
			} else {
				if( last_was_right_anchor){
					// If the last was a right anchor, but with the current one, we 
					// cannot extend, then add its length.
					homo += last_length;
				}
				
				last_was_right_anchor = 0;
			}
			
			// Cache values for later
			last_pos_Q = this_pos_Q;
			last_pos_S = this_pos_S;
			last_length= this_length;
		}
		
		// Advance
		this_pos_Q += this_length + 1;
	}
	
	// We might miss a few nucleotides if the last anchor was also a right anchor.
	if( last_was_right_anchor ){
		homo += last_length;
	}
	
	// Avoid NaN.
	if ( snps <= 2) snps = 2;
	if ( homo <= 3) homo = 3;
	
	// TODO: remove this from production code.
	if( FLAGS & F_VERBOSE ){
		fprintf( stderr, "snps: %lu, homo: %lu\n", snps, homo);
		fprintf( stderr, "number of right anchors: %lu\n", num_right_anchors);
	}
	
	return (double)snps/(double)homo;
}

/**
 * @brief Computes the distance matrix.
 *
 * The distMatrix() populates the D matrix with computed distances. It allocates D and
 * filles it with useful values, but the caller has to free it!
 * @return The distance matrix
 * @param sequences An array of pointers to the sequences.
 * @param n The number of sequences.
 */
double *distMatrix( seq_t* sequences, int n){
	double *D = (double*)malloc( n * n * sizeof(double));
	assert(D);
	
	double d;
	
	int i;

	#pragma omp parallel for num_threads( THREADS)
	for(i=0;i<n;i++){
		esa_t E = {NULL,NULL,NULL,NULL,0,NULL};
		
		// initialize the enhanced suffix array
		if( FLAGS & F_SINGLE ){
			E.S = (const char*) sequences[i].S;
			E.len = sequences[i].len;
		} else {
			E.S = (const char*) sequences[i].RS;
			E.len = sequences[i].RSlen;
		}
		
		int result;

		result = compute_SA( &E);
		assert( result == 0); // zero errors
		result = compute_LCP_PHI( &E);
		assert( result == 0);
	
		E.rmq_lcp = new RMQ_succinct(E.LCP, E.len);
		
		// now compare every other sequence to i
		int j;
		for(j=0; j<n; j++){
			if( j == i) {
				D(i,j) = 0.0;
				continue;
			}
			
			// TODO: remove this from production code, or provide a nicer
			// progress indicator.
			if( FLAGS & F_VERBOSE ){
				#pragma omp critical
				{
					fprintf( stderr, "comparing %d and %d\n", i, j);
				}
			}

			size_t ql = sequences[j].len;
			
			if( STRATEGY == S_SIMPLE ){
				d = dist( &E, sequences[j].S, ql);
			} else if( STRATEGY == S_ANCHOR ){
				d = dist_anchor( &E, sequences[j].S, ql);
			}
			
			if( !(FLAGS & F_RAW)){
				/*  Our shustring method might miss a mutation or two. Hence we need to correct  
					the distance using math. See Haubold, Pfaffelhuber et al. (2009) */
				if( STRATEGY == S_SIMPLE ){
					d = divergence( 1.0/d, E.len, 0.5, 0.5);
				}
				d = -0.75 * log(1.0- (4.0 / 3.0) * d ); // jukes cantor
			}
			D(i,j) = d;
		}
		
		delete E.rmq_lcp;
		free( E.SA);
		free( E.ISA);
		free( E.LCP);
	}
	
	return D;
}

/**
 * @brief Prints the distance matrix.
 * @param sequences An array of pointers to the sequences.
 * @param n The number of sequences.
 */
void printDistMatrix( seq_t* sequences, int n){
	int i, j;

	// initialise the sequences
	#pragma omp parallel for num_threads( THREADS)
	for( i=0;i<n;i++){
		if( sequences[i].S == NULL){
			fprintf(stderr, "missing sequence %d\n", i);
			exit(1);
		}
		sequences[i].len = strlen( sequences[i].S);
		
		// double stranded comparision?
		if( !(FLAGS & F_SINGLE) ){
			sequences[i].RS = catcomp( sequences[i].S, sequences[i].len);
			sequences[i].RSlen = 2 * sequences[i].len + 1;
		}
	}
	
	// compute the distances
	double *D = distMatrix( sequences, n);
	
	// print the results
	printf("%d\n", n);
	for( i=0;i<n;i++){
		printf("%8s", sequences[i].name);
		for( j=0;j<n;j++){
			printf(" %1.4lf", D(i,j));
		}
		printf("\n");
	}
	
	free(D);
}


