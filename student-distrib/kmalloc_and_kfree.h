

/* this is useless
typedef struct memory_map
{
    uint32_t size;
    uint32_t base_addr_low;
    uint32_t base_addr_high;
    uint32_t length_low;
    uint32_t length_high;
    uint32_t type;
} memory_map_t;
*/

typedef struct memory_map_entry
{
    uint32_t id;
    uint32_t size;
    uint32_t next;
    uint32_t prev;
} mm_block_t;

