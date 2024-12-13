// #ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Memory management unit mm/mm.c
 */

#include "mm.h"
#include <stdlib.h>
#include <stdio.h>

struct pgn_t *global_fifo = NULL;

/*
 * init_pte - Initialize PTE entry
 */
int init_pte(uint32_t *pte,
             int pre,    // present
             int fpn,    // FPN
             int drt,    // dirty
             int swp,    // swap
             int swptyp, // swap type
             int swpoff) // swap offset
{
  if (pre != 0)
  {
    if (swp == 0)
    { // Non swap ~ page online
      if (fpn == 0)
        return -1; // Invalid setting

      /* Valid setting with FPN */
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);
    }
    else
    { // page swapped
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
      SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
    }
  }

  return 0;
}

/*
 * pte_set_swap - Set PTE entry for swapped page
 * @pte    : target page table entry (PTE)
 * @swptyp : swap type
 * @swpoff : swap offset
 */
int pte_set_swap(uint32_t *pte, int swptyp, int swpoff)
{
  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);

  SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
  SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);

  return 0;
}

/*
 * pte_set_swap - Set PTE entry for on-line page
 * @pte   : target page table entry (PTE)
 * @fpn   : frame page number (FPN)
 */
int pte_set_fpn(uint32_t *pte, int fpn)
{
  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);

  SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);

  return 0;
}

/*
 * vmap_page_range - map a range of page at aligned address
 */
int vmap_page_range(struct pcb_t *caller,           // process call
                    int addr,                       // start address which is aligned to pagesz
                    int pgnum,                      // num of mapping page
                    struct framephy_struct *frames, // list of the mapped frames
                    struct vm_rg_struct *ret_rg)    // return mapped region, the real mapped fp
{                                                   // no guarantee all given pages are mapped
  // uint32_t *pte = malloc(sizeof(uint32_t));
  // struct framephy_struct *fpit = malloc(sizeof(struct framephy_struct));
  // int fpn;
  int pgit = 0;
  int pgn = PAGING_PGN(addr);

  /* TODO: update the rg_end and rg_start of ret_rg
  //ret_rg->rg_end =  ....
  //ret_rg->rg_start = ...
  //ret_rg->vmaid = ...
  */
  ret_rg->rg_start = addr;
  ret_rg->rg_end = addr + pgnum * PAGING_PAGESZ;
  ret_rg->vmaid = (addr < PAGING_SBRK_INIT_SZ) ? 0 : 1;
  // fpit->fp_next = frames;

  // check if frames are available
  if(frames == NULL) return -1;

  for (pgit = 0; pgit < pgnum; ++pgit)
  {
    // Get the frame to map
    int fpn = frames->fpn;  // Physical frame number
    uint32_t pte;

    // Initialize Page Table Entry (PTE)
    init_pte(&pte, 1, fpn, 0, 0, 0, 0); // Assuming "present" flag is 1, no write-protect, etc.

    // Map this page in the page table
    caller->mm->pgd[pgn + pgit] = pte;

    // Move to the next frame
    frames = frames->fp_next;
    if (frames == NULL && pgit < pgnum - 1) {
        return -1; // Error: Not enough frames for all pages
    }

    // Enqueue the page number for page replacement tracking (FIFO)
    enlist_pgn_node(&caller->mm->fifo_pgn, pgn + pgit);
  }
  return 0;
}

/*
 * alloc_pages_range - allocate req_pgnum of frame in ram
 * @caller    : caller
 * @req_pgnum : request page num
 * @frm_lst   : frame list
 */

int alloc_pages_range(struct pcb_t *caller, int req_pgnum, struct framephy_struct **frm_lst)
{
  int pgit, fpn;
  struct framephy_struct *newfp_str;

  // Allocate requested number of frames (pages) for the caller
  for (pgit = 0; pgit < req_pgnum; pgit++) {
    // Get a free frame physical number (fpn) from memory
    if (MEMPHY_get_freefp(caller->mram, &fpn) == 0) {
      // If a free frame is found, allocate it and add to the list

      // Initialize the frame structure (it can be extended to contain more info)
      newfp_str = malloc(sizeof(struct framephy_struct));
      newfp_str->fpn = fpn;  // Store the frame physical number (fpn) in the structure
      
      // Link the allocated frame to the list of allocated frames
      newfp_str->fp_next = *frm_lst; // Link to the previous frames in the list
      *frm_lst = newfp_str; // Set the head of the list to the new frame

    } else {
      // If not enough frames are available, handle the error case
      // Free any frames that were allocated until now
      struct framephy_struct *temp;
      while (*frm_lst != NULL) {
        temp = *frm_lst;
        *frm_lst = (*frm_lst)->fp_next;
        free(temp);  // Free the allocated frames
      }

      // Return an error code (e.g., -1) to indicate failure to allocate all frames
      return -1;
    }
  }

  // Successfully allocated all pages, return 0
  return 0;
}

