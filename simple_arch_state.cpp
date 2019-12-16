#include "simple_arch_state.hpp"
#include "arch_mem.hpp"

namespace rvfun
{
SimpleArchState::SimpleArchState()
{
}

uint64_t SimpleArchState::readMem(uint64_t va, uint32_t sz) const
{
	return mem_->readMem(va, sz);
}

void SimpleArchState::writeMem(uint64_t va, uint32_t sz, uint64_t val)
{
	mem_->writeMem(va, sz, val);
}

}

