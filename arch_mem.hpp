#ifndef RVFUN_ARCH_MEM_HPP
#define RVFUN_ARCH_MEM_HPP

#include <cstdint>

namespace rvfun
{

/// Interface to Architected Memory
class ArchMem
{
public:
	virtual uint64_t readMem(uint64_t va, uint32_t sz) const = 0;
	virtual void    writeMem(uint64_t va, uint32_t sz, uint64_t val) = 0;
};

} // namespace

#endif

