#include <hash.h>
#include "filesys/off_t.h"
#include "lib/user/syscall.h"

struct spt_elem {
	struct hash_elem hash_elem;

	struct file *file;
	off_t file_offset;
	uint32_t upage;
	uint32_t read_bytes;
	uint32_t zero_bytes;
	bool writeable;
};

struct mmap_id_elem {
	struct hash_elem hash_elem;

	int mmap_id;
	uint32_t upage;
	uint32_t size;
};

struct hash *supp_pagedir_create (void);
struct hash *mmap_id_create (void);

int mmap_get_id (void);

bool page_load (void *addr, void *esp);

void page_unmap (mapid_t mmap_id);
