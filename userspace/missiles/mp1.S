# mp1.S - missile-command version
# Good luck, and have fun!

.data

    # Missile fields
    NEXT      = 0
    X         = 4
    Y         = 8
    VX        = 12
    VY        = 16
    DEST_X    = 20
    DEST_Y    = 24
    EXPLODED  = 28
    C         = 32

    # Characters
    EXPLOSION = 64
    SPACE     = 32
    CROSSHAIR = 43

    # Limits
    WIDTH     = 80
    HEIGHT    = 25

    # Local offsets in update_missiles
    UM_STATUS_CHANGED = -16
    UM_PREV_MISSILE   = -20
    UM_PREV_X         = -24
    UM_PREV_Y         = -28


.globl mp1_missile_list, base_alive, mp1_score
mp1_missile_list:   .long 0x0           # Head of list pointer
base_alive:         .long 0x0           # Array of 3 bytes, plus a padding byte
mp1_score:          .long 0x0           # Player's current score

base_pic:           .string "/^^^\\"    # Picture of a live base
dead_base_pic:      .string "xxxxx"     # Picture of a dead base
crosshairs_x:       .long 0x0           # X-position of the crosshairs
crosshairs_y:       .long 0x0           # Y-position of the crosshairs
mp1_ioctl_jumptable:                    # Jump table for mp1_ioctl
    .long mp1_ioctl_startgame
    .long mp1_ioctl_addmissile
    .long mp1_ioctl_movexhairs
    .long mp1_ioctl_getstatus
    .long mp1_ioctl_endgame


.text

# --------------------------------------------------------------------
# void mp1_rtc_tasklet(unsigned long garbage)
#
# (1) Updates the list of missiles
# (2) Redraws the bases
# (3) Redraws the crosshair
#
# Input: None
# Output: None
# Convention: cdecl
# --------------------------------------------------------------------
.globl mp1_rtc_tasklet
mp1_rtc_tasklet:
    call update_missiles
    call draw_bases
    call draw_crosshair
    ret


# --------------------------------------------------------------------
# int mp1_ioctl(unsigned long arg, unsigned int cmd)
#
# Dispatch function for the ioctls.
#
# Valid commands:
# (0) - startgame
# (1) - addmissile
# (2) - movexhairs
# (3) - getstatus
# (4) - endgame
#
# Input:
#   arg - the argument to the ioctl
#   cmd - which ioctl to execute
#
# Output: Zero on success, non-zero on failure.
# Convention: cdecl
# --------------------------------------------------------------------
.globl mp1_ioctl
mp1_ioctl:
    movl 8(%esp), %eax                  // cmd
    cmpl $5, %eax                       // if (cmd < 0 || cmd >= 5)
    jae mp1_ioctl_err
    jmp *mp1_ioctl_jumptable(, %eax, 4) // mp1_ioctl_jumptable[cmd](arg)
mp1_ioctl_err:
    movl $-1, %eax
    ret


# --------------------------------------------------------------------
# mp1_draw_char_hi
#
# Draws a character at the specified XY position.
#
# Input:
#   HI16(%ecx) - x-coordinate
#   HI16(%edx) - y-coordinate
#   %al - character
#
# Output: None
# Convention: cdecl registers clobbered, see input for args
# --------------------------------------------------------------------
mp1_draw_char_hi:
    shrl $16, %ecx                      // Get top 16 x bits
    shrl $16, %edx                      // Get top 16 y bits
    call mp1_draw_char_lo
    ret


# --------------------------------------------------------------------
# mp1_draw_char_lo
#
# Draws a character at the specified XY position.
#
# Input:
#   %ecx - x-coordinate
#   %edx - y-coordinate
#   %al - character
#
# Output: None
# Convention: cdecl registers clobbered, see input for args
# --------------------------------------------------------------------
mp1_draw_char_lo:
    pushl %eax                          // Save char so we can use eax for mul
    movl %edx, %eax                     // offset = y * WIDTH + x
    imull $WIDTH, %eax
    addl %ecx, %eax
    shll $1, %eax                       // Two bytes per character
    popl %ecx                           // Restore char to cl
    movl vmem_base_addr(,1), %edx       // Print character
    movb %cl, (%edx, %eax, 1)
    ret


