// #ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

#include "string.h"
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>

/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)
{
    struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;

    if (rg_elmt->rg_start >= rg_elmt->rg_end)
        return -1;

    if (rg_node != NULL)
        rg_elmt->rg_next = rg_node;

    /* Enlist the new region */
    // mm->mmap->vm_freerg_list = &rg_elmt;

    struct vm_rg_struct *newNode = malloc(sizeof(struct vm_rg_struct));
    newNode->rg_start = rg_elmt->rg_start;
    newNode->rg_end = rg_elmt->rg_end;
    newNode->rg_next = NULL;

    if (rg_node == NULL || newNode->rg_end < rg_node->rg_start)
    {
        newNode->rg_next = rg_node;
        mm->mmap->vm_freerg_list = newNode;

        return 0;
    }

    struct vm_rg_struct *curr = mm->mmap->vm_freerg_list;
    struct vm_rg_struct *prev = NULL;
    int merged = 0;

    while (curr != NULL)
    {
        if (newNode->rg_start > curr->rg_end)
        {
            prev = curr;
            curr = curr->rg_next;
        }

        else if (newNode->rg_end >= curr->rg_start)
        {
            curr->rg_start = (newNode->rg_start < curr->rg_start) ? newNode->rg_start : curr->rg_start;
            curr->rg_end = (newNode->rg_end > curr->rg_end) ? newNode->rg_end : curr->rg_end;

            merged = 1;
            free(newNode);
            break;
        }
        else
            break;
    }

    if (!merged)
    {
        prev->rg_next = newNode;
        newNode->rg_next = curr;

        curr = newNode;
    }

    struct vm_rg_struct *curr_next = curr->rg_next;
    if (curr_next != NULL && curr_next->rg_start <= curr->rg_end)
    {
        curr->rg_end = (curr_next->rg_end > curr->rg_end) ? curr_next->rg_end : curr->rg_end;
        curr->rg_next = curr_next->rg_next;
        free(curr_next);
    }

    return 0;
}

/*get_vma_by_num - get vm area by numID
 *@mm: memory region
 *@vmaid: ID vm area to alloc memory region
 *
 */
struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid)
{
    struct vm_area_struct *pvma = mm->mmap;

    if (mm->mmap == NULL)
        return NULL;

    int vmait = 0;

    while (vmait < vmaid)
    {
        if (pvma == NULL)
            return NULL;

        vmait++;
        pvma = pvma->vm_next;
    }

    return pvma;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
    if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
        return NULL;

    return &mm->symrgtbl[rgid];
}

/*__alloc - allocate a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *@alloc_addr: address of allocated memory region
 *
 */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, int size, int *alloc_addr)
{
    /*Allocate at the toproof */
    struct vm_rg_struct rgnode;

    /* TODO: commit the vmaid */
    // rgnode.vmaid

    if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
    {
        printf("[__ALLOC] Found free region: start=0x%d, end=0x%d\n", rgnode.rg_start, rgnode.rg_end);
        
        // update symbol table
        caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
        caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
        caller->mm->symrgtbl[rgid].vmaid = rgnode.vmaid;

        *alloc_addr = rgnode.rg_start;

        return 0;
    }

    /* TODO: get_free_vmrg_area FAILED handle the region management (Fig.6)*/

    /* TODO retrive current vma if needed, current comment out due to compiler redundant warning*/
    /*Attempt to increate limit to get space */

    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
    if (!cur_vma) {
        printf("[__ALLOC] Error: Cannot find VMA with ID %d\n", vmaid);
        return -1;
    }
    int inc_sz = PAGING_PAGE_ALIGNSZ(size);
    int inc_limit_ret;
    if (inc_vma_limit(caller, vmaid, inc_sz, &inc_limit_ret) != 0) {
        printf("[__ALLOC] Error: Failed to expand VMA %d\n", vmaid);
        return -1;
    }
    /* TODO retrive old_sbrk if needed, current comment out due to compiler redundant warning*/
    int old_sbrk = cur_vma->sbrk;

    // Determine the new region's boundaries based on VMA type
    if (vmaid == 0) { // DATA segment (expand upwards)
        rgnode.rg_start = old_sbrk;
        //cur_vma->vm_end - inc_limit_ret;
        rgnode.rg_end = rgnode.rg_start + size;
                //printf("Debug===============================%d",rgnode.rg_end);

        old_sbrk = rgnode.rg_end;
        // rgnode.rg_end += inc_limit_ret;
    } else if (vmaid == 1) { // HEAP segment (expand downwards)
    rgnode.rg_end = old_sbrk;
    rgnode.rg_start = rgnode.rg_end - size; 
    old_sbrk = rgnode.rg_start;
    // cur_vma->vm_start -= inc_limit_ret; // Cập nhật vm_start
    // rgnode.rg_start = cur_vma->vm_end;
    } else {
        printf("[__ALLOC] Error: Unsupported VMA ID %d\n", vmaid);
        return -1;
    }
    rgnode.vmaid = vmaid;

    // Map the new region into physical RAM
    //printf("[__ALLOC] Mapping new region: start=0x%d, end=0x%d, size=%d\n",
           //rgnode.rg_start, rgnode.rg_end, inc_sz);
      if(vmaid == 0){
      if (vm_map_ram(caller, rgnode.rg_start, rgnode.rg_end, rgnode.rg_start,
                    inc_sz / PAGING_PAGESZ, &rgnode) < 0) {
          // Rollback VMA expansion on failure
           //printf("Debug===============================%d",rgnode.rg_end);
          if (vmaid == 0) cur_vma->vm_end -= inc_limit_ret;
          if (vmaid == 1) cur_vma->vm_start += inc_limit_ret;
          printf("[__ALLOC] Error: Failed to map memory for VMA %d\n", vmaid);
          return -1;
      }
    }   
 
    // Update symbol table and return the allocated address
    caller->mm->symrgtbl[rgid] = rgnode;
    *alloc_addr = rgnode.rg_start;

    printf("[__ALLOC] Allocation successful: reg.start = 0x%d , reg.end = 0x%d for reg %d\n", rgnode.rg_start, rgnode.rg_end, rgid);
   
    return 0;
}

