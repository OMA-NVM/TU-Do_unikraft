#include <arm/arm64/pmc_64.h>
unsigned long arm64_pmc_number_available_counters()
{
	unsigned long hdcr_value = 5;
	asm volatile("mrs %0, pmcr_el0" : "=r"(hdcr_value));
	return (hdcr_value & (0b11111 << 11)) >> 11;
}
unsigned int arm64_pmc_get_clock_divider()
{
	unsigned long hdcr_value = 5;
	asm volatile("mrs %0, pmcr_el0" : "=r"(hdcr_value));
	return (hdcr_value & (0b1 << 3)) >> 3;
}
void arm64_pmc_set_clock_divider(unsigned int enabled)
{
	unsigned long hdcr_value = 5;
	asm volatile("mrs %0, pmcr_el0" : "=r"(hdcr_value));
	hdcr_value &= ~(0b1 << 3);
	hdcr_value |= (enabled << 3);
	asm volatile("msr pmcr_el0, %0" ::"r"(hdcr_value));
}
void arm64_pmc_reset_cycle_counter()
{
	unsigned long hdcr_value = 5;
	asm volatile("mrs %0, pmcr_el0" : "=r"(hdcr_value));
	hdcr_value |= (0b1 << 2);
	asm volatile("msr pmcr_el0, %0" ::"r"(hdcr_value));
}
void arm64_pmc_reset_all_event_counter()
{
	unsigned long hdcr_value = 5;
	asm volatile("mrs %0, pmcr_el0" : "=r"(hdcr_value));
	hdcr_value |= (0b1 << 1);
	asm volatile("msr pmcr_el0, %0" ::"r"(hdcr_value));
}
void arm64_pmc_set_counters_enabled(unsigned int enabled)
{
	unsigned long hdcr_value = 5;
	asm volatile("mrs %0, pmcr_el0" : "=r"(hdcr_value));
	hdcr_value &= ~(0b1 << 0);
	hdcr_value |= (enabled << 0);
	asm volatile("msr pmcr_el0, %0" ::"r"(hdcr_value));
}
void arm64_pmc_set_event_counter_enabled(unsigned long counter_num,
					 unsigned int enabled)
{
	unsigned long pmcntenset = 5;
	asm volatile("mrs %0, pmcntenset_el0" : "=r"(pmcntenset));
	pmcntenset &= ~(0b1 << counter_num);
	pmcntenset |= (enabled << counter_num);
	asm volatile("msr pmcntenset_el0, %0" ::"r"(pmcntenset));
}

void arm64_pmc_enable_overflow_interrupt(unsigned long counter_num,
					 unsigned int enabled)
{
	if (!enabled) {
		unsigned long pmintenclr = 5;
		asm volatile("mrs %0, pmintenclr_el1" : "=r"(pmintenclr));
		pmintenclr |= (0b1 << counter_num);
		asm volatile("msr pmintenclr_el1, %0" ::"r"(pmintenclr));
	} else {
		unsigned long pmintenset = 5;
		asm volatile("mrs %0, pmintenset_el1" : "=r"(pmintenset));
		pmintenset |= (0b1 << counter_num);
		asm volatile("msr pmintenset_el1, %0" ::"r"(pmintenset));
	}
}
