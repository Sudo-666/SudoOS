#pragma once
#include "../limine.h"
#include "pmm.h"

const char* memtype2str(uint64_t type);

void debug_memmap(struct limine_memmap_response *mmap);
