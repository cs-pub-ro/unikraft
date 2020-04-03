/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Authors: Stefan Teodorescu <stefanl.teodorescu@gmail.com>
 *
 * Copyright (c) 2020, University Politehnica of Bucharest. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * THIS HEADER MAY NOT BE EXTRACTED OR MODIFIED IN ANY WAY.
 */

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <uk/assert.h>
#include <uk/print.h>
#include <uk/plat/mm.h>

unsigned long phys_bitmap_start_addr;
unsigned long phys_bitmap_length;

unsigned long bitmap_start_addr;
unsigned long bitmap_length;

unsigned long internal_pt_start_addr;
unsigned long internal_pt_length;

unsigned long allocatable_memory_start_addr;
unsigned long allocatable_memory_length;

static unsigned long get_free_vpage(void)
{
	unsigned long offset;

	offset = uk_bitmap_find_next_zero_area(
			(unsigned long *) bitmap_start_addr, bitmap_length,
			0 /* start */, 1 /* nr */, 0 /* align_mask */);

	uk_bitmap_set((unsigned long *) bitmap_start_addr, offset, 1);

	if (offset * PAGE_SIZE > internal_pt_length) {
		uk_pr_err("Filled up all available space for page tables\n");
		return PAGE_INVALID;
	}

	return internal_pt_start_addr + (offset << PAGE_SHIFT);
}

static unsigned long uk_pt_alloc_table(size_t level)
{
	unsigned long here = get_free_vpage();

	if (here == PAGE_INVALID)
		return PAGE_INVALID;

	/* Xen requires that PTs are mapped read-only */
#ifdef CONFIG_PARAVIRT
	uk_page_set_prot(here, PAGE_PROT_READ);
#endif	/* CONFIG_PARAVIRT */

	/*
	 * This is an L(n + 1) entry, so we set L(n + 1) flags
	 * (Index in pagetable_protections is level of PT - 1)
	 */
	return (virt_to_mfn(here) << PAGE_SHIFT) | pagetable_protections[level];
}

static void uk_pt_release_if_unused(unsigned long vaddr, unsigned long pt,
		unsigned long parent_pt, size_t level)
{
	unsigned long offset;
	size_t i;

	if (!PAGE_ALIGNED(pt) || !PAGE_ALIGNED(parent_pt)) {
		uk_pr_err("Table's address must be aligned to page size\n");
		return;
	}

	for (i = 0; i < pagetable_entries[level - 1]; i++)
		if (PAGE_PRESENT(uk_pte_read(pt, i, level)))
			return;

	ukarch_pte_write(parent_pt, Lx_OFFSET(vaddr, level + 1), 0, level + 1);
	ukarch_flush_tlb_entry(parent_pt);

	offset = (pt - internal_pt_start_addr) >> PAGE_SHIFT;
	uk_bitmap_clear((unsigned long *) bitmap_start_addr, offset, 1);
}

int uk_page_map(unsigned long vaddr, unsigned long paddr, unsigned long prot)
{
	unsigned long pt, pte;

	if (!PAGE_ALIGNED(vaddr)) {
		uk_pr_err("Address must be aligned to page size\n");
		return -1;
	}

	if (paddr == -1) {
		paddr = ukarch_phys_frame_get();

		if (paddr == PAGE_INVALID)
			return -1;
	}

	/*
	 * XXX: On 64-bits architectures (x86_64 and arm64) the hierarchical
	 * page tables have a 4 level layout. This implementation will need a
	 * revision when introducing support for 32-bits architectures, since
	 * there are only 3 levels of page tables.
	 */
	pt = ukarch_read_pt_base();

	pte = uk_pte_read(pt, L4_OFFSET(vaddr), 4);
	if (!PAGE_PRESENT(pte)) {
		pte = uk_pt_alloc_table(3);

		if (pte == PAGE_INVALID)
			return -1;

		ukarch_pte_write(pt, L4_OFFSET(vaddr), pte, 4);
	}

	pt = (unsigned long) pte_to_virt(pte);
	pte = uk_pte_read(pt, L3_OFFSET(vaddr), 3);
	if (!PAGE_PRESENT(pte)) {
		pte = uk_pt_alloc_table(2);

		if (pte == PAGE_INVALID)
			return -1;

		ukarch_pte_write(pt, L3_OFFSET(vaddr), pte, 3);
	}

	pt = (unsigned long) pte_to_virt(pte);
	pte = uk_pte_read(pt, L2_OFFSET(vaddr), 2);
	if (!PAGE_PRESENT(pte)) {
		pte = uk_pt_alloc_table(1);

		if (pte == PAGE_INVALID)
			return -1;

		ukarch_pte_write(pt, L2_OFFSET(vaddr), pte, 2);
	}

	pt = (unsigned long) pte_to_virt(pte);
	pte = uk_pte_read(pt, L1_OFFSET(vaddr), 1);
	if (!PAGE_PRESENT(pte)) {
		ukarch_pte_write(pt, L1_OFFSET(vaddr),
				 ukarch_pte_l1e(PTE_REMOVE_FLAGS(paddr), prot),
				 1);
	}

	return 0;
}

