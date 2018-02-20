.data
    LOCATION    = 0
    ON_CHAR     = 2
    OFF_CHAR    = 3
    ON_LENGTH   = 4
    OFF_LENGTH  = 6
    COUNTDOWN   = 8
    STATUS      = 10
    NEXT        = 12
    STRUCT_SIZE = 16
    WINDOW_SIZE = 80*25

// Pointer to head of list (initialized to NULL)
mp1_list_head:
    .long 0

// Jump table for mp1_ioctl()
mp1_ioctl_jumptable:
    .long mp1_ioctl_add
    .long mp1_ioctl_remove
    .long mp1_ioctl_find
    .long mp1_ioctl_sync

.text

.global mp1_rtc_tasklet
.global mp1_ioctl


// --------------------------------------------------------------------
// void mp1_draw_struct(mp1_blink_struct *s@ebx)
//
// Draws the struct at the appropriate location.
//
// Note: the parameter is in EBX. Otherwise, cdecl applies.
// --------------------------------------------------------------------
mp1_draw_struct:
    cmpw $0, STATUS(%ebx)
    jz mp1_draw_struct_off
mp1_draw_struct_on:
    movb ON_CHAR(%ebx), %al
    jmp mp1_draw_struct_draw
mp1_draw_struct_off:
    movb OFF_CHAR(%ebx), %al
mp1_draw_struct_draw:
    movzwl LOCATION(%ebx), %ecx
    movl vmem_base_addr, %edx
    movb %al, (%edx, %ecx, 2)
    ret


// --------------------------------------------------------------------
// void mp1_rtc_tasklet(unsigned long ignored)
//
// Updates the structures. For each struct that reaches
// the end of its countdown, toggle its state.
// --------------------------------------------------------------------
mp1_rtc_tasklet:
    pushl %ebp
    movl %esp, %ebp
    pushl %ebx

    movl mp1_list_head, %ebx
mp1_rtc_tasklet_loop:
    testl %ebx, %ebx
    jz mp1_rtc_tasklet_loop_end

    // Decrement countdown. If we reach 0...
    movw COUNTDOWN(%ebx), %ax
    decw %ax
    movw %ax, COUNTDOWN(%ebx)
    testw %ax, %ax
    jnz mp1_rtc_tasklet_loop_next

    // Toggle the status between 0 and 1
    movw STATUS(%ebx), %ax
    xorw $1, %ax
    movw %ax, STATUS(%ebx)

    // Set countdown to on_length/off_length accordingly
    test %ax, %ax
    jz mp1_rtc_tasklet_loop_to_off
mp1_rtc_tasklet_loop_to_on:
    movw ON_LENGTH(%ebx), %ax
    jmp mp1_rtc_tasklet_loop_update_countdown
mp1_rtc_tasklet_loop_to_off:
    movw OFF_LENGTH(%ebx), %ax
mp1_rtc_tasklet_loop_update_countdown:
    movw %ax, COUNTDOWN(%ebx)

    // And then draw the new state
    call mp1_draw_struct
mp1_rtc_tasklet_loop_next:
    movl NEXT(%ebx), %ebx
    jmp mp1_rtc_tasklet_loop
mp1_rtc_tasklet_loop_end:
    popl %ebx
    leave
    ret


// --------------------------------------------------------------------
// mp1_blink_struct **mp1_find_by_location(unsigned short location)
//
// Finds a struct by its location. Returns a pointer to the only
// pointer to the structure (the double-indirection is useful
// when deleting stuff). Returns NULL if no struct has the
// given location.
// --------------------------------------------------------------------
mp1_find_by_location:
    pushl %ebp
    movl %esp, %ebp
    // Note: This is a double-indirect pointer.
    // It either points to the list head, or the
    // next field of the previous struct.
    movl $mp1_list_head, %eax
mp1_find_by_location_loop:
    movl (%eax), %edx
    testl %edx, %edx
    jz mp1_find_by_location_loop_end

    // Compare location against parameter
    movw LOCATION(%edx), %cx
    cmpw %cx, 8(%ebp)
    je mp1_find_by_location_ret

    // Move to next struct in the list
    leal NEXT(%edx), %eax
    jmp mp1_find_by_location_loop
mp1_find_by_location_loop_end:
    xorl %eax, %eax
mp1_find_by_location_ret:
    leave
    ret


// --------------------------------------------------------------------
// int mp1_ioctl(unsigned long arg, unsigned long cmd)
//
// Dispatch function for the ioctl calls.
// --------------------------------------------------------------------
mp1_ioctl:
    movl 8(%esp), %eax
    cmpl $4, %eax
    jae mp1_ioctl_err
    jmp *mp1_ioctl_jumptable(, %eax, 4)
mp1_ioctl_err:
    movl $-1, %eax
    ret