/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __free(struct pcb_t *caller, int rgid)
{
    struct vm_rg_struct *rgnode;

    // Dummy initialization for avoding compiler dummay warning
    // in incompleted TODO code rgnode will overwrite through implementing
    // the manipulation of rgid later
    // rgnode.vmaid = 0;  //dummy initialization
    // rgnode.vmaid = 1;  //dummy initialization

    if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
        return -1;

    /* TODO: Manage the collect freed region to freerg_list */

    rgnode = get_symrg_byid(caller->mm, rgid);
    if (!rgnode)
        return -1;
    /*enlist the obsoleted memory region */
    enlist_vm_freerg_list(caller->mm, rgnode);
    rgnode->rg_start = rgnode->rg_end = -1;
    rgnode->rg_next = NULL;
    return 0;
}

/*pgalloc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int pgalloc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
    int addr;

    /* By default using vmaid = 0 */
    return __alloc(proc, 0, reg_index, size, &addr);
}

/*pgmalloc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify vaiable in symbole table)
 */
int pgmalloc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
    int addr;

    /* By default using vmaid = 1 */
    return __alloc(proc, 1, reg_index, size, &addr);
}

/*pgfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */

int pgfree_data(struct pcb_t *proc, uint32_t reg_index)
{
    return __free(proc, reg_index);
}

/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pagenum: PGN
 *@framenum: return FPN
 *@caller: caller
 *
 */
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
{
    uint32_t pte = mm->pgd[pgn];

    if (!PAGING_PTE_PAGE_PRESENT(pte))
    { /* Page is not online, make it actively living */
        int vicpgn, swpfpn;
        int vicfpn;
        uint32_t vicpte;

        int tgtfpn = PAGING_PTE_SWP(pte); // the target frame storing our variable

        /* TODO: Play with your paging theory here */
        /* Find victim page */
        do
        {
            if (find_victim_page(caller->mm, &vicpgn) != 0)
            { // find the vicitim page number via fifo
                return -1;
            }
        } while (vicpgn == pgn);

        vicpte = caller->mm->pgd[vicpgn];
        vicfpn = PAGING_FPN(vicpte);
        struct memphy_struct *memswp = (struct memphy_struct *)caller->mswp;

        /* FIND OFFSET MEMSWAP*/
        // 4 swap
        int i;
        for (i = 0; i < PAGING_MAX_MMSWP; i++)
        {
            if (memswp + i == caller->active_mswp)
                break;
        }

        if (MEMPHY_get_freefp(caller->active_mswp, &swpfpn) != 0)
        {
            struct memphy_struct **mswpIterator = caller->mswp;
            for (int j = 0; j < PAGING_MAX_MMSWP; j++)
            {
                if (MEMPHY_get_freefp(mswpIterator[j], &swpfpn) == 0)
                {
                    __swap_cp_page(caller->mram, vicfpn, mswpIterator[j], swpfpn);
                    caller->active_mswp = mswpIterator[j];
                    break;
                }
            }
        }
        else
        {
            /* Copy victim frame to swap */
            __swap_cp_page(caller->mram, vicfpn, caller->active_mswp, swpfpn);
        }
        /*Copy target frame from swap to victim frame in RAM*/
        __swap_cp_page(memswp + i, tgtfpn, caller->mram, vicfpn);
        MEMPHY_put_freefp(memswp + i, tgtfpn);

        pte_set_swap(&caller->mm->pgd[vicpgn], i, swpfpn);

        pte_set_fpn(&caller->mm->pgd[pgn], vicfpn);
        enlist_pgn_node(&caller->mm->fifo_pgn, pgn);
        pte = caller->mm->pgd[pgn];
    }

    *fpn = PAGING_PTE_FPN(pte);

    return 0;
}

