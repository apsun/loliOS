#if UBSAN_ENABLED

#include <attrib.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    const char *file;
    int line;
    int col;
} ubsan_source_loc_t;

#define MAKE_UBSAN_HANDLER(name, ...)              \
    __noreturn __used void                         \
    __ubsan_handle_##name(void *s, ## __VA_ARGS__) \
    {                                              \
        ubsan_source_loc_t *source = s;            \
        fprintf(                                   \
            stderr,                                \
            "UBSan detected %s at %s:%d:%d\n",     \
            #name,                                 \
            source->file,                          \
            source->line,                          \
            source->col);                          \
        abort();                                   \
    }

MAKE_UBSAN_HANDLER(add_overflow, void *lhs, void *rhs)
MAKE_UBSAN_HANDLER(builtin_unreachable)
MAKE_UBSAN_HANDLER(divrem_overflow, void *lhs, void *rhs)
MAKE_UBSAN_HANDLER(mul_overflow, void *lhs, void *rhs)
MAKE_UBSAN_HANDLER(negate_overflow, void *val)
MAKE_UBSAN_HANDLER(out_of_bounds, void *index)
MAKE_UBSAN_HANDLER(pointer_overflow, void *base, void *result)
MAKE_UBSAN_HANDLER(shift_out_of_bounds, void *lhs, void *rhs)
MAKE_UBSAN_HANDLER(sub_overflow, void *lhs, void *rhs)
MAKE_UBSAN_HANDLER(type_mismatch_v1, void *ptr)

#endif /* UBSAN_ENABLED */
