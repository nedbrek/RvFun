#ifndef RVFUN_ARCH_STATE_HPP
#define RVFUN_ARCH_STATE_HPP

#include <cstdint>

namespace rvfun
{
class System;

/// Interface to Architected State
class ArchState
{
public:
	virtual ~ArchState() = default;

	virtual uint64_t getReg(uint32_t num) const = 0;
	virtual void     setReg(uint32_t num, uint64_t val) = 0;

	virtual double getFloat(uint32_t num) const = 0;
	virtual void   setFloat(uint32_t num, double val) = 0;

	virtual uint64_t getCr(uint32_t num) const = 0;
	virtual void     setCr(uint32_t num, uint64_t val) = 0;

	/// read instruction memory (no side effects, not coherent)
	virtual uint64_t readImem(uint64_t va, uint32_t sz) const = 0;
	virtual uint64_t readMem(uint64_t va, uint32_t sz) const = 0;
	virtual void     writeMem(uint64_t va, uint32_t sz, uint64_t val) = 0;

	/// update PC
	virtual void incPc(int64_t delta = 4) = 0;

	/// get PC
	virtual uint64_t getPc() const = 0;
	virtual void setPc(uint64_t pc) = 0;

	virtual System* getSys() = 0;
	virtual const System* getSys() const = 0;
};

namespace Reg
{
	enum
	{
		RA = 1, ///< Return Address (aka Link Register)
		SP = 2  ///< Stack Pointer
	};
}

} // namespace

#endif

