#ifndef _PIPE_H
#define _PIPE_H

#include "types.h"

#ifndef ASM

/* Creates a new pipe */
__cdecl int pipe_pipe(int *readfd, int *writefd);

#endif /* ASM */

#endif /* _PIPE_H */