/*
 * vm_map_ram - do the mapping all vm are to ram storage device
 * @caller    : caller
 * @astart    : vm area start
 * @aend      : vm area end
 * @mapstart  : start mapping point
 * @incpgnum  : number of mapped page
 * @ret_rg    : returned region
 */
int vm_map_ram(struct pcb_t *caller, int astart, int aend, int mapstart, int incpgnum, struct vm_rg_struct *ret_rg)
{
  struct framephy_struct *frm_lst = NULL;
  int ret_alloc;

  /*@bksysnet: author provides a feasible solution of getting frames
   *FATAL logic in here, wrong behaviour if we have not enough page
   *i.e. we request 1000 frames meanwhile our RAM has size of 3 frames
   *Don't try to perform that case in this simple work, it will result
   *in endless procedure of swap-off to get frame and we have not provide
   *duplicate control mechanism, keep it simple
   */
  ret_alloc = alloc_pages_range(caller, incpgnum, &frm_lst);

  if (ret_alloc < 0 && ret_alloc != -3000)
    return -1;

  /* Out of memory */
  if (ret_alloc == -3000)
  {
#ifdef MMDBG
    printf("OOM: vm_map_ram out of memory \n");
#endif
    return -1;
  }

  /* it leaves the case of memory is enough but half in ram, half in swap
   * do the swaping all to swapper to get the all in ram */
  vmap_page_range(caller, mapstart, incpgnum, frm_lst, ret_rg);

  return 0;
}

/* Swap copy content page from source frame to destination frame
 * @mpsrc  : source memphy
 * @srcfpn : source physical page number (FPN)
 * @mpdst  : destination memphy
 * @dstfpn : destination physical page number (FPN)
 **/
int __swap_cp_page(struct memphy_struct *mpsrc, int srcfpn,
                   struct memphy_struct *mpdst, int dstfpn)
{
  int cellidx;
  int addrsrc, addrdst;
  for (cellidx = 0; cellidx < PAGING_PAGESZ; cellidx++)
  {
    addrsrc = srcfpn * PAGING_PAGESZ + cellidx;
    addrdst = dstfpn * PAGING_PAGESZ + cellidx;

    BYTE data;
    MEMPHY_read(mpsrc, addrsrc, &data);
    MEMPHY_write(mpdst, addrdst, data);
  }

  return 0;
}

/*
 *Initialize a empty Memory Management instance
 * @mm:     self mm
 * @caller: mm owner
 */
int init_mm(struct mm_struct *mm, struct pcb_t *caller)
{
  // create VMA for heap segment
  struct vm_area_struct *vma0 = malloc(sizeof(struct vm_area_struct));
  struct vm_area_struct *vma1 = malloc(sizeof(struct vm_area_struct));
  if(!vma0) {
    perror("Allocation failed for VMA0!");
    return -1;
  }
  if(!vma1) {
    perror("Allocation failed for VMA1!");
    free(vma0);
    return -1;
  }
  // allocation page with max size page
  mm->pgd = malloc(PAGING_MAX_PGN * sizeof(uint32_t));

  /* By default the owner comes with at least one vma for DATA */
  vma0->vm_id = 0;
  vma0->vm_start = 0;
  vma0->vm_end = PAGING_SBRK_INIT_SZ;
  //vma0->sbrk = vma0->vm_start;
  vma0->sbrk = 0;
  struct vm_rg_struct *first_rg = init_vm_rg(vma0->vm_start, vma0->vm_end, 0);
  enlist_vm_rg_node(&vma0->vm_freerg_list, first_rg);

  // set VMA1 for heap segment (from highest address)
  vma1->vm_id = 1;
  vma1->vm_start = PAGING_MEMRAMSZ;
  vma1->vm_end = 0;
  vma1->sbrk = PAGING_MEMRAMSZ;

  struct vm_rg_struct *heap_rg = init_vm_rg(vma1->vm_start, vma1->vm_end, 0);
  enlist_vm_rg_node(&vma1->vm_freerg_list, heap_rg);

  vma0->vm_next = vma1;
  vma1->vm_next = NULL;

  vma0->vm_mm = mm;
  vma1->vm_mm = mm;


  mm->mmap = vma0;

  /* TODO update VMA0 next */
  // vma0->next = ...

  /* TODO: update one vma for HEAP */
  // vma1->vm_id = ...
  // vma1->vm_start = ...
  // vma1->vm_end = ...
  // vma1->sbrk = ...
  // enlist_vm_rg_node(&vma1...)
  // vma1->vm_next
  // enlist_vm_rg_node(&vma1->vm_freerg_list,...)

  /* Point vma owner backward */
  // vma0->vm_mm = mm;
  // vma1->vm_mm = mm;

  /* TODO: update mmap */
  // mm->mmap = ...

  return 0;
}