# --------------------------------------------------------------------
# missile *mp1_missile_remove(missile *prev_missile)
#
# Destroys a missile.
#
# Input: The missile BEFORE the missile to destroy. If this is NULL,
#        the first missile in the list is destroyed.
# Output: Returns the next missile in the list.
# Convention: cdecl
# --------------------------------------------------------------------
mp1_missile_remove:
    pushl %ebp
    movl %esp, %ebp
    pushl %ebx
    movl 8(%ebp), %eax                  // prev_missile
    // If previous missile is NULL,
    // we should remove the first one
    testl %eax, %eax                    // if (prev_missile != NULL)
    jz mp1_missile_remove_first
    movl NEXT(%eax), %ecx               // missile *remove_me = prev_missile->next
    movl NEXT(%ecx), %ebx               // missile *new_next = remove_me->next
    movl %ebx, NEXT(%eax)               // prev_missile->next = new_next
    jmp mp1_missile_remove_free
mp1_missile_remove_first:
    movl mp1_missile_list, %ecx         // missile *remove_me = mp1_missile_list
    movl NEXT(%ecx), %ebx               // missile *new_next = remove_me->next
    movl %ebx, mp1_missile_list         // mp1_missile_list = new_next
mp1_missile_remove_free:
    pushl %ecx
    call mp1_free                       // mp1_free(remove_me)
    addl $4, %esp
    movl %ebx, %eax                     // return new_next
    popl %ebx
    leave
    ret


# --------------------------------------------------------------------
# void update_missiles()
#
# Updates missile position and state.
#
# (1) Move missiles according to their velocity
# (2) If a missile is off-screen, destroy it
# (3) If a missile reaches its destination, explode it
# (4) If a missile is finished exploding, destroy it
# (5) Draw missiles on the screen
# (6) Notify userspace if any missiles exploded
#
# Input: None
# Output: None
# Convention: cdecl
# --------------------------------------------------------------------
update_missiles:
    pushl %ebp
    movl %esp, %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    pushl $0                            // int status_changed@[ebp-16] = 0
    pushl $0                            // missile *prev_missile@[ebp-20] = NULL
    pushl $0                            // int prev_x@[ebp-24]
    pushl $0                            // int prev_y@[ebp-28]
    movl mp1_missile_list, %ebx         // missile *curr_missile = mp1_missile_list
update_missiles_loop:                   // while (curr_missile != NULL)
    testl %ebx, %ebx
    jz update_missiles_end
    // Save previous position so we
    // can clear the previous character
    movl X(%ebx), %ecx
    movl Y(%ebx), %edx
    movl %ecx, UM_PREV_X(%ebp)          // prev_x = curr_missile->x
    movl %edx, UM_PREV_Y(%ebp)          // prev_y = curr_missile->y
    // Update position from velocity
    // if the missile is not exploding
    cmpl $0, EXPLODED(%ebx)             // if (curr_missile->exploded != 0)
    jne update_missiles_loop_check_explode
    movl VX(%ebx), %ecx
    movl VY(%ebx), %edx
    addl %ecx, X(%ebx)                  // curr_missile->x += curr_missile->vx
    addl %edx, Y(%ebx)                  // curr_missile->y += curr_missile->vy
    // Check if missile is off screen
    movswl X+2(%ebx), %ecx              // hi_x = HI16(curr_missile->x)
    movswl Y+2(%ebx), %edx              // hi_y = HI16(curr_missile->y)
    cmpl $WIDTH, %ecx                   // if (hi_x < 0 || hi_x >= 80)
    jae update_missiles_loop_remove_off_screen
    cmpl $HEIGHT, %edx                  // if (hi_y < 0 || hi_y >= 25)
    jb update_missiles_loop_check_explode
update_missiles_loop_remove_off_screen:
    // The missile is off-screen,
    // clear previous missile position
    // and destroy the missile
    movl UM_PREV_X(%ebp), %ecx
    movl UM_PREV_Y(%ebp), %edx
    movb $SPACE, %al
    call mp1_draw_char_hi               // mp1_draw_char_hi(prev_x, prev_y, ' ')
    pushl UM_PREV_MISSILE(%ebp)
    call mp1_missile_remove             // curr_missile = mp1_missile_remove(prev_missile)
    addl $4, %esp
    movl %eax, %ebx
    jmp update_missiles_loop
update_missiles_loop_check_explode:
    // Check if missile is exploding
    // or destination has been reached
    cmpl $0, EXPLODED(%ebx)             // if (curr_missile->exploded > 0)
    jg update_missiles_loop_explode
    movswl X+2(%ebx), %ecx              // hi_x = HI16(curr_missile->x)
    movswl Y+2(%ebx), %edx              // hi_y = HI16(curr_missile->y)
    cmpl DEST_X(%ebx), %ecx             // if (hi_x == curr_missile->dest_x)
    jne update_missiles_loop_normal_draw
    cmpl DEST_Y(%ebx), %edx             // if (hi_y == curr_missile->dest_y)
    jne update_missiles_loop_normal_draw
