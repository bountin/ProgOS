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

bool page_load (void *addr)
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
    file_seek (spt->file, spt->file_offset);
    if (file_read (spt->file, kpage, spt->read_bytes) != (int) spt->read_bytes) {
      palloc_free_page (kpage);
      return false;
    }
    memset (kpage + spt->read_bytes, 0, spt->zero_bytes);
    writeable = spt->writeable;
  } else {
    return false;
    memset (kpage, 0, PGSIZE);
    writeable = true;
  }

  /* Add the page to the process's address space. */
  if (!install_page (spt->upage, kpage, writeable)) {
    palloc_free_page (kpage);
    return false;
  }

  return true;
}
