#ifndef RVFUN_SPARSE_MEM_HPP
#define RVFUN_SPARSE_MEM_HPP

#include "arch_mem.hpp"
#include <vector>

namespace rvfun
{
/// Sparse array implementation of Architected Memory
class SparseMem : public ArchMem
{
public:
	SparseMem();

	void addBlock(uint64_t va, uint32_t sz, const void *data = nullptr);

	uint64_t readMem(uint64_t va, uint32_t sz) const override;

	void writeMem(uint64_t va, uint32_t sz, uint64_t val) override;

private:
	struct MemBlock;
	std::vector<MemBlock*> blocks_;
};

} // namespace

#endif