update_missiles_loop_explode:
    // Explode missile, remove if done,
    // else draw explosion character
    pushl %ebx
    call missile_explode                // status_changed |= missile_explode(curr_missile)
    addl $4, %esp
    orl %eax, UM_STATUS_CHANGED(%ebp)
    decl EXPLODED(%ebx)                 // curr_missile->exploded--
    cmpl $0, EXPLODED(%ebx)             // if (curr_missile->exploded == 0)
    jne update_missiles_loop_explode_draw
    movl X(%ebx), %ecx
    movl Y(%ebx), %edx
    movb $SPACE, %al
    call mp1_draw_char_hi               // mp1_draw_char_hi(curr_x, curr_y, ' ')
    pushl UM_PREV_MISSILE(%ebp)
    call mp1_missile_remove             // curr_missile = mp1_missile_remove(prev_missile)
    addl $4, %esp
    movl %eax, %ebx
    jmp update_missiles_loop
update_missiles_loop_explode_draw:
    // Clear previous explosion,
    // draw new explosion
    movl UM_PREV_X(%ebp), %ecx
    movl UM_PREV_Y(%ebp), %edx
    movb $SPACE, %al
    call mp1_draw_char_hi               // mp1_draw_char_hi(prev_x, prev_y, ' ')
    movl X(%ebx), %ecx
    movl Y(%ebx), %edx
    movb $EXPLOSION, %al
    call mp1_draw_char_hi               // mp1_draw_char_hi(curr_x, curr_y, '@')
    jmp update_missiles_loop_increment
update_missiles_loop_normal_draw:
    // Clear previous missile,
    // draw new missile
    movl UM_PREV_X(%ebp), %ecx
    movl UM_PREV_Y(%ebp), %edx
    movb $SPACE, %al
    call mp1_draw_char_hi               // mp1_draw_char_hi(prev_x, prev_y, ' ')
    movl X(%ebx), %ecx
    movl Y(%ebx), %edx
    movb C(%ebx), %al
    call mp1_draw_char_hi               // mp1_draw_char_hi(curr_x, curr_y, curr_missile->c)
update_missiles_loop_increment:
    // Move to next missile in list
    movl %ebx, UM_PREV_MISSILE(%ebp)    // prev_missile = curr_missile
    movl NEXT(%ebx), %ebx               // curr_missile = curr_missile->next
update_missiles_loop_next:
    jmp update_missiles_loop
update_missiles_end:
    // Notify if any missile exploded
    cmpl $0, UM_STATUS_CHANGED(%ebp)    // if (status_changed != 0)
    je update_missiles_ret
    call mp1_notify_user                // mp1_notify_user()
update_missiles_ret:
    addl $16, %esp                      // Destroy 4 local variables
    popl %edi
    popl %esi
    popl %ebx
    leave
    ret


# --------------------------------------------------------------------
# void draw_bases()
#
# Draws each base at the bottom of the screen.
#
# Input: None
# Output: None
# Convention: cdecl
# --------------------------------------------------------------------
draw_bases:
    pushl %ebp
    movl %esp, %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    xorl %esi, %esi                     // i = 0
draw_bases_loop:                        // for (i = 0; i < 3; i++)
    cmpl $3, %esi                       // 3 bases
    je draw_bases_end
    xorl %edi, %edi                     // j = 0
    // Get the appropriate "image"
    // for the current base state
    cmpb $0, base_alive(%esi)           // if (!base_alive[i])
    jne draw_bases_loop_alive
    movl $dead_base_pic, %ebx           // pic = dead_base_pic
    jmp draw_bases_loop_inner
draw_bases_loop_alive:
    movl $base_pic, %ebx                // pic = base_pic
draw_bases_loop_inner:                  // for (j = 0; j < 5; j++)
    cmpl $5, %edi                       // 5 characters per base
    je draw_bases_loop_increment
    movl %esi, %eax
    incl %eax                           // Base index multiplier starts from 1
    imull $20, %eax                     // Bases are 20 units apart
    subl $2, %eax                       // Center base (left = center - 2)
    addl %edi, %eax
    movl %eax, %ecx                     // x = 20 * (i + 1) - 2 + j
    movl $HEIGHT-1, %edx                // y = HEIGHT - 1
    movb 0(%ebx, %edi, 1), %al          // c = pic[j]
    call mp1_draw_char_lo               // mp1_draw_char_lo(x, y, c)
    incl %edi
    jmp draw_bases_loop_inner
