/**
 * Copyright (c) 2014 Himanshu Chauhan.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file irq.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief PCI IRQ  fail handing code.
 *
 * All the work under drivers/pci/ is a derived work from Linux's
 * PCI Framework. The following is the commit ID from which it has
 * been derived.
 *
 * commit 97bf6af1f928216fd6c5a66e8a57bfa95a659672
 * Linux 3.19-rc1
 *
 * PCI IRQ failure handing code
 *
 * Copyright (c) 2008 James Bottomley <James.Bottomley@HansenPartnership.com>
 */

#include <linux/acpi.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/pci.h>

static void pci_note_irq_problem(struct pci_dev *pdev, const char *reason)
{
	struct pci_dev *parent = to_pci_dev(pdev->dev.parent);

	dev_err(&pdev->dev,
		"Potentially misrouted IRQ (Bridge %s %04x:%04x)\n",
		dev_name(&parent->dev), parent->vendor, parent->devid);
	dev_err(&pdev->dev, "%s\n", reason);
	dev_err(&pdev->dev, "Please report to linux-kernel@vger.kernel.org\n");
	WARN_ON(1);
}

/**
 * pci_lost_interrupt - reports a lost PCI interrupt
 * @pdev:	device whose interrupt is lost
 *
 * The primary function of this routine is to report a lost interrupt
 * in a standard way which users can recognise (instead of blaming the
 * driver).
 *
 * Returns:
 *  a suggestion for fixing it (although the driver is not required to
 * act on this).
 */
enum pci_lost_interrupt_reason pci_lost_interrupt(struct pci_dev *pdev)
{
	if (pdev->msi_enabled || pdev->msix_enabled) {
		enum pci_lost_interrupt_reason ret;

		if (pdev->msix_enabled) {
			pci_note_irq_problem(pdev, "MSIX routing failure");
			ret = PCI_LOST_IRQ_DISABLE_MSIX;
		} else {
			pci_note_irq_problem(pdev, "MSI routing failure");
			ret = PCI_LOST_IRQ_DISABLE_MSI;
		}
		return ret;
	}
#ifdef CONFIG_ACPI
#if 0
	if (!(acpi_disabled || acpi_noirq)) {
		pci_note_irq_problem(pdev, "Potential ACPI misrouting please reboot with acpi=noirq");
		/* currently no way to fix acpi on the fly */
		return PCI_LOST_IRQ_DISABLE_ACPI;
	}
#endif
#endif
	pci_note_irq_problem(pdev, "unknown cause (not MSI or ACPI)");
	return PCI_LOST_IRQ_NO_INFORMATION;
}
EXPORT_SYMBOL(pci_lost_interrupt);
