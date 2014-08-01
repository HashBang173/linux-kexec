/*
 *  ARM64 Specific Sleep Functionality
 *
 *  Copyright (C) 2013-2014, Linaro Ltd.
 *      Author: Graeme Gregory <graeme.gregory@linaro.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/acpi.h>

/*
 * Currently the ACPI 5.1 standard does not define S states in a
 * manner which is usable for ARM64. These two stubs are sufficient
 * that system initialises and device PM works.
 */
u32 acpi_target_system_state(void)
{
	return ACPI_STATE_S0;
}
EXPORT_SYMBOL_GPL(acpi_target_system_state);

int __init acpi_sleep_init(void)
{
	return -ENOSYS;
}
