#ifndef _MT19937_H
#define _MT19937_H

#ifndef ASM

/* Seeds the random number generator */
void srand(unsigned int seed);

/* Generates a random number in [0, 2^32) */
unsigned int urand(void);

/* Generates a random number in [0, 2^31) */
int rand(void);

#endif

#endif /* _MT19937_H */
