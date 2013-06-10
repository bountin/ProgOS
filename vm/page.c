#include "vm/page.h"
#include <debug.h>
#include "threads/thread.h"
#include <stdio.h>
#include <string.h>
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "filesys/file.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"

static unsigned
page_hash (const struct hash_elem *elem, void *aux UNUSED)
{
  const struct spt_elem *p = hash_entry (elem, struct spt_elem, hash_elem); 
  return hash_bytes (&p->upage, sizeof p->upage);
}

static bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED)
{
  const struct spt_elem *a = hash_entry (a_, struct spt_elem, hash_elem);
  const struct spt_elem *b = hash_entry (b_, struct spt_elem, hash_elem);

  return a->upage < b->upage;
}

struct hash *supp_pagedir_create (void)
{
  struct hash *spd = malloc (sizeof (struct hash));

  hash_init (spd, page_hash, page_less, (void *) NULL);

  return spd;
}

bool page_load (void *addr, void *esp)
{
  struct spt_elem search;
  struct spt_elem *spt;
  struct hash_elem *e;
  struct thread *t = thread_current ();
  bool writeable;

  ASSERT (!hash_empty (t->supp_pagedir));

  /* Get a page of memory. */
  uint8_t *kpage = palloc_get_page (PAL_USER);
  if (kpage == NULL)
    return false;

  search.upage = (uint32_t)addr & ~PGMASK;
  e = hash_find (t->supp_pagedir, &search.hash_elem);

  if (e != NULL) {
    // Page was found in the suppl. page directory
    spt = hash_entry (e, struct spt_elem, hash_elem);

    /* Load this page. */
    if (file_read_at (spt->file, kpage, spt->read_bytes, spt->file_offset) != (int) spt->read_bytes) {
      palloc_free_page (kpage);
      return false;
    }
    memset (kpage + spt->read_bytes, 0, spt->zero_bytes);
    writeable = spt->writeable;

    /* Add the page to the process's address space. */
    if (!install_page (spt->upage, kpage, writeable)) {
      palloc_free_page (kpage);
      return false;
    }
  } else {
    if (t->esp != NULL)
      esp = t->esp;
    if (esp < PHYS_BASE - 8*1024*1024) {
      // Limiting stack to 8 MB
      palloc_free_page (kpage);
      return false;
    }
    if (addr > esp || ((uint32_t)esp - (uint32_t)addr) <= 32) {
      // Install stack page
      if (!install_page ((void *)search.upage, kpage, true)) {
        palloc_free_page (kpage);
        return false;
      }
    } else {
      palloc_free_page (kpage);
      return false;
    }
  }

  return true;
}

static unsigned
mmap_id_hash (const struct hash_elem *elem, void *aux UNUSED)
{
  const struct mmap_id_elem *p = hash_entry (elem, struct mmap_id_elem, hash_elem); 
  return hash_bytes (&p->mmap_id, sizeof p->mmap_id);
}

static bool
mmap_id_less (const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED)
{
  const struct mmap_id_elem *a = hash_entry (a_, struct mmap_id_elem, hash_elem);
  const struct mmap_id_elem *b = hash_entry (b_, struct mmap_id_elem, hash_elem);

  return a->mmap_id < b->mmap_id;
}

struct hash *mmap_id_create (void)
{
  struct hash *spd = malloc (sizeof (struct hash));

  hash_init (spd, mmap_id_hash, mmap_id_less, (void *) NULL);

  return spd;
}

int mmap_get_id	(void)
{
  static int id = 1;

  return id++;
}

void page_unmap (mapid_t mmap_id)
{
  struct mmap_id_elem *mmap_id_elem;
  struct hash_elem *hash_elem;
  struct thread *t = thread_current ();
  unsigned int i;

  /* Search for mmap_id element */
  struct mmap_id_elem mmap_search;
  mmap_search.mmap_id = mmap_id;
  hash_elem = hash_delete (t->mmap_id_dir, &mmap_search.hash_elem);
  mmap_id_elem = hash_entry (hash_elem, struct mmap_id_elem, hash_elem);
  if (mmap_id_elem == NULL)
    return;

  uint32_t upage = mmap_id_elem->upage;
  for (i = 0; i * PGSIZE < mmap_id_elem->size; i++) {
    /* Remove page from supplemental page directory */
    struct spt_elem spt_elem_search, *spt_elem;
    spt_elem_search.upage = upage + i * PGSIZE;
    hash_elem = hash_delete (t->supp_pagedir, &spt_elem_search.hash_elem);
    spt_elem = hash_entry (hash_elem, struct spt_elem, hash_elem);
    ASSERT (spt_elem != NULL);

    /* Write the page back to the file */
    void *kpage = pagedir_get_page (t->pagedir, (void *)(upage + i*PGSIZE));
    if ((kpage != NULL) && pagedir_is_dirty (t->pagedir, (void *)(upage + i*PGSIZE))) {
      file_write_at (spt_elem->file, kpage, spt_elem->read_bytes, spt_elem->file_offset);
    }

    /* (Maybe) remove page from page directory */
    pagedir_clear_page (t->pagedir, (void *)upage);

    free (spt_elem);
  }
  free (mmap_id_elem);
}
