#ifndef _LOOPBACK_H
#define _LOOPBACK_H

#ifndef ASM

/* Delivers any enqueued loopback packets */
void loopback_deliver(void);

/* Initializes the loopback interface */
void loopback_init(void);

#endif /* ASM */

#endif /* _LOOPBACK_H */