// --------------------------------------------------------------------
// int mp1_ioctl_add(mp1_blink_struct *arg)
//
// Adds a new struct into the list. Returns -1
// if the argument is invalid, and 0 otherwise.
// --------------------------------------------------------------------
mp1_ioctl_add:
    pushl %ebp
    movl %esp, %ebp
    pushl %ebx

    // Allocate kernel memory for the struct
    pushl $STRUCT_SIZE
    call mp1_malloc
    addl $4, %esp
    testl %eax, %eax
    jz mp1_ioctl_add_err
    movl %eax, %ebx

    // Copy from userspace to kernel
    pushl $STRUCT_SIZE
    movl 8(%ebp), %eax
    pushl %eax
    pushl %ebx
    call mp1_copy_from_user
    addl $12, %esp
    testl %eax, %eax
    jnz mp1_ioctl_add_copy_err

    // Ensure location is valid
    cmpw $WINDOW_SIZE, LOCATION(%ebx)
    jae mp1_ioctl_add_copy_err

    // Countdown is initially set to on_length
    movw ON_LENGTH(%ebx), %ax
    movw %ax, COUNTDOWN(%ebx)

    // Status is initially set to 1
    movw $1, STATUS(%ebx)

    // Next pointer is set to the original head...
    movl mp1_list_head, %eax
    movl %eax, NEXT(%ebx)

    // And the head is set to the new struct
    movl %ebx, mp1_list_head

    // And finally we draw the character
    call mp1_draw_struct

    xorl %eax, %eax
    jmp mp1_ioctl_add_ret
mp1_ioctl_add_copy_err:
    pushl %ebx
    call mp1_free
    addl $4, %esp
mp1_ioctl_add_err:
    movl $-1, %eax
mp1_ioctl_add_ret:
    popl %ebx
    leave
    ret


// --------------------------------------------------------------------
// int mp1_ioctl_remove(unsigned long location)
//
// Removes a struct with the given location from the list.
// Returns -1 if no struct with the given location exists,
// and 0 otherwise.
// --------------------------------------------------------------------
mp1_ioctl_remove:
    pushl %ebp
    movl %esp, %ebp

    // First we find the struct...
    movl 8(%ebp), %eax
    pushl %eax
    call mp1_find_by_location
    addl $4, %esp
    testl %eax, %eax
    jz mp1_ioctl_remove_not_found

    // Then we replace it with the next struct
    // in the list...
    movl (%eax), %edx
    movl NEXT(%edx), %ecx
    movl %ecx, (%eax)

    // Then we free it.
    pushl %edx
    call mp1_free
    addl $4, %esp

    xorl %eax, %eax
    jmp mp1_ioctl_remove_ret
mp1_ioctl_remove_not_found:
    movl $-1, %eax
mp1_ioctl_remove_ret:
    leave
    ret


// --------------------------------------------------------------------
// int mp1_ioctl_find(mp1_blink_struct *arg)
//
// Finds a struct with the same location as the argument.
// If found, the struct is copied to the struct that arg
// points to, and 0 is returned. Otherwise, -1 is returned.
// --------------------------------------------------------------------
mp1_ioctl_find:
    pushl %ebp
    movl %esp, %ebp
    pushl %ebx

    // The only reliable way to know if the
    // argument is valid is by actually trying
    // to copy it to the kernel (note that this
    // copies to a stack buffer). After this,
    // cx holds the location (if the copy
    // succeeded).
    subl $4, %esp
    movl %esp, %ecx
    pushl $2
    movl 8(%ebp), %eax
    pushl %eax
    pushl %ecx
    call mp1_copy_from_user
    addl $12, %esp
    popl %ecx
    testl %eax, %eax
    jnz mp1_ioctl_find_err

    // Now we find the struct by its location
    pushl %ecx
    call mp1_find_by_location
    addl $4, %esp
    testl %eax, %eax
    jz mp1_ioctl_find_err

    // Found it? Copy it to userspace!
    pushl $STRUCT_SIZE
    movl (%eax), %eax
    pushl %eax
    movl 8(%ebp), %eax
    pushl %eax
    call mp1_copy_to_user
    addl $12, %esp
    testl %eax, %eax
    jz mp1_ioctl_find_ret
mp1_ioctl_find_err:
    movl $-1, %eax
mp1_ioctl_find_ret:
    popl %ebx
    leave
    ret


// --------------------------------------------------------------------
// int mp1_ioctl_sync(unsigned long packed)
//
// Synchronizes the structs with the given dest and src locations.
//
// Note: packed is interpreted as a struct {
//    unsigned short destination; // low 16 bits
//    unsigned short source;     // high 16 bits
// }
// --------------------------------------------------------------------
mp1_ioctl_sync:
    pushl %ebp
    movl %esp, %ebp
    pushl %ebx

    // Find destination (ebx)
    movzwl 8(%ebp), %eax
    pushl %eax
    call mp1_find_by_location
    addl $4, %esp
    test %eax, %eax
    jz mp1_ioctl_sync_err
    movl (%eax), %ebx

    // Find source (ecx)
    movzwl 10(%ebp), %eax
    pushl %eax
    call mp1_find_by_location
    addl $4, %esp
    test %eax, %eax
    jz mp1_ioctl_sync_err
    movl (%eax), %ecx

    // Copy fields source -> destination
    movw ON_LENGTH(%ecx), %ax
    movw %ax, ON_LENGTH(%ebx)
    movw OFF_LENGTH(%ecx), %ax
    movw %ax, OFF_LENGTH(%ebx)
    movw COUNTDOWN(%ecx), %ax
    movw %ax, COUNTDOWN(%ebx)
    movw STATUS(%ecx), %ax
    movw %ax, STATUS(%ebx)

    // Draw destination (already in ebx)
    call mp1_draw_struct

    xorl %eax, %eax
    jmp mp1_ioctl_sync_ret
mp1_ioctl_sync_err:
    movl $-1, %eax
mp1_ioctl_sync_ret:
    popl %ebx
    leave
    ret


.end
