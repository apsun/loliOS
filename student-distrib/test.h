#ifndef _TEST_H
#define _TEST_H

#include "types.h"

#ifndef ASM

/* Executes the specified test number */
void test_execute(int32_t test_num);

/* Testing "shell" */
void test_shell(void);

#endif /* ASM */

#endif /* _TEST_H */
