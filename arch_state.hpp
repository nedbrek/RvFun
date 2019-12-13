#ifndef RVFUN_ARCH_STATE_HPP
#define RVFUN_ARCH_STATE_HPP

#include <cstdint>

namespace rvfun
{

/// Interface to Architected State
class ArchState
{
public:
	virtual uint64_t getReg(uint32_t num) const = 0;
	virtual void     setReg(uint32_t num, uint64_t val) = 0;

	/// update PC
	virtual void incPc(int64_t delta = 4) = 0;

	/// get PC
	virtual uint64_t getPc() const = 0;
};

} // namespace

#endif

