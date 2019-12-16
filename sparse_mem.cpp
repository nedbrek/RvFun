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
			mem = reinterpret_cast<uint8_t*>(malloc(sz));
			memcpy(mem, data, sz);
		}
		else
		{
			mem = reinterpret_cast<uint8_t*>(calloc(sz, 1));
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
	// TODO - check for overlap
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
			std::cerr << "Cross block access" << std::endl; // TODO - handle
		}
	}

	std::cerr << "Access outside of allocated memory: "
		 << std::hex << va << ' ' << sz << std::endl;

	return ret;
}

void SparseMem::writeMem(uint64_t va, uint32_t sz, uint64_t val)
{
	// TODO
}

}

