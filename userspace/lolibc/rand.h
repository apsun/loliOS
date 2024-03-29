#ifndef _LOLIBC_RAND_H
#define _LOLIBC_RAND_H

/* Seeds the random number generator */
void srand(unsigned int seed);

/* Generates a random number in [0, 2^32) */
unsigned int urand(void);

/* Generates a random number in [0, 2^31) */
int rand(void);

#endif /* _LOLIBC_RAND_H */
