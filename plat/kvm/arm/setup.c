/* SPDX-License-Identifier: ISC */
/*
 * Authors: Wei Chen <Wei.Chen@arm.com>
 *
 * Copyright (c) 2018 Arm Ltd.
 *
 * Permission to use, copy, modify, and/or distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice appear
 * in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <config.h>
#include <libfdt.h>
#include <uk/plat/common/sections.h>
#include <uart/pl011.h>
#ifdef CONFIG_RTC_PL031
#include <rtc/pl031.h>
#endif /* CONFIG_RTC_PL031 */
#include <kvm/config.h>
#include <uk/assert.h>
#include <kvm-arm/mm.h>
#include <kvm/intctrl.h>
#include <arm/cpu.h>
#include <arm/arm64/cpu.h>
#include <arm/smccc.h>
#include <uk/arch/limits.h>

struct plat_config _libplat_cfg = { 0 };

#define MAX_CMDLINE_SIZE 1024
static char cmdline[MAX_CMDLINE_SIZE];
static const char *appname = CONFIG_UK_NAME;

smccc_conduit_fn_t smccc_psci_call;

#define _libplat_newstack(sp) ({				\
	__asm__ __volatile__("mov sp, %0\n" ::"r" (sp));	\
})

static void _init_dtb(void *dtb_pointer)
{
	int ret;

	if ((ret = fdt_check_header(dtb_pointer)))
		UK_CRASH("Invalid DTB: %s\n", fdt_strerror(ret));

	_libplat_cfg.dtb = dtb_pointer;
	uk_pr_info("Found device tree on: %p\n", dtb_pointer);
}

static void _dtb_get_psci_method(void)
{
	int fdtpsci, len;
	const char *fdtmethod;

	/*
	 * We just support PSCI-0.2 and PSCI-1.0, the PSCI-0.1 would not
	 * be supported.
	 */
	fdtpsci = fdt_node_offset_by_compatible(_libplat_cfg.dtb,
						-1, "arm,psci-1.0");
	if (fdtpsci < 0)
		fdtpsci = fdt_node_offset_by_compatible(_libplat_cfg.dtb,
							-1, "arm,psci-0.2");

	if (fdtpsci < 0) {
		uk_pr_info("No PSCI conduit found in DTB\n");
		goto enomethod;
	}

	fdtmethod = fdt_getprop(_libplat_cfg.dtb, fdtpsci, "method", &len);
	if (!fdtmethod || (len <= 0)) {
		uk_pr_info("No PSCI method found\n");
		goto enomethod;
	}

	if (!strcmp(fdtmethod, "hvc"))
		smccc_psci_call = smccc_hvc;
	else if (!strcmp(fdtmethod, "smc"))
		smccc_psci_call = smccc_smc;
	else {
		uk_pr_info("Invalid PSCI conduit method: %s\n",
			   fdtmethod);
		goto enomethod;
	}
	uk_pr_info("PSCI method: %s\n", fdtmethod);
	return;

enomethod:
	uk_pr_info("Support PSCI from PSCI-0.2\n");
	smccc_psci_call = NULL;
}

