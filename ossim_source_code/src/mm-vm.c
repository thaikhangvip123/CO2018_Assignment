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
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct rg_elmt)
{
    struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;

  if (rg_elmt.rg_start >= rg_elmt.rg_end)
    return -1;

  if (rg_node != NULL)
    rg_elmt.rg_next = rg_node;

  /* Enlist the new region */
  mm->mmap->vm_freerg_list = &rg_elmt;

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
    if (!caller || !caller->mm || !alloc_addr || size <= 0) {
        return -1;  // Kiểm tra đầu vào hợp lệ
    }

    struct vm_rg_struct rgnode;
    rgnode.vmaid = vmaid;
    int remaining_size = size;

        // Đảm bảo kích thước mở rộng tối thiểu là bội của PAGING_SBRK_INIT_SZ
    int min_expand_size = PAGING_SBRK_INIT_SZ;
    int aligned_size = (remaining_size + min_expand_size - 1) / min_expand_size * min_expand_size;

    while (remaining_size > 0) {
        if (get_free_vmrg_area(caller, vmaid, remaining_size, &rgnode) == 0) {
            caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
            caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
            caller->mm->symrgtbl[rgid].vmaid = rgnode.vmaid;
            *alloc_addr = rgnode.rg_start;
            return 0;
        }

        int expand_size = (remaining_size > aligned_size) ? aligned_size : min_expand_size;
        if (vmaid == 1) expand_size = -expand_size;

        if (inc_vma_limit(caller, vmaid, expand_size, NULL) != 0) {
            printf("Failed to expand segment %d\n", vmaid);
            return -1;
        }

        printf("Segment %d expanded successfully by %d bytes\n", vmaid, expand_size);
        remaining_size -= abs(expand_size);
    }

    return -1;
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
    // Lấy thông tin về vùng nhớ cần giải phóng từ bảng ký hiệu
    struct vm_rg_struct rgnode;
    if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ) {
        return -1;  // rgid không hợp lệ
    }
    rgnode = caller->mm->symrgtbl[rgid];

    // Kiểm tra nếu vùng nhớ hợp lệ
    if (rgnode.vmaid < 0 || rgnode.vmaid > 1) {
        return -1;  // vmaid không hợp lệ
    }
    // Quản lý việc giải phóng và thêm vào danh sách vùng nhớ đã giải phóng
    // Cần xử lý thêm các thông tin như kích thước vùng, vị trí giải phóng...
    enlist_vm_freerg_list(caller->mm, rgnode);

    // Sau khi giải phóng, bạn có thể cập nhật bảng ký hiệu hoặc bảng quản lý vùng nhớ nếu cần.
    // Trong trường hợp này, chỉ cần đánh dấu lại hoặc loại bỏ thông tin vùng đã giải phóng.

    // (Optional) Đánh dấu vùng nhớ đã giải phóng trong bảng ký hiệu.
    caller->mm->symrgtbl[rgid].rg_start = 0;  // Đánh dấu lại (có thể cần thêm thông tin về trạng thái vùng đã giải phóng)
    caller->mm->symrgtbl[rgid].rg_end = 0;
    caller->mm->symrgtbl[rgid].vmaid = -1;  // Đánh dấu vmaid là không hợp lệ

    return 0;  // Thành công
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

    if (!PAGING_PTE_PAGE_PRESENT(pte)) {
    //printf("[PG_GETPAGE] Page %d is not in memory, performing swap...\n", pgn);

    int vicpgn, swpfpn;
    if (find_victim_page(mm, &vicpgn) != 0) {
        printf("[PG_GETPAGE] Error: Cannot find victim page\n");
        return -1;
    }
    pthread_mutex_lock(&swap_lock);
    if (MEMPHY_get_freefp(caller->active_mswp, &swpfpn) != 0) {
        printf("[PG_GETPAGE] Error: No free frame in swap\n");
        pthread_mutex_unlock(&swap_lock);
        return -1;
    }
    pthread_mutex_unlock(&swap_lock);
    //printf("[PG_GETPAGE] Swapping out victim page %d to frame %d in swap\n", vicpgn, swpfpn);
    __swap_cp_page(caller->mram, vicpgn, caller->active_mswp, swpfpn);

    // Cập nhật trạng thái của victim page
    pte_set_swap(&mm->pgd[vicpgn], PAGING_PTE_SWAPPED_MASK, swpfpn);

    //printf("[PG_GETPAGE] Victim page %d swapped out successfully.\n", vicpgn);

     pte_set_fpn(&mm->pgd[pgn], vicpgn);
        PAGING_PTE_SET_PRESENT(mm->pgd[pgn]);

        // Thêm trang mục tiêu vào danh sách FIFO
        if (enlist_pgn_node(&caller->mm->fifo_pgn, pgn) != 0) {
            return -1; // Thêm vào danh sách thất bại
        }
}

    *fpn = PAGING_PTE_FPN(mm->pgd[pgn]);
    //printf("[PG_GETPAGE] Page %d is in memory, frame number %d\n", pgn, *fpn);

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
    if (pg_getpage(mm, pgn, &fpn, caller) != 0) {
        printf("[PG_GETVAL] Error: Failed to get page for addr=%d (page=%d).\n", addr, pgn);
        return -1;
    }
    printf("[PG_SETVAL] SUCCESS to get page for addr=%d (page=%d).\n", addr, pgn);
    int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
    pthread_mutex_lock(&ram_lock);
    if (MEMPHY_read(caller->mram, phyaddr, data) != 0) {
        printf("[PG_GETVAL] Error: Failed to read from physical address %d.\n", phyaddr);
        pthread_mutex_unlock(&ram_lock);
        return -1;
    }
    pthread_mutex_unlock(&ram_lock);
    printf("[PG_GETVAL] SUCCESS to read from physical address %d.\n", phyaddr);
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

    if (pg_getpage(mm, pgn, &fpn, caller) != 0) {
        printf("[PG_SETVAL] Error: Failed to get page for addr=%d (page=%d).\n", addr, pgn);
        return -1;
    }
    printf("[PG_SETVAL] SUCESS GET PAGE for addr =%d (page =%d).\n", addr, pgn);

    int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
    pthread_mutex_lock(&ram_lock);
    if (MEMPHY_write(caller->mram, phyaddr, value) != 0) {
        printf("[PG_SETVAL] Error: Failed to write to physical address %d.\n", phyaddr);
        pthread_mutex_unlock(&ram_lock);
        return -1;
    }
    pthread_mutex_unlock(&ram_lock);
    printf("success write to physical address %d.\n", phyaddr);

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

  for(pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
  {
    pte= caller->mm->pgd[pagenum];
    if (!PAGING_PTE_PAGE_PRESENT(pte))
    {
        fpn = PAGING_PTE_FPN(pte);
        pthread_mutex_lock(&ram_lock);
        MEMPHY_put_freefp(caller->mram, fpn);
        pthread_mutex_unlock(&ram_lock);
    } else {
        fpn = PAGING_PTE_SWP(pte);
        pthread_mutex_lock(&swap_lock);
        MEMPHY_put_freefp(caller->active_mswp, fpn);    
        pthread_mutex_unlock(&swap_lock);
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
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
    if (!cur_vma) return NULL;

    // Kiểm tra vượt giới hạn `vm_end`
    if (cur_vma->sbrk + alignedsz > cur_vma->vm_end) return NULL;

    struct vm_rg_struct *newrg = malloc(sizeof(struct vm_rg_struct));
    if (!newrg) return NULL;

    newrg->rg_start = cur_vma->sbrk;
    newrg->rg_end = cur_vma->sbrk + alignedsz;
    cur_vma->sbrk += alignedsz;

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
    struct vm_area_struct *cur = caller->mm->mmap;
    if(vmaend < 0 ){
        printf("[VALIDATE_OVERLAP] Error: NOT ENOUGH MEMORY,%d %d\n");
        return -1;
    }
    #ifdef MM_PAGING_HEAP_GODOWN
    if(vmaend > caller->vmemsz) {
        printf("[VALIDATE_OVERLAP] Error: NOT ENOUGH MEMORY,%d %d\n", caller->vmemsz, vmaend);
        return -1;
    }
    #endif

    while (cur) {
        if (cur->vm_id != vmaid && 
            ((vmastart >= cur->vm_start && vmastart < cur->vm_end) ||
             (vmaend > cur->vm_start && vmaend <= cur->vm_end))) {
            printf("[VALIDATE_OVERLAP] Error: Overlap detected with VMA %d\n", cur->vm_id);
            return -1;
        }
        cur = cur->vm_next;
    }

    //printf("[VALIDATE_OVERLAP] No overlap detected for VMA %d\n", vmaid);
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
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
    if (!cur_vma) return -1;

    int inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz);
    int incnumpage = inc_amt / PAGING_PAGESZ;

    printf("[INC_VMA_LIMIT] Expanding VMA %d by %d bytes (%d pages)\n", vmaid, inc_sz, incnumpage);

    if (validate_overlap_vm_area(caller, vmaid, cur_vma->vm_end, cur_vma->vm_end + (vmaid == 1? -inc_amt:inc_amt)) < 0) {
        printf("[INC_VMA_LIMIT] Error: Overlap detected while expanding VMA %d\n", vmaid);
        return -1;
    }


    if(vmaid == 1) {
      cur_vma->vm_end -= inc_amt;
    }else if(vmaid == 0) {
      cur_vma->vm_end += inc_amt;
    }
    *inc_limit_ret = inc_amt;

    printf("[INC_VMA_LIMIT] VMA %d expanded to: start=%d, end=%d\n", vmaid, cur_vma->vm_start, cur_vma->vm_end);
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

    if (!pg) {
        printf("[FIND_VICTIM_PAGE] Error: No pages in FIFO list to select as victim.\n");
        return -1;
    }
        

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
        mm->fifo_pgn = NULL;
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
                { /*End of free list */
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