struct vm_rg_struct *init_vm_rg(int rg_start, int rg_end, int vmaid)
{
  struct vm_rg_struct *rgnode = malloc(sizeof(struct vm_rg_struct));

  rgnode->rg_start = rg_start;
  rgnode->rg_end = rg_end;
  rgnode->vmaid = vmaid;
  rgnode->rg_next = NULL;

  return rgnode;
}

int enlist_vm_rg_node(struct vm_rg_struct **rglist, struct vm_rg_struct *rgnode)
{
  rgnode->rg_next = *rglist;
  *rglist = rgnode;

  return 0;
}

int enlist_pgn_node(struct pgn_t **plist, int pgn)
{
  struct pgn_t *pnode = malloc(sizeof(struct pgn_t));

  pnode->pgn = pgn;
  pnode->pg_next = *plist;
  *plist = pnode;

  return 0;
}

int print_list_fp(struct framephy_struct *ifp)
{
  struct framephy_struct *fp = ifp;

  printf("print_list_fp: ");
  if (fp == NULL)
  {
    printf("NULL list\n");
    return -1;
  }
  printf("\n");
  while (fp != NULL)
  {
    printf("fp[%d]\n", fp->fpn);
    fp = fp->fp_next;
  }
  printf("\n");
  return 0;
}

int print_list_rg(struct vm_rg_struct *irg)
{
  struct vm_rg_struct *rg = irg;

  printf("print_list_rg: ");
  if (rg == NULL)
  {
    printf("NULL list\n");
    return -1;
  }
  printf("\n");
  while (rg != NULL)
  {
    printf("rg[%ld->%ld<at>vma=%d]\n", rg->rg_start, rg->rg_end, rg->vmaid);
    rg = rg->rg_next;
  }
  printf("\n");
  return 0;
}

int print_list_vma(struct vm_area_struct *ivma)
{
  struct vm_area_struct *vma = ivma;

  printf("print_list_vma: ");
  if (vma == NULL)
  {
    printf("NULL list\n");
    return -1;
  }
  printf("\n");
  while (vma != NULL)
  {
    printf("va[%ld->%ld]\n", vma->vm_start, vma->vm_end);
    vma = vma->vm_next;
  }
  printf("\n");
  return 0;
}

int print_list_pgn(struct pgn_t *ip)
{
  printf("print_list_pgn: ");
  if (ip == NULL)
  {
    printf("NULL list\n");
    return -1;
  }
  printf("\n");
  while (ip != NULL)
  {
    printf("va[%d]-\n", ip->pgn);
    ip = ip->pg_next;
  }
  printf("n");
  return 0;
}

int print_pgtbl(struct pcb_t *caller, uint32_t start, uint32_t end)
{
  int pgn_start, pgn_end;
  int pgit;

  if (end == -1)
  {
    pgn_start = 0;
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, 0);
    end = cur_vma->vm_end;
  }
  pgn_start = PAGING_PGN(start);
  pgn_end = PAGING_PGN(end);

  printf("print_pgtbl: %d - %d", start, end);
  if (caller == NULL)
  {
    printf("NULL caller\n");
    return -1;
  }
  printf("\n");

  for (pgit = pgn_start; pgit < pgn_end; pgit++)
  {
    printf("%08ld: %08x\n", pgit * sizeof(uint32_t), caller->mm->pgd[pgit]);
  }

  return 0;
}

// #endif