int uk_page_unmap(unsigned long vaddr)
{
	unsigned long l1_table, l2_table, l3_table, l4_table, pte;
	unsigned long offset, pfn;

	if (!PAGE_ALIGNED(vaddr)) {
		uk_pr_err("Address must be aligned to page size\n");
		return -1;
	}

	l4_table = ukarch_read_pt_base();
	pte = uk_pte_read(l4_table, L4_OFFSET(vaddr), 4);
	if (!PAGE_PRESENT(pte))
		return -1;

	l3_table = (unsigned long) pte_to_virt(pte);
	pte = uk_pte_read(l3_table, L3_OFFSET(vaddr), 3);
	if (!PAGE_PRESENT(pte))
		return -1;

	l2_table = (unsigned long) pte_to_virt(pte);
	pte = uk_pte_read(l2_table, L2_OFFSET(vaddr), 2);
	if (!PAGE_PRESENT(pte))
		return -1;

	l1_table = (unsigned long) pte_to_virt(pte);
	pfn = pte_to_pfn(uk_pte_read(l1_table, L1_OFFSET(vaddr), 1));
	ukarch_pte_write(l1_table, L1_OFFSET(vaddr), 0, 1);
	ukarch_flush_tlb_entry(vaddr);

	offset = pfn - (allocatable_memory_start_addr >> PAGE_SHIFT);
	uk_bitmap_clear((unsigned long *) phys_bitmap_start_addr, offset, 1);

	uk_pt_release_if_unused(vaddr, l1_table, l2_table, 1);
	uk_pt_release_if_unused(vaddr, l2_table, l3_table, 2);
	uk_pt_release_if_unused(vaddr, l3_table, l4_table, 3);

	return 0;
}

unsigned long uk_virt_to_l1_pte(unsigned long vaddr)
{
	unsigned long pt, pt_entry;

	if (!PAGE_ALIGNED(vaddr)) {
		uk_pr_err("Address must be aligned to page size\n");
		return PAGE_NOT_MAPPED;
	}

	pt = ukarch_read_pt_base();
	pt_entry = uk_pte_read(pt, L4_OFFSET(vaddr), 4);
	if (!PAGE_PRESENT(pt_entry))
		return PAGE_NOT_MAPPED;

	pt = (unsigned long) pte_to_virt(pt_entry);
	pt_entry = uk_pte_read(pt, L3_OFFSET(vaddr), 3);
	if (!PAGE_PRESENT(pt_entry))
		return PAGE_NOT_MAPPED;

	if (PAGE_HUGE(pt_entry))
		return pt_entry;

	pt = (unsigned long) pte_to_virt(pt_entry);
	pt_entry = uk_pte_read(pt, L2_OFFSET(vaddr), 2);
	if (!PAGE_PRESENT(pt_entry))
		return PAGE_NOT_MAPPED;

	if (PAGE_LARGE(pt_entry))
		return pt_entry;

	pt = (unsigned long) pte_to_virt(pt_entry);
	pt_entry = uk_pte_read(pt, L1_OFFSET(vaddr), 1);

	return pt_entry;
}

int uk_pt_init(unsigned long paddr_start, size_t len)
{
	/*
	 * The needed bookkeeping internal structures are:
	 * - a physical address bitmap, to keep track of all available physical
	 *   addresses (which will have a bit for every frame, so the size
	 *   allocatable_memory_length / PAGE_SIZE)
	 * - a memory area where page tables are stored
	 * - a bitmap for pages used as page tables
	 *
	 * In order to obtain the size of each structure, the following equation
	 * must be solved:
	 * size(phys_bitmap) + size(pt_bitmap) + size(pt_area)
	 * + size(allocatable_memory) == len
	 */
	double internal_pt_const = 1.0 / L1_PAGETABLE_ENTRIES
		+ 1.0 / (L1_PAGETABLE_ENTRIES * L2_PAGETABLE_ENTRIES)
		+ 1.0 / (L1_PAGETABLE_ENTRIES * L2_PAGETABLE_ENTRIES * L3_PAGETABLE_ENTRIES);

	double allocatable_memory_constant = 1.0 /
		(1 + internal_pt_const + 1.0 / PAGE_SIZE + internal_pt_const / PAGE_SIZE);

	UK_ASSERT(PAGE_ALIGNED(paddr_start));
	UK_ASSERT(PAGE_ALIGNED(len));

	/*
	 * |allocatable_memory_length| is the length of the remaining memory,
	 * after allocating the necessary bitmaps.
	 */
	allocatable_memory_length = len * allocatable_memory_constant;
	allocatable_memory_length = PAGE_ALIGN_DOWN(allocatable_memory_length);

	/*
	 * Need to bookkeep |allocatable_memory_length| bytes of physical
	 * memory, starting from |allocatable_memory_start_addr|. This is the
	 * physical memory given by the hypervisor.
	 *
	 * In Xen's case, the bitmap keeps the pseudo-physical addresses, the
	 * translation to machine frames being done later.
	 */
	phys_bitmap_start_addr = paddr_start;
	phys_bitmap_length = allocatable_memory_length >> PAGE_SHIFT;
	uk_bitmap_zero((unsigned long *) phys_bitmap_start_addr,
			phys_bitmap_length);

	internal_pt_start_addr =
		PAGE_ALIGN(phys_bitmap_start_addr + phys_bitmap_length);
	internal_pt_length = PAGE_ALIGN((unsigned long)
			(allocatable_memory_length * internal_pt_const));
	memset((void *) internal_pt_start_addr, 0, internal_pt_length);

	/* Bookkeeping free pages used for PT allocations */
	bitmap_start_addr =
		PAGE_ALIGN(internal_pt_start_addr + internal_pt_length);
	bitmap_length = internal_pt_length >> PAGE_SHIFT;
	uk_bitmap_zero((unsigned long *) bitmap_start_addr, bitmap_length);

	/* The remaining memory is the actual usable memory */
	allocatable_memory_start_addr =
	    PAGE_ALIGN(bitmap_start_addr + bitmap_length);

	if (allocatable_memory_start_addr + allocatable_memory_length > paddr_start + len)
		allocatable_memory_length -=
			allocatable_memory_start_addr
			+ allocatable_memory_length
			- paddr_start - len;

	return 0;
}
