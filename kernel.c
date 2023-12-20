#include "kernel.h"

/* This function will create a process with the user-specified virtual memory size,
 * the mapping to physical memory is not built up yet (PFN = -1, present = 0),
 * returns a >= 0 pid (index in MMStruct array) when succeeded, -1 when failed. */
int proc_create_vm(struct Kernel* kernel, int size) {
    // 1. Check if a free process slot exists and if there's enough free space
    if (0 >= size || size > VIRTUAL_SPACE_SIZE) return -1;

    int no_of_pages_needed = (size - 1) / PAGE_SIZE + 1;
    if (kernel->allocated_pages + no_of_pages_needed > KERNEL_SPACE_SIZE / PAGE_SIZE) return -1;

    int pid = -1;
    for (int i = 0; i < MAX_PROCESS_NUM; ++i)
        if (!kernel->running[i]) {
            pid = i;
            break;
        }
    if (pid == -1) return -1;

    // 2. Malloc page_table and update allocated_pages
    kernel->allocated_pages += no_of_pages_needed;
    kernel->running[pid] = 1;

    kernel->mm[pid].size = size;
    kernel->mm[pid].page_table = malloc(sizeof(struct PageTable));
    kernel->mm[pid].page_table->ptes = malloc(sizeof(struct PTE) * no_of_pages_needed);

    // 3. The mapping to physical memory is not built up yet (PFN = -1, present = 0)
    for (int i = 0; i < no_of_pages_needed; ++i) {
        kernel->mm[pid].page_table->ptes[i].PFN = -1;
        kernel->mm[pid].page_table->ptes[i].present = 0;
    }

    return pid;
}

/* This function will read the virtual memory segment [addr, addr + size) of a user-specified process to buf (buf shd be >= size),
 * if any page of the VM segment is not yet mapped to physical memory, this will map it first with first fit policy,
 * returns 0 when succeeded, -1 when failed. */
int vm_read(struct Kernel* kernel, int pid, char* addr, int size, char* buf) {
    // 1. Check if the reading range is out-of-bounds
    if (size <= 0) return -1;
    if (0 > (long long) addr || (long long) addr >= kernel->mm[pid].size) return -1;
    if ((long long) addr + size > kernel->mm[pid].size) return -1;

    // 2. If any page of the VM segment is not yet mapped to physical memory, map it first with first fit policy
    int start = (long long) addr / PAGE_SIZE, start_offset = (long long) addr % PAGE_SIZE;
    int end = ((long long) addr + size - 1) / PAGE_SIZE, end_offset = ((long long) addr + size) % PAGE_SIZE;
    for (int i = start, curr = 0; i <= end; ++i) {
        if (!kernel->mm[pid].page_table->ptes[i].present) {
            int pfn = -1;
            for (int j = 0; j < KERNEL_SPACE_SIZE / PAGE_SIZE; ++j)
                if (!kernel->occupied_pages[j]) {
                    kernel->occupied_pages[j] = 1;
                    pfn = j;
                    break;
                }
            if (pfn == -1) return -1;

            kernel->mm[pid].page_table->ptes[i].PFN = pfn;
            kernel->mm[pid].page_table->ptes[i].present = 1;
        }

        int pfn = kernel->mm[pid].page_table->ptes[i].PFN;
        // Single page read
        if (start == end) memcpy(buf, kernel->space + PAGE_SIZE * pfn + start_offset, size);
        // Multiple page read
        else if (i == start) {
            memcpy(buf, kernel->space + PAGE_SIZE * pfn + start_offset, PAGE_SIZE - start_offset);
            curr += PAGE_SIZE - start_offset;
        }
        else if (i == end) memcpy(buf + curr, kernel->space + PAGE_SIZE * pfn, end_offset);
        else {
            memcpy(buf + curr, kernel->space + PAGE_SIZE * pfn, PAGE_SIZE);
            curr += PAGE_SIZE;
        }
    }
    return 0;
}

/* This function will write the virtual memory segment [addr, addr + size) of a user-specified process with buf (buf shd be >= size),
 * if any page of the VM segment is not yet mapped to physical memory, this will map it first with first fit policy,
 * returns 0 when succeeded, -1 when failed. */
int vm_write(struct Kernel* kernel, int pid, char* addr, int size, char* buf) {
    // 1. Check if the writing range is out-of-bounds
    if (size <= 0) return -1;
    if (0 > (long long) addr || (long long) addr >= kernel->mm[pid].size) return -1;
    if ((long long) addr + size > kernel->mm[pid].size) return -1;

    // 2. If any page of the VM segment is not yet mapped to physical memory, map it first with first fit policy
    int start = (long long) addr / PAGE_SIZE, start_offset = (long long) addr % PAGE_SIZE;
    int end = ((long long) addr + size - 1) / PAGE_SIZE, end_offset = ((long long) addr + size) % PAGE_SIZE;
    for (int i = start, curr = 0; i <= end; ++i) {
        if (!kernel->mm[pid].page_table->ptes[i].present) {
            int pfn = -1;
            for (int j = 0; j < KERNEL_SPACE_SIZE / PAGE_SIZE; ++j)
                if (!kernel->occupied_pages[j]) {
                    kernel->occupied_pages[j] = 1;
                    pfn = j;
                    break;
                }
            if (pfn == -1) return -1;

            kernel->mm[pid].page_table->ptes[i].PFN = pfn;
            kernel->mm[pid].page_table->ptes[i].present = 1;
        }

        int pfn = kernel->mm[pid].page_table->ptes[i].PFN;
        // Single page write
        if (start == end) memcpy(kernel->space + PAGE_SIZE * pfn + start_offset, buf, size);
        // Multiple page write
        else if (i == start) {
            memcpy(kernel->space + PAGE_SIZE * pfn + start_offset, buf, PAGE_SIZE - start_offset);
            curr += PAGE_SIZE - start_offset;
        }
        else if (i == end) memcpy(kernel->space + PAGE_SIZE * pfn, buf + curr, end_offset);
        else {
            memcpy(kernel->space + PAGE_SIZE * pfn, buf + curr, PAGE_SIZE);
            curr += PAGE_SIZE;
        }
    }
    return 0;
}

/* This function will destroy a user-specified process,
 * returns 0 when succeeded, -1 when failed. */
int proc_exit_vm(struct Kernel* kernel, int pid) {
    if (!kernel->running[pid]) return -1;

    // 1. Unset the corresponding pages in occupied_pages
    int no_of_pages_allocated = (kernel->mm[pid].size - 1) / PAGE_SIZE + 1;
    for (int i = 0; i < no_of_pages_allocated; ++i)
        if (kernel->mm[pid].page_table->ptes[i].present)
            kernel->occupied_pages[kernel->mm[pid].page_table->ptes[i].PFN] = 0;
    kernel->mm[pid].size = 0;
    kernel->allocated_pages -= no_of_pages_allocated;

    // 2. Be a responsible system programmer
    free(kernel->mm[pid].page_table->ptes);
    free(kernel->mm[pid].page_table);
    kernel->mm[pid].page_table = NULL;

    // Bye.
    kernel->running[pid] = 0;

    return 0;
}
