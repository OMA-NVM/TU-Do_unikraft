#pragma once
#include <uk/config.h>
/*
 * Filename:
 * /home/christian/repos/uk/unikraft_mirror/plat/common/include/arm/arm64/mmu.h
 * Path: /home/christian/repos/uk/unikraft_mirror/plat/common/include/arm/arm64
 * Created Date: Wednesday, January 8th 2020, 9:48:35 am
 * Author: Christian Hakert
 *
 * This is a platform abstracting header for MMU functionality. Up to now, the
 * main purpose is to modify the mapping and the attrivutes of last level page
 * map entries. This is needed for libraries, which actively modify memory
 * permissions and make use of virtual memory, such as isolation of concurrent
 * threads, memory mirroring or transparent remapping
 */

enum plat_mmu_memory_permissions {
	PLAT_MMU_PERMISSION_RW_FROM_OS,
	PLAT_MMU_PERMISSION_RW_FROM_OS_USER,
	PLAT_MMU_PERMISSION_R_FROM_OS,
	PLAT_MMU_PERMISSION_R_FROM_OS_USER
};


#ifdef CONFIG_SEPARATE_STACK_PAGETABLES
#define PLAT_MMU_VSTACK_BASE 0x800000000
void plat_mmu_setup_stack_pages();
#endif
enum plat_mmu_memory_permissions
plat_mmu_get_access_permissions(unsigned long address);
void plat_mmu_set_access_permissions(unsigned long address,
				     unsigned long permissions);
void plat_mmu_set_access_flag(unsigned long address,
				     unsigned char flag);

unsigned long plat_mmu_get_pm_mapping(unsigned long address);
void plat_mmu_set_pm_mapping(unsigned long address, unsigned long pm_map);