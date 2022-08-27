#include <arm/arm64/mmu.h>
#include <stdio.h>
#include <stdint.h>
#include <uk/arch/limits.h>
#include <gem5-arm/mm.h>
// This is the very end of the memory
extern unsigned long _end;
extern unsigned long _start_bin;

void plat_mmu_flush_tlb()
{
	asm volatile("dsb ishst;"
		     "tlbi vmalle1;"
		     "dsb ish;"
		     "isb;");
}

#ifdef CONFIG_SEPARATE_STACK_PAGETABLES
#define PLAT_MMU_STACK_L3_SIZE                                                 \
	((((CONFIG_APPLICATION_STACK_SIZE) >> 12) + 1) * 2)
#define PLAT_MMU_STACK_L2_SIZE ((PLAT_MMU_STACK_L3_SIZE / 512) + 1)
unsigned long plat_mmu_stack_l2_table[PLAT_MMU_STACK_L2_SIZE]
    __attribute((aligned(0x1000)));
unsigned long plat_mmu_stack_l3_table[PLAT_MMU_STACK_L3_SIZE]
    __attribute((aligned(0x1000)));

void plat_mmu_setup_stack_pages()
{
	// Figure out L1 Entry first
	unsigned long *l1_table =
	    (unsigned long *)(((unsigned long)&_end) + L1_TABLE_OFFSET);
	// Write L1 Table entry to L2 (!G)
	l1_table[PLAT_MMU_VSTACK_BASE >> 30] =
	    ((unsigned long)plat_mmu_stack_l2_table) | 0b11;

	// Fill the L2 Table with further 2MB Table entries
	unsigned long number_l2_entries =
	    ((CONFIG_APPLICATION_STACK_SIZE >> 21) * 2) + 1;
	for (unsigned long i = 0; i < number_l2_entries; i++) {
		plat_mmu_stack_l2_table[i] =
		    (((unsigned long)plat_mmu_stack_l3_table) + i * 4096)
		    | 0b11;
	}

	// Fill the L3 Table with 4k block entries
	unsigned long number_l3_entries =
	    (CONFIG_APPLICATION_STACK_SIZE >> 12) * 2;
	extern unsigned long __NVMSYMBOL__APPLICATION_STACK_BEGIN;
	unsigned long real_stack_begin =
	    (unsigned long)(&__NVMSYMBOL__APPLICATION_STACK_BEGIN);
	for (unsigned long i = 0; i < number_l3_entries; i++) {
		plat_mmu_stack_l3_table[i] =
		    (real_stack_begin
		     + (i % (CONFIG_APPLICATION_STACK_SIZE >> 12)) * 4096)
		    | 0b11100100111;
	}

	plat_mmu_flush_tlb();
}
#endif
enum plat_mmu_memory_permissions
plat_mmu_get_access_permissions(unsigned long address)
{
	// Determine L3 Table begin
	unsigned long *l3_table =
	    (unsigned long *)(((unsigned long)&_end) + L3_TABLE_OFFSET);

	// Determine the virtual address offset
	unsigned long vm_offset = (unsigned long)(&_start_bin);

#ifdef CONFIG_SEPARATE_STACK_PAGETABLES
	if (address >= PLAT_MMU_VSTACK_BASE) {
		l3_table = plat_mmu_stack_l3_table;
		vm_offset = PLAT_MMU_VSTACK_BASE;
	}
#endif
	// Load the permissions
	unsigned long target_permissions =
	    l3_table[(address - vm_offset) >> 12] & 0xC0;
	return target_permissions >> 6;
}