static void _init_dtb_mem(void)
{
	int fdt_mem, prop_len = 0, prop_min_len;
	int naddr, nsize;
	const uint64_t *regs;
	uint64_t mem_base, mem_size, max_addr;

	/* search for assigned VM memory in DTB */
	if (fdt_num_mem_rsv(_libplat_cfg.dtb) != 0)
		uk_pr_warn("Reserved memory is not supported\n");

	fdt_mem = fdt_node_offset_by_prop_value(_libplat_cfg.dtb, -1,
						"device_type",
						"memory", sizeof("memory"));
	if (fdt_mem < 0) {
		uk_pr_warn("No memory found in DTB\n");
		return;
	}

	naddr = fdt_address_cells(_libplat_cfg.dtb, fdt_mem);
	if (naddr < 0 || naddr >= FDT_MAX_NCELLS)
		UK_CRASH("Could not find proper address cells!\n");

	nsize = fdt_size_cells(_libplat_cfg.dtb, fdt_mem);
	if (nsize < 0 || nsize >= FDT_MAX_NCELLS)
		UK_CRASH("Could not find proper size cells!\n");

	/*
	 * QEMU will always provide us at least one bank of memory.
	 * unikraft will use the first bank for the time-being.
	 */
	regs = fdt_getprop(_libplat_cfg.dtb, fdt_mem, "reg", &prop_len);

	/*
	 * The property must contain at least the start address
	 * and size, each of which is 8-bytes.
	 */
	prop_min_len = (int)sizeof(fdt32_t) * (naddr + nsize);
	if (regs == NULL || prop_len < prop_min_len)
		UK_CRASH("Bad 'reg' property: %p %d\n", regs, prop_len);

	/* If we have more than one memory bank, give a warning messasge */
	if (prop_len > prop_min_len)
		uk_pr_warn("Currently, we support only one memory bank!\n");

	mem_base = fdt64_to_cpu(regs[0]);
	mem_size = fdt64_to_cpu(regs[1]);
	if (mem_base > __TEXT)
		UK_CRASH("Fatal: Image outside of RAM\n");

	max_addr = mem_base + mem_size;
	_libplat_cfg.pagetable.start = ALIGN_DOWN((uintptr_t)__END,
						     __PAGE_SIZE);
	_libplat_cfg.pagetable.len   = ALIGN_UP(page_table_size,
						   __PAGE_SIZE);
	_libplat_cfg.pagetable.end   = _libplat_cfg.pagetable.start
					  + _libplat_cfg.pagetable.len;

	/* AArch64 require stack be 16-bytes alignment by default */
	_libplat_cfg.bstack.end   = ALIGN_DOWN(max_addr,
						  __STACK_ALIGN_SIZE);
	_libplat_cfg.bstack.len   = ALIGN_UP(__STACK_SIZE,
						__STACK_ALIGN_SIZE);
	_libplat_cfg.bstack.start = _liblat_cfg.bstack.end
				       - _liblat_cfg.bstack.len;

	_libplat_cfg.heap.start = _libplat_cfg.pagetable.end;
	_libplat_cfg.heap.end   = _libplat_cfg.bstack.start;
	_libplat_cfg.heap.len   = _libplat_cfg.heap.end
				     - _libplat_cfg.heap.start;

	if (_libplat_cfg.heap.start > _libplat_cfg.heap.end)
		UK_CRASH("Not enough memory, giving up...\n");
}

static void _dtb_get_cmdline(char *cmdline, size_t maxlen)
{
	int fdtchosen, len;
	const char *fdtcmdline;

	/* TODO: Proper error handling */
	fdtchosen = fdt_path_offset(_libplat_cfg.dtb, "/chosen");
	if (!fdtchosen)
		goto enocmdl;
	fdtcmdline = fdt_getprop(_libplat_cfg.dtb, fdtchosen, "bootargs",
				 &len);
	if (!fdtcmdline || (len <= 0))
		goto enocmdl;

	if (likely(maxlen >= (unsigned int)len))
		maxlen = len;
	else
		uk_pr_err("Command line too long, truncated\n");

	strncpy(cmdline, fdtcmdline, maxlen);
	/* ensure null termination */
	cmdline[maxlen - 1] = '\0';

	uk_pr_info("Command line: %s\n", cmdline);
	return;

enocmdl:
	uk_pr_info("No command line found\n");
}

void __no_pauth _libplat_start(void *dtb_pointer)
{
	int ret;

	_init_dtb(dtb_pointer);

	pl011_console_init(dtb_pointer);

	uk_pr_info("Entering from KVM (arm64)...\n");

	/* Get command line from DTB */
	_dtb_get_cmdline(cmdline, sizeof(cmdline));

	/* Get PSCI method from DTB */
	_dtb_get_psci_method();

	/* Initialize memory from DTB */
	_init_dtb_mem();

#ifdef CONFIG_RTC_PL031
	/* Initialize RTC */
	pl031_init_rtc(dtb_pointer);
#endif /* CONFIG_RTC_PL031 */

	/* Initialize interrupt controller */
	intctrl_init();

	/* Initialize logical boot CPU */
	ret = lcpu_init(lcpu_get_bsp());
	if (unlikely(ret))
		UK_CRASH("Failed to initialize bootstrapping CPU: %d\n", ret);

#ifdef CONFIG_HAVE_SMP
	ret = lcpu_mp_init(CONFIG_UKPLAT_LCPU_RUN_IRQ,
			   CONFIG_UKPLAT_LCPU_WAKEUP_IRQ,
			   _libkvmplat_cfg.dtb);
	if (unlikely(ret))
		UK_CRASH("SMP initialization failed: %d.\n", ret);
#endif /* CONFIG_HAVE_SMP */

	uk_pr_info("pagetable start: %p\n",
		   (void *) _libplat_cfg.pagetable.start);
	uk_pr_info("     heap start: %p\n",
		   (void *) _libplat_cfg.heap.start);
	uk_pr_info("      stack top: %p\n",
		   (void *) _libplat_cfg.bstack.start);

	/*
	 * Switch away from the bootstrap stack as early as possible.
	 */
	uk_pr_info("Switch from bootstrap stack to stack @%p\n",
		   (void *) _libplat_cfg.bstack.end);

	_libplat_newstack(_libplat_cfg.bstack.end);

	ukplat_entry_argp(DECONST(char *, appname),
			  (char *)cmdline, strlen(cmdline));
}