/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
    int pgn = PAGING_PGN(addr);
    int off = PAGING_OFFST(addr);
    int fpn;

    /* Get the page to MEMRAM, swap from MEMSWAP if needed */
    if (pg_getpage(mm, pgn, &fpn, caller) != 0)
        return -1; /* invalid page access */

    int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

    MEMPHY_read(caller->mram, phyaddr, data);

    return 0;
}

/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{
    int pgn = PAGING_PGN(addr);
    int off = PAGING_OFFST(addr);
    int fpn;

    /* Get the page to MEMRAM, swap from MEMSWAP if needed */
    if (pg_getpage(mm, pgn, &fpn, caller) != 0)
        return -1; /* invalid page access */

    int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

    MEMPHY_write(caller->mram, phyaddr, value);

    return 0;
}

/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __read(struct pcb_t *caller, int rgid, int offset, BYTE *data)
{
    struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
    int vmaid = currg->vmaid;

    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

    if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
        return -1;

    pg_getval(caller->mm, currg->rg_start + offset, data, caller);

    return 0;
}

/*pgwrite - PAGING-based read a region memory */
int pgread(
    struct pcb_t *proc, // Process executing the instruction
    uint32_t source,    // Index of source register
    uint32_t offset,    // Source address = [source] + [offset]
    uint32_t destination)
{
    BYTE data;
    int val = __read(proc, source, offset, &data);

    destination = (uint32_t)data;
#ifdef IODUMP
    printf("read region=%d offset=%d value=%d\n", source, offset, data);
#ifdef PAGETBL_DUMP
    print_pgtbl(proc, 0, -1); // print max TBL
#endif
    MEMPHY_dump(proc->mram);
#endif

    return val;
}

/*__write - write a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __write(struct pcb_t *caller, int rgid, int offset, BYTE value)
{
    struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
    int vmaid = currg->vmaid;

    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

    if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
        return -1;

    pg_setval(caller->mm, currg->rg_start + offset, value, caller);

    return 0;
}

/*pgwrite - PAGING-based write a region memory */
int pgwrite(
    struct pcb_t *proc,   // Process executing the instruction
    BYTE data,            // Data to be wrttien into memory
    uint32_t destination, // Index of destination register
    uint32_t offset)
{
#ifdef IODUMP
    printf("write region=%d offset=%d value=%d\n", destination, offset, data);
#ifdef PAGETBL_DUMP
    print_pgtbl(proc, 0, -1); // print max TBL
#endif
    MEMPHY_dump(proc->mram);
#endif

    return __write(proc, destination, offset, data);
}

/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int free_pcb_memph(struct pcb_t *caller)
{
    int pagenum, fpn;
    uint32_t pte;

    for (pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
    {
        pte = caller->mm->pgd[pagenum];

        if (!PAGING_PTE_PAGE_PRESENT(pte))
        {
            fpn = PAGING_PTE_FPN(pte);
            MEMPHY_put_freefp(caller->mram, fpn);
        }
        else
        {
            fpn = PAGING_PTE_SWP(pte);
            MEMPHY_put_freefp(caller->active_mswp, fpn);
        }
    }

    return 0;
}

/*get_vm_area_node - get vm area for a number of pages
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
struct vm_rg_struct *get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid, int size, int alignedsz)
{
    struct vm_rg_struct *newrg;
    /* TODO retrive current vma to obtain newrg, current comment out due to compiler redundant warning*/
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

    newrg = malloc(sizeof(struct vm_rg_struct));

    /* TODO: update the newrg boundary
    // newrg->rg_start = ...
    // newrg->rg_end = ...
    */

    newrg->rg_start = cur_vma->sbrk;
    newrg->rg_end = newrg->rg_start + size;

    return newrg;
}

