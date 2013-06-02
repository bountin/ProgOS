#include <hash.h>
#include "filesys/off_t.h"

struct spt_elem {
	struct hash_elem hash_elem;

	struct file *file;
	off_t file_offset;
	uint32_t upage;
	uint32_t read_bytes;
	uint32_t zero_bytes;
	bool writeable;
};

struct hash *supp_pagedir_create (void);

void page_load (void *addr);
