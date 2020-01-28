#ifndef RVFUN_SIMPLE_ARCH_STATE_HPP
#define RVFUN_SIMPLE_ARCH_STATE_HPP

#include "arch_state.hpp"

namespace rvfun
{
class ArchMem;

/// Simple Implementation of ArchState
class SimpleArchState : public ArchState
{
public:
	SimpleArchState();

	void setMem(ArchMem *mem) { mem_ = mem; }
	void setSys(System *sys) { sys_ = sys; }

	//---from ArchState
	uint64_t getReg(uint32_t num) const override
	{
		return num == 0 ? 0 : ireg[num];
	}

	void setReg(uint32_t num, uint64_t val) override
	{
		if (num != 0)
			ireg[num] = val;
	}

	uint64_t readMem(uint64_t va, uint32_t sz) const override;
	void writeMem(uint64_t va, uint32_t sz, uint64_t val) override;

	void incPc(int64_t delta) override
	{
		pc_ += delta;
	}

	uint64_t getPc() const override
	{
		return pc_;
	}

	void setPc(uint64_t pc) override
	{
		pc_ = pc;
	}

	System* getSys() override { return sys_; }
	const System* getSys() const override { return sys_; }

private:
	static constexpr uint32_t NUM_REGS = 32;
	uint64_t pc_ = 0;
	uint64_t ireg[NUM_REGS] = {0,};
	ArchMem *mem_ = nullptr;
	System *sys_ = nullptr;
};

}

#endif

