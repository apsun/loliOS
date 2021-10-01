#if UBSAN_ENABLED

#include <stdio.h>
#include <stdlib.h>

typedef struct {
    const char *file;
    int line;
    int col;
} ubsan_source_loc_t;

#define MAKE_UBSAN_HANDLER(name)                      \
    __attribute__((noreturn, used)) void              \
    __ubsan_handle_##name(ubsan_source_loc_t *source) \
    {                                                 \
        fprintf(                                      \
            stderr,                                   \
            "UBSan detected %s at %s:%d:%d\n",        \
            #name,                                    \
            source->file,                             \
            source->line,                             \
            source->col);                             \
        abort();                                      \
    }

MAKE_UBSAN_HANDLER(add_overflow)
MAKE_UBSAN_HANDLER(builtin_unreachable)
MAKE_UBSAN_HANDLER(divrem_overflow)
MAKE_UBSAN_HANDLER(mul_overflow)
MAKE_UBSAN_HANDLER(negate_overflow)
MAKE_UBSAN_HANDLER(out_of_bounds)
MAKE_UBSAN_HANDLER(pointer_overflow)
MAKE_UBSAN_HANDLER(shift_out_of_bounds)
MAKE_UBSAN_HANDLER(sub_overflow)
MAKE_UBSAN_HANDLER(type_mismatch_v1)

#endif /* UBSAN_ENABLED */
