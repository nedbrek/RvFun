#include "sparse_mem.hpp"
#include <cstring>
#include <iostream>

namespace rvfun
{
struct SparseMem::MemBlock
{
	uint64_t va;
	uint32_t sz;
	uint8_t *mem;

	MemBlock(uint64_t a, uint32_t s, const void *data)
	: va(a)
	, sz(s)
	{
		if (data)
		{
			mem = static_cast<uint8_t*>(malloc(sz));
			memcpy(mem, data, sz);
		}
		else
		{
			mem = static_cast<uint8_t*>(calloc(sz, 1));
		}
	}

	~MemBlock()
	{
		free(mem);
	}
};

SparseMem::SparseMem()
{
}

void SparseMem::addBlock(uint64_t va, uint32_t sz, const void *data)
{
	// check for overlap
	for (const auto &b : blocks_)
	{
		const uint64_t block_end = b->va + b->sz;
		if (block_end == va) // grow block
		{
			// TODO growing through gap

			// calculate new size
			sz += b->sz;

			// grow block
			b->mem = static_cast<uint8_t*>(realloc(b->mem, sz));

			// init new mem
			for (uint32_t a = b->sz; a < sz; ++a)
				b->mem[a] = 0;

			if (data)
			{
				// copy in new data
				const uint8_t *bdata = static_cast<const uint8_t*>(data);
				for (uint32_t i = 0; i < sz; ++i)
					b->mem[va - b->va + i] = bdata[i];
			}

			// update block size
			b->sz = sz;

			return; // done
		}
		// TODO other overlap cases
	}
	blocks_.emplace_back(new MemBlock(va, sz, data));
}

uint64_t SparseMem::readMem(uint64_t va, uint32_t sz) const
{
	uint64_t ret = 0;

	for (const auto &b : blocks_)
	{
		const uint64_t block_end = b->va + b->sz;
		// if block starts to the left, and ends past the beginning
		if (b->va <= va && block_end > va)
		{
			// if block covers all of access
			if (va + sz <= block_end)
			{
				const uint64_t offset = va - b->va;
				memcpy(&ret, b->mem + offset, sz);
				return ret;
			}
			//else
			std::cerr << " Cross block read" << std::endl; // TODO - handle
		}
	}

	std::cerr << " Access outside of allocated memory: "
		 << std::hex << va << std::dec << ' ' << sz << std::endl;

	return ret;
}

void SparseMem::writeMem(const uint64_t va, const uint32_t sz, const uint64_t val)
{
	for (const auto &b : blocks_)
	{
		const uint64_t block_end = b->va + b->sz;
		// if block starts to the left, and ends past the beginning
		if (b->va <= va && block_end > va)
		{
			// if block covers all of access
			if (va + sz <= block_end)
			{
				const uint64_t offset = va - b->va;
				memcpy(b->mem + offset, &val, sz);
				return;
			}
			//else
			std::cerr << " Cross block write" << std::endl; // TODO - handle
		}
	}

	std::cerr << " Write access outside of allocated memory: "
		 << std::hex << va << std::dec << ' ' << sz << std::endl;
}

}

