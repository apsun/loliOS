#ifndef _LOLIBC_ATTRIB_H
#define _LOLIBC_ATTRIB_H

#define __cdecl __attribute__((cdecl))
#define __used __attribute__((used))
#define __unused __attribute__((unused))
#define __noinline __attribute__((noinline))
#define __always_inline __attribute__((always_inline))
#define __packed __attribute__((packed))
#define __aligned(x) __attribute__((aligned(x)))
#define __fallthrough __attribute__((fallthrough))
#define __noreturn __attribute__((noreturn))
#define __unreachable __builtin_unreachable()

#endif /* _LOLIBC_ATTRIB_H */
