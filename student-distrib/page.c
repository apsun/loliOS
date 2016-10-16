#include "page.h"

#define VIDEO 0xB8000

void page_init(void)
{
	uint32_t * pdt_ptr = (uint32_t *)page_dir_table;

	int i;
	for (i = 0; i < NUM_PDE; ++i) { /* 1024 entries */
		pdt_ptr[i] = 0;
	}
	/* pde_ptr now points to the first entry of pdt */
	pdek_t * pdek_ptr = (pdek_t *) pdt_ptr;

	pdek_ptr->present = 1;
	pdek_ptr->write = 1;
	pdek_ptr->user = 0;
	pdek_ptr->write_through = 0;
	pdek_ptr->cache_disabled = 0;
	pdek_ptr->accessed = 0;
	pdek_ptr->reserved = 0;
	pdek_ptr->size = 0; /* 0 for 4kB */
	pdek_ptr->global = 1;
	pdek_ptr->avail = 0;
	/*
		page_table
		| 31 			    12 | 11  		0 |
		| 20 bits 			   | 12 bits	  |
		| 11111111111111111111 | 000000000000 |
		| 0xfffff 			   | 0x000 		  |
	 */
	pdek_ptr->base_addr = (page_table & 0xfffff000) >> 12;
	
	/* pde_ptr now points to the second entry of pdt */
	/* 4 is the size of one entry of page directory entry */
	pdem_t * pdem_ptr = (pdem_t *) (pdt_ptr + 1);
	pdem_ptr->present = 1;
	pdem_ptr->write = 1;
	pdem_ptr->user = 0;
	pdem_ptr->write_through = 0;
	pdem_ptr->cache_disabled = 0;
	pdem_ptr->accessed = 0;
	pdem_ptr->dirty = 0;
	pdem_ptr->size = 1; /* 1 for 4MB */
	pdem_ptr->global = 1;
	pdem_ptr->avail = 0;
	pdem_ptr->page_attr_idx = 0;
	pdem_ptr->reserved = 0;
	pdem_ptr->base_addr = 1; /* 4MB >> 12 >> 10 */

	/* initialize page table of first page dir entry */
	uint32_t * pt_ptr = (uint32_t *) page_table;
	for (i = 0; i < NUM_PTE; ++i)
	{
		/* not present in memory and write to 1 */
		pt_ptr[i] = 0x2;
		pt_ptr[i] |= (i << 12);
	}
	
	uint32_t video_mem_idx = VIDEO >> 12; /* most significant 10 bit is 0 for this addr */
	pe_t * video_mem_ptr = (pe_t *) pt_ptr + (video_mem_idx);
	video_mem_ptr->present = 1;
	video_mem_ptr->write = 1;
	video_mem_ptr->user = 1;
	video_mem_ptr->write_through = 0;
	video_mem_ptr->cache_disabled = 1; /* assume video mem not caching */
	video_mem_ptr->accessed = 0;
	video_mem_ptr->dirty = 0;
	video_mem_ptr->page_attr_idx = 0;
	video_mem_ptr->global = 1;
	video_mem_ptr->avail = 0;
	video_mem_ptr->base_addr = video_mem_idx; /* addr = video_mem >> 12 */

	/* enable paging */
	asm volatile(
		"movl $page_dir_table, %%eax;"
		"andl 0xffffffe7,%%eax;"
		"movl %%eax, %%cr3;"
		"movl %%cr0, %%eax;"
		"orl 0x80000000, %%eax;"
		"movl %%eax, %%cr0;"
		"movl %%cr4, %%eax;"
		"orl 0x00000010, %%eax;"
		"movl %%eax, %%cr4;"		
		: 
		: 
		: "eax", "cc");
}
