/**
 * # Author: Christian Hakert <christian.hakert@tu-dortmund.de>
 * # Create Time: 2019-12-06 10:27:13
 * # Modified by: Christian Hakert
 * # Modified time: 2019-12-06 11:00:11
 * # Description: This class provides the interface to the generic cpu
 * implementation, but uses m5ops to perform the interaction with gem5 actually
 */

#include "m5ops.h"
#include <cpu.h>

/*
 * Halts the CPU until the next external interrupt is fired. For Arm,
 * we can use WFI to implement this feature.
 */
void halt(void)
{
	__asm__ __volatile__("wfi");
}

/* Systems support PSCI >= 0.2 can do system reset from PSCI */
void reset(void)
{
	uk_pr_crit("Resetting gem5 via m5ops");
	extern void m5_exit();
	m5_exit();
}

/* Systems support PSCI >= 0.2 can do system off from PSCI */
void system_off(void)
{
	uk_pr_crit("Stopping gem5 via m5ops");
	extern void m5_exit();
	m5_exit();
}
