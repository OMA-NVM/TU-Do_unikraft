/*
 * Filename: plat/common/include/arm/arm64/pmc_64.h
 * Path: plat/common/include/arm/arm64
 * Created Date: Monday, December 16th 2019, 8:50:26 am
 * Author: Christian Hakert
 *
 * This is a header for an ARMv8 general purpose performance counter
 * implementation
 */

#pragma once
unsigned long arm64_pmc_number_available_counters();
unsigned int arm64_pmc_get_clock_divider();
void arm64_pmc_set_clock_divider(unsigned int enabled);
void arm64_pmc_reset_cycle_counter();
void arm64_pmc_reset_all_event_counters();
void arm64_pmc_set_counters_enabled(unsigned int enabled);
void arm64_pmc_set_event_counter_enabled(unsigned long counter_num,
					 unsigned int enabled);

#define arm64_pmc_read_event_counter(counter_num)                              \
	({                                                                     \
		unsigned int counter = 42;                                     \
		asm volatile("mrs %0, pmevcntr" #counter_num "_el0"            \
			     : "=r"(counter));                                 \
		counter;                                                       \
	})
#define arm64_pmc_write_event_counter(counter_num, enabled)                    \
	({                                                                     \
		unsigned int write_val = enabled;                              \
		asm volatile("msr pmevcntr" #counter_num                       \
			     "_el0, %0" ::"r"(write_val));                     \
		write_val;                                                     \
	})
#define arm64_pmc_set_count_event(counter_num, event)                          \
	({                                                                     \
		unsigned int pmevtyper = 42;                                   \
		unsigned int event_num = event;                                \
		asm volatile("mrs %0, pmevtyper" #counter_num "_el0"           \
			     : "=r"(pmevtyper));                               \
		pmevtyper &= ~(0b1111111111);                                  \
		pmevtyper |= (event_num & 0b1111111111);                       \
		asm volatile("msr pmevtyper" #counter_num                      \
			     "_el0, %0" ::"r"(pmevtyper));                     \
		event_num;                                                     \
	})
#define arm64_pmc_set_pmevtyper_bit(counter_num, enabled, bit_offset)          \
	({                                                                     \
		unsigned int pmevtyper = 42;                                   \
		unsigned int bit = bit_offset;                                 \
		asm volatile("mrs %0, pmevtyper" #counter_num "_el0"           \
			     : "=r"(pmevtyper));                               \
		if (!enabled) {                                                \
			pmevtyper |= (0b1 << bit);                             \
		} else {                                                       \
			pmevtyper &= ~(0b1 << bit);                            \
		}                                                              \
		asm volatile("msr pmevtyper" #counter_num                      \
			     "_el0, %0" ::"r"(pmevtyper));                     \
		pmevtyper;                                                     \
	})
#define arm64_pmc_set_secure_el3_counting(counter_num, enabled)                \
	arm64_pmc_set_pmevtyper_bit(counter_num, enabled, 26)
#define arm64_pmc_set_non_secure_el2_counting(counter_num, enabled)            \
	arm64_pmc_set_pmevtyper_bit(counter_num, enabled, 27)
#define arm64_pmc_set_non_secure_el0_counting(counter_num, enabled)            \
	arm64_pmc_set_pmevtyper_bit(counter_num, enabled, 28)
#define arm64_pmc_set_non_secure_el1_counting(counter_num, enabled)            \
	arm64_pmc_set_pmevtyper_bit(counter_num, enabled, 29)
#define arm64_pmc_set_el0_counting(counter_num, enabled)                       \
	arm64_pmc_set_pmevtyper_bit(counter_num, enabled, 30)
#define arm64_pmc_set_el1_counting(counter_num, enabled)                       \
	arm64_pmc_set_pmevtyper_bit(counter_num, enabled, 31)
void arm64_pmc_enable_overflow_interrupt(unsigned long counter_num,
					 unsigned int enabled);

enum arm64_pmc_event_values {
	ARM64_PMC_BUS_ACCESS_STORE = 0x61,
	ARM64_PMC_BUS_ACCESS = 0x19,
	ARM64_PMC_CPU_CYCLES = 0x11
};