draw_bases_loop_increment:
    incl %esi
    jmp draw_bases_loop
draw_bases_end:
    popl %edi
    popl %esi
    popl %ebx
    leave
    ret


# --------------------------------------------------------------------
# void draw_crosshair()
#
# Draws the crosshair.
#
# Input: None
# Output: None
# --------------------------------------------------------------------
draw_crosshair:
    movl crosshairs_x, %ecx
    movl crosshairs_y, %edx
    movb $CROSSHAIR, %al
    call mp1_draw_char_lo               // mp1_draw_char_lo(crosshairs_x, crosshairs_y, '+')
    ret


# --------------------------------------------------------------------
# int mp1_ioctl_startgame(unsigned long ignore)
#
# Initializes the game.
#
# (1) Sets all bases to alive state
# (2) Resets the score to zero
# (3) Centers the crosshair
#
# Input: None
# Output: Always returns zero.
# Convention: cdecl
# --------------------------------------------------------------------
mp1_ioctl_startgame:
    movl $0, mp1_missile_list           // mp1_missile_list = NULL
    movl $0x00ffffff, base_alive        // base_alive = 0x00ffffff (all bases alive)
    movl $0, mp1_score                  // mp1_score = 0
    movl $40, crosshairs_x              // crosshairs_x = 40 (center xhair x)
    movl $12, crosshairs_y              // crosshairs_y = 12 (center xhair y)
    xorl %eax, %eax                     // return 0
    ret


# --------------------------------------------------------------------
# int mp1_ioctl_addmissile(missile *user_missile)
#
# Copies a missile from userspace and adds it to the missile list.
#
# Input: A userspace pointer to the missile to add.
# Output: Zero if the missile could be copied, non-zero otherwise.
# Convention: cdecl
# --------------------------------------------------------------------
mp1_ioctl_addmissile:
    pushl %ebp
    movl %esp, %ebp
    pushl %ebx
    // Allocate kernel memory
    pushl $36                           // sizeof(missile)
    call mp1_malloc                     // mem = mp1_malloc(sizeof(missile))
    addl $4, %esp
    testl %eax, %eax                    // if (mem == NULL)
    jz mp1_ioctl_addmissile_err
    movl %eax, %ebx
    // Copy userspace to kernel
    pushl $33                           // Only first 33 bytes are used in missile struct
    pushl 8(%ebp)                       // user_missile
    pushl %ebx
    call mp1_copy_from_user             // err = mp1_copy_from_user(mem, user_missile, 33)
    addl $12, %esp
    testl %eax, %eax                    // if (err != 0)
    jnz mp1_ioctl_addmissile_err_cleanup
    // Insert missile into list
    movl mp1_missile_list, %eax
    movl %eax, NEXT(%ebx)               // mem->next = mp1_missile_list
    movl %ebx, mp1_missile_list         // mp1_missile_list = mem
    xorl %eax, %eax                     // return 0
    jmp mp1_ioctl_addmissile_ret
mp1_ioctl_addmissile_err_cleanup:
    // Error occurred, free memory
    pushl %ebx
    call mp1_free                       // mp1_free(mem)
    addl $4, %esp
mp1_ioctl_addmissile_err:
    movl $-1, %eax                      // return -1
mp1_ioctl_addmissile_ret:
    popl %ebx
    leave
    ret


# --------------------------------------------------------------------
# int mp1_ioctl_movexhairs(unsigned long xhair_delta_packed)
#
# Moves the crosshair by the specified delta and redraws it.
#
# Input: Packed crosshair delta. Low 16 bits == dx, high 16 bits == dy.
# Output: Always returns zero.
# Convention: cdecl
# --------------------------------------------------------------------
mp1_ioctl_movexhairs:
    pushl %ebp
    movl %esp, %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    // Save previous xhair position
    // so we can clear it later
    movl crosshairs_x, %ecx             // prev_x = crosshairs_x
    movl crosshairs_y, %edx             // prev_y = crosshairs_y
    // Unpack xhair delta
    movswl 8(%ebp), %esi                // dx = LO16(xhair_delta_packed)
    movswl 10(%ebp), %edi               // dy = HI16(xhair_delta_packed)
    // Move xhair position
    addl %esi, crosshairs_x             // crosshairs_x += dx
    addl %edi, crosshairs_y             // crosshairs_y += dy
    // Ensure xhairs stay on screen
    cmpl $0, crosshairs_x               // if (crosshairs_x < 0)
    jge mp1_ioctl_movexhairs_test_x_80
    movl $0, crosshairs_x               // crosshairs_x = 0
