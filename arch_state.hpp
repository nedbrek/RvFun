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

	virtual uint64_t readMem(uint64_t va, uint32_t sz) const = 0;
	virtual void     writeMem(uint64_t va, uint32_t sz, uint64_t val) = 0;

	/// update PC
	virtual void incPc(int64_t delta = 4) = 0;

	/// get PC
	virtual uint64_t getPc() const = 0;
	virtual void setPc(uint64_t pc) = 0;
};

namespace Reg
{
	enum
	{
		SP = 2
	};
}

} // namespace

#endif

