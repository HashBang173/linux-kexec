/*
 * Spin Table SMP initialisation
 *
 * Copyright (C) 2013 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#define DEBUG

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/smp.h>
#include <linux/types.h>

#include <asm/cacheflush.h>
#include <asm/cpu_ops.h>
#include <asm/cputype.h>
#include <asm/smp_plat.h>
#include <asm/system_misc.h>

#include "cpu-properties.h"

static const char method_name[] = "spin-table";

extern void secondary_holding_pen(void);

volatile unsigned long secondary_holding_pen_release = INVALID_HWID;
volatile unsigned long secondary_holding_pen_return = INVALID_ADDR;
volatile unsigned long secondary_holding_pen_count = 0;

static DEFINE_PER_CPU(struct cpu_properties, cpu_properties) = {
	.cpu_release_addr = INVALID_ADDR,
	.cpu_return_addr = INVALID_ADDR,
};

/*
 * Write secondary_holding_pen_release in a way that is guaranteed to be
 * visible to all observers, irrespective of whether they're taking part
 * in coherency or not.  This is necessary for the hotplug code to work
 * reliably.
 */
static void write_pen_release(u64 val)
{
	void *start = (void *)&secondary_holding_pen_release;
	unsigned long size = sizeof(secondary_holding_pen_release);

	secondary_holding_pen_release = val;
	__flush_dcache_area(start, size);
}

static int smp_spin_table_cpu_init(struct device_node *dn, unsigned int cpu)
{
	int result;
	struct cpu_properties *p = &per_cpu(cpu_properties, cpu);

	result = read_cpu_properties(p, dn);

	if (result) {
		pr_err("ERROR: CPU %d: read_cpu_properties failed.\n", cpu);
		return -1;
	}

	return 0;
}

static int smp_spin_table_cpu_prepare(unsigned int cpu)
{
	const struct cpu_properties *p = &per_cpu(cpu_properties, cpu);
	__le64 __iomem *release_addr;

	if (p->cpu_release_addr == INVALID_ADDR)
		return -ENODEV;

	/*
	 * The cpu-release-addr may or may not be inside the linear mapping.
	 * As ioremap_cache will either give us a new mapping or reuse the
	 * existing linear mapping, we can use it to cover both cases. In
	 * either case the memory will be MT_NORMAL.
	 */
	release_addr = ioremap_cache(p->cpu_release_addr,
				     sizeof(*release_addr));
	if (!release_addr)
		return -ENOMEM;

	/*
	 * We write the release address as LE regardless of the native
	 * endianess of the kernel. Therefore, any boot-loaders that
	 * read this address need to convert this address to the
	 * boot-loader's endianess before jumping. This is mandated by
	 * the boot protocol.
	 */
	writeq_relaxed(__pa(secondary_holding_pen), release_addr);
	__flush_dcache_area((__force void *)release_addr,
			    sizeof(*release_addr));

	/*
	 * Send an event to wake up the secondary CPU.
	 */
	sev();

	iounmap(release_addr);

	return 0;
}

static int smp_spin_table_cpu_boot(unsigned int cpu)
{
	/*
	 * Update the pen release flag.
	 */
	write_pen_release(cpu_logical_map(cpu));

	/*
	 * Send an event, causing the secondaries to read pen_release.
	 */
	sev();

	return 0;
}


static int smp_spin_table_cpu_disable(unsigned int cpu)
{
	struct cpu_properties *p = &per_cpu(cpu_properties, cpu);

	return (p->cpu_return_addr == INVALID_ADDR) ? -EOPNOTSUPP : 0;
}

static void smp_spin_table_cpu_die(unsigned int cpu)
{
	const struct cpu_properties *p = &per_cpu(cpu_properties, cpu);

	/* Send cpu back to spin-table. */

	local_disable(DAIF_ALL);
	soft_restart(p->cpu_release_addr);

	BUG();
}

const struct cpu_operations smp_spin_table_ops = {
	.name		= method_name,
	.cpu_init	= smp_spin_table_cpu_init,
	.cpu_prepare	= smp_spin_table_cpu_prepare,
	.cpu_boot	= smp_spin_table_cpu_boot,
	.cpu_disable	= smp_spin_table_cpu_disable,
	.cpu_die	= smp_spin_table_cpu_die,
};

void smp_spin_table_shutdown(void)
{
	struct device_node *dn;

	pr_devel("%s: ->\n", __func__);
	pr_devel("%s:%d: secondary_holding_pen_count: %lu\n", __func__,
		__LINE__, secondary_holding_pen_count);

	/*
	 * Empty the secondary_holding_pen by releasing all cpus described in
	 * the device tree.  Contiune the loop if any errors are encountered.
	 */

	for (dn = NULL; (dn = of_find_node_by_type(dn, "cpu")); ) {
		struct cpu_properties cp;
		unsigned long timeout;
		unsigned long trigger;

		if (read_cpu_properties(&cp, dn))
			continue;

		if (cp.hwid == cpu_logical_map(smp_processor_id()))
			continue;

		if (strcmp(cp.enable_method, method_name))
			continue;

		/* For debugging. */

		if (cp.cpu_return_addr == 1)
			cp.cpu_return_addr = (u64)secondary_holding_pen;

		trigger = secondary_holding_pen_count - 1;

		mb();

		secondary_holding_pen_return = cp.cpu_return_addr;

		__flush_dcache_area((void *)&secondary_holding_pen_return,
			sizeof(secondary_holding_pen_return));

		secondary_holding_pen_release = cp.hwid;

		__flush_dcache_area((void *)&secondary_holding_pen_release,
			sizeof(secondary_holding_pen_release));

		sev();

		/* Wait for cpu to leave the secondary_holding_pen. */

		for (timeout = 1000;
			timeout && secondary_holding_pen_count > trigger;
			timeout--) {
			udelay(10);
			mb();
		}

		if (!timeout)
			pr_warn("%s: hwid %llx: shutdown timeout\n", __func__,
				cp.hwid);
	}

	pr_devel("%s:%d: secondary_holding_pen_count: %lu\n", __func__,
		__LINE__, secondary_holding_pen_count);

	if (secondary_holding_pen_count)
		pr_warn("%s: Warning: Some cpus may be lost (%lu).\n",
			__func__, secondary_holding_pen_count);

	pr_devel("%s: <-\n", __func__);
}

const struct cpu_operation_method cpu_operation_spin_table = {
	.shutdown = smp_spin_table_shutdown,
};