mp1_ioctl_movexhairs_test_x_80:
    cmpl $WIDTH-1, crosshairs_x         // if (crosshairs_x > 79)
    jle mp1_ioctl_movexhairs_test_y_0
    movl $WIDTH-1, crosshairs_x         // crosshairs_x = 79
mp1_ioctl_movexhairs_test_y_0:
    cmpl $0, crosshairs_y               // if (crosshairs_y < 0)
    jge mp1_ioctl_movexhairs_test_y_25
    movl $0, crosshairs_y               // crosshairs_y = 0
mp1_ioctl_movexhairs_test_y_25:
    cmpl $HEIGHT-1, crosshairs_y        // if (crosshairs_y > 24)
    jle mp1_ioctl_movexhairs_draw
    movl $HEIGHT-1, crosshairs_y        // crosshairs_y = 24
mp1_ioctl_movexhairs_draw:
    // Clear previous xhair and draw
    // new one
    movb $SPACE, %al
    call mp1_draw_char_lo               // mp1_draw_char_lo(prev_x, prev_y, ' ')
    call draw_crosshair                 // draw_crosshair()
    xorl %eax, %eax                     // return 0
    popl %edi
    popl %esi
    popl %ebx
    leave
    ret


# --------------------------------------------------------------------
# int mp1_ioctl_getstatus(unsigned long *user_status)
#
# Copies the current game state to userspace.
#
# Input: A userspace pointer to the output status.
# Output: Zero if the status could be copied, non-zero otherwise.
# Convention: cdecl
# --------------------------------------------------------------------
mp1_ioctl_getstatus:
    pushl %ebp
    movl %esp, %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    xorl %edx, %edx                     // status = 0
    movw mp1_score, %dx                 // LO16(status) = LO16(mp1_score)
    // Set the bits corresponding to
    // the base alive state (1 = alive,
    // 0 = dead)
    testl $0x000000ff, base_alive       // if (base_alive[0])
    jz mp1_ioctl_getstatus_base2
    orl $0x00010000, %edx               // Set bit 16
mp1_ioctl_getstatus_base2:
    testl $0x0000ff00, base_alive       // if (base_alive[1])
    jz mp1_ioctl_getstatus_base3
    orl $0x00020000, %edx               // Set bit 17
mp1_ioctl_getstatus_base3:
    testl $0x00ff0000, base_alive       // if (base_alive[2])
    jz mp1_ioctl_getstatus_copy
    orl $0x00040000, %edx               // Set bit 18
mp1_ioctl_getstatus_copy:
    // Copy status into userspace
    pushl %edx                          // Push status since we need to take address
    movl %esp, %eax                     // &status
    pushl $4                            // sizeof(long)
    pushl %eax
    pushl 8(%ebp)                       // user_status
    call mp1_copy_to_user               // err = mp1_copy_to_user(user_status, &status, sizeof(long))
    addl $16, %esp                      // Pop 3 args and status variable
    testl %eax, %eax                    // if (err != 0)
    jnz mp1_ioctl_getstatus_err
    xorl %eax, %eax                     // return 0
    jmp mp1_ioctl_getstatus_ret
mp1_ioctl_getstatus_err:
    movl $-1, %eax                      // return -1
mp1_ioctl_getstatus_ret:
    popl %edi
    popl %esi
    popl %ebx
    leave
    ret


# --------------------------------------------------------------------
# int mp1_ioctl_endgame(unsigned long ignore)
#
# Frees all missiles.
#
# Input: None
# Output: Always returns zero.
# Convention: cdecl
# --------------------------------------------------------------------
mp1_ioctl_endgame:
    pushl %ebp
    movl %esp, %ebp
    pushl %ebx
    movl mp1_missile_list, %ebx         // missile *curr = mp1_missile_list
mp1_ioctl_endgame_loop:
    testl %ebx, %ebx                    // while (curr != NULL)
    jz mp1_ioctl_endgame_ret
    pushl %ebx                          // missile *curr2 = curr
    movl NEXT(%ebx), %ebx               // curr = curr->next
    call mp1_free                       // mp1_free(curr2)
    addl $4, %esp
    jmp mp1_ioctl_endgame_loop
mp1_ioctl_endgame_ret:
    movl $0, mp1_missile_list           // mp1_missile_list = NULL
    xorl %eax, %eax                     // return 0
    popl %ebx
    leave
    ret
