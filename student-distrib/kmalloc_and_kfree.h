//revise size and types (*) of fields 

typedef struct { //pg 354
    uint32_t vm_head;
    int_regs_t context; //??
    uint32_t total_area; //area available for allocation
    uint32_t used_area;
    vm_area_struct vm_list[/*TBD*/];
} mm_struct_t;

typedef struct { //pg 358
    uint32_t mm; //index of owner mm
    uint32_t vm_next; //next region
    uint32_t vm_prev;
    uint32_t size; //negative if not used

} vm_area_struct; //memory_region_t