/*validate_overlap_vm_area
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, int vmastart, int vmaend)
{
    struct vm_area_struct *vma = caller->mm->mmap;

    /* TODO validate the planned memory area is not overlapped */

    while (vma != NULL)
    {
        if (vma->vm_start <= vmastart && vmastart < vma->vm_end)
            return -1;
        if (vma->vm_start < vmaend && vmaend <= vma->vm_end)
            return -1;
        vma = vma->vm_next;
    }
    return 0;
}

/*inc_vma_limit - increase vm area limits to reserve space for new variable
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@inc_sz: increment size
 *@inc_limit_ret: increment limit return
 *
 */
int inc_vma_limit(struct pcb_t *caller, int vmaid, int inc_sz, int *inc_limit_ret)
{
    struct vm_rg_struct *newrg = malloc(sizeof(struct vm_rg_struct));
    int inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz);
    int incnumpage = inc_amt / PAGING_PAGESZ;
    struct vm_rg_struct *area = get_vm_area_node_at_brk(caller, vmaid, inc_sz, inc_amt);
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

    int old_end = cur_vma->vm_end;

    /*Validate overlap of obtained region */
    if (validate_overlap_vm_area(caller, vmaid, area->rg_start, area->rg_end) < 0)
        return -1; /*Overlap and failed allocation */

    /* TODO: Obtain the new vm area based on vmaid */
    // cur_vma->vm_end...
    //  inc_limit_ret...
    cur_vma->vm_end += inc_sz;

    if (vm_map_ram(caller, area->rg_start, area->rg_end,
                   old_end, incnumpage, newrg) < 0)
        return -1; /* Map the memory to MEMRAM */

    return 0;
}

/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
int find_victim_page(struct mm_struct *mm, int *retpgn)
{
    struct pgn_t *pg = mm->fifo_pgn;

    /* TODO: Implement the theorical mechanism to find the victim page */

    if (!pg)
        return -1;

    while (pg->pg_next && pg->pg_next->pg_next)
    {
        pg = pg->pg_next;
    }
    // th1 second to last
    if (pg->pg_next)
    {
        *retpgn = pg->pg_next->pgn;
        free(pg->pg_next);
        pg->pg_next = NULL;
    }
    // th2 only one
    else
    {
        *retpgn = pg->pgn;
        free(pg);
        mm->fifo_pgn = pg = NULL;
    }

    return 0;
}

/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size
 *
 */
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
{
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

    struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;

    if (rgit == NULL)
        return -1;

    /* Probe unintialized newrg */
    newrg->rg_start = newrg->rg_end = -1;

    /* Traverse on list of free vm region to find a fit space */
    while (rgit != NULL && rgit->vmaid == vmaid)
    {
        if (rgit->rg_start + size <= rgit->rg_end)
        { /* Current region has enough space */
            newrg->rg_start = rgit->rg_start;
            newrg->rg_end = rgit->rg_start + size;

            /* Update left space in chosen region */
            if (rgit->rg_start + size < rgit->rg_end)
            {
                rgit->rg_start = rgit->rg_start + size;
            }
            else
            { /*Use up all space, remove current node */
                /*Clone next rg node */
                struct vm_rg_struct *nextrg = rgit->rg_next;

                /*Cloning */
                if (nextrg != NULL)
                {
                    rgit->rg_start = nextrg->rg_start;
                    rgit->rg_end = nextrg->rg_end;

                    rgit->rg_next = nextrg->rg_next;

                    free(nextrg);
                }
                else
                {                                  /*End of free list */
                    rgit->rg_start = rgit->rg_end; // dummy, size 0 region
                    rgit->rg_next = NULL;
                }
            }
        }
        else
        {
            rgit = rgit->rg_next; // Traverse next rg
        }
    }

    if (newrg->rg_start == -1) // new region not found
        return -1;

    return 0;
}

// #endif
