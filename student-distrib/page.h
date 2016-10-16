#ifndef PAGE_H
#define PAGE_H

#include "x86_desc.h"
#include "types.h"

extern uint32_t page_dir_table;
extern uint32_t page_table;

/* structure for page entry */
typedef struct __attribute__((packed)) pe_t {
	uint8_t present : 1;
	uint8_t write :1;
	uint8_t user : 1;
	uint8_t write_through: 1;
	uint8_t cache_disabled : 1;
	uint8_t accessed : 1;
	uint8_t dirty : 1;
	uint8_t page_attr_idx : 1;
	uint8_t global : 1;
	uint8_t avail : 3;
	uint32_t base_addr : 20;
} pe_t;

/* structure for 4kB page dir entry */
typedef struct __attribute__((packed)) pdek_t {
	uint8_t present : 1;
	uint8_t write : 1;
	uint8_t user : 1;
	uint8_t write_through: 1;
	uint8_t cache_disabled : 1;
	uint8_t accessed : 1;
	uint8_t reserved : 1;
	uint8_t size : 1;
	uint8_t global : 1;
	uint8_t avail : 3;
	uint32_t base_addr : 20;
} pdek_t;	

/* structure for 4MB page dir entry */
typedef struct __attribute__((packed)) pdem_t {
	uint8_t present : 1;
	uint8_t write :1;
	uint8_t user : 1;
	uint8_t write_through: 1;
	uint8_t cache_disabled : 1;
	uint8_t accessed : 1;
	uint8_t dirty : 1;
	uint8_t size : 1;
	uint8_t global : 1;
	uint8_t avail : 3;
	uint8_t page_attr_idx : 1;
	uint16_t reserved : 9;
	uint16_t base_addr : 10;
} pdem_t;

extern void page_init();

#endif