void plat_mmu_set_access_permissions(unsigned long address,
				     unsigned long permissions)
{
	// Determine L3 Table begin
	unsigned long *l3_table =
	    (unsigned long *)(((unsigned long)&_end) + L3_TABLE_OFFSET);

	// Determine the virtual address offset
	unsigned long vm_offset = (unsigned long)(&_start_bin);

#ifdef CONFIG_SEPARATE_STACK_PAGETABLES
	if (address >= PLAT_MMU_VSTACK_BASE) {
		l3_table = plat_mmu_stack_l3_table;
		vm_offset = PLAT_MMU_VSTACK_BASE;
	}
#endif
	// Load the permissions
	unsigned long target_page = l3_table[(address - vm_offset) >> 12];
	target_page &= ~(0xC0);
	target_page |= ((permissions << 6) & 0xC0);
	l3_table[(address - vm_offset) >> 12] = target_page;

#ifdef CONFIG_SEPARATE_STACK_PAGETABLES
	if (address >= PLAT_MMU_VSTACK_BASE) {
		if ((address - vm_offset) < CONFIG_APPLICATION_STACK_SIZE) {
			address += CONFIG_APPLICATION_STACK_SIZE;
		} else {
			address -= CONFIG_APPLICATION_STACK_SIZE;
		}
		target_page = l3_table[(address - vm_offset) >> 12];
		target_page &= ~(0xC0);
		target_page |= ((permissions << 6) & 0xC0);
		l3_table[(address - vm_offset) >> 12] = target_page;
	}
#endif
	plat_mmu_flush_tlb();
}

void plat_mmu_set_access_flag(unsigned long address,
				     unsigned char flag)
{
	// Determine L3 Table begin
	volatile unsigned long *l3_table =
	    (volatile unsigned long *)(((unsigned long)&_end) + L3_TABLE_OFFSET);

	// Determine the virtual address offset
	unsigned long vm_offset = (unsigned long)(&_start_bin);

    // printf("Setting flag for 0x%lx\n", address);

	// Load the permissions
	volatile unsigned long target_page = l3_table[(address - vm_offset) >> 12];
    // printf("FROM: 0x%lx\n", target_page);
	target_page &= ~(0b1 << 10);
	target_page |= ((flag << 10) & (0b1 << 10));
    // printf("TO: 0x%lx\n", target_page);
	l3_table[(address - vm_offset) >> 12] = target_page;
    
	plat_mmu_flush_tlb();
}

unsigned long plat_mmu_get_pm_mapping(unsigned long address)
{
	// Determine L3 Table begin
	unsigned long *l3_table =
	    (unsigned long *)(((unsigned long)&_end) + L3_TABLE_OFFSET);

	// Determine the virtual address offset
	unsigned long vm_offset = (unsigned long)(&_start_bin);

#ifdef CONFIG_SEPARATE_STACK_PAGETABLES
	if (address >= PLAT_MMU_VSTACK_BASE) {
		l3_table = plat_mmu_stack_l3_table;
		vm_offset = PLAT_MMU_VSTACK_BASE;
	}
#endif
	// Load the address
	unsigned long target_pm_page =
	    l3_table[(address - vm_offset) >> 12] & 0xFFFFFFFFF000;
	return target_pm_page;
}
void plat_mmu_set_pm_mapping(unsigned long address, unsigned long pm_map)
{
	// Determine L3 Table begin
	unsigned long *l3_table =
	    (unsigned long *)(((unsigned long)&_end) + L3_TABLE_OFFSET);

	// Determine the virtual address offset
	unsigned long vm_offset = (unsigned long)(&_start_bin);

#ifdef CONFIG_SEPARATE_STACK_PAGETABLES
	if (address >= PLAT_MMU_VSTACK_BASE) {
		l3_table = plat_mmu_stack_l3_table;
		vm_offset = PLAT_MMU_VSTACK_BASE;
	}
#endif
	// Load the address
	unsigned long target_page = l3_table[(address - vm_offset) >> 12];
	// Modify the address
	target_page &= ~0xFFFFFFFFF000;
	target_page |= (pm_map & 0xFFFFFFFFF000);
	l3_table[(address - vm_offset) >> 12] = target_page;

#ifdef CONFIG_SEPARATE_STACK_PAGETABLES
	if (address >= PLAT_MMU_VSTACK_BASE) {
		if ((address - vm_offset) < CONFIG_APPLICATION_STACK_SIZE) {
			address += CONFIG_APPLICATION_STACK_SIZE;
		} else {
			address -= CONFIG_APPLICATION_STACK_SIZE;
		}
		// Load the address
		target_page = l3_table[(address - vm_offset) >> 12];
		// Modify the address
		target_page &= ~0xFFFFFFFFF000;
		target_page |= (pm_map & 0xFFFFFFFFF000);
		l3_table[(address - vm_offset) >> 12] = target_page;
	}
#endif
	plat_mmu_flush_tlb();
}