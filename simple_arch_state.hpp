#ifndef RVFUN_SIMPLE_ARCH_STATE_HPP
#define RVFUN_SIMPLE_ARCH_STATE_HPP

#include "arch_state.hpp"

namespace rvfun
{

/// Simple Implementation of ArchState
class SimpleArchState : public ArchState
{
public:
	SimpleArchState()
	{
	}

	uint64_t getReg(uint32_t num) const override
	{
		return num == 0 ? 0 : ireg[num];
	}

	void setReg(uint32_t num, uint64_t val) override
	{
		if (num != 0)
			ireg[num] = val;
	}

	void incPc(int64_t delta) override
	{
		pc_ += delta;
	}

	uint64_t getPc() const override
	{
		return pc_;
	}

private:
	static constexpr uint32_t NUM_REGS = 32;
	uint64_t pc_ = 0;
	uint64_t ireg[NUM_REGS] = {0,};
	// TODO memory
};

}

#endif

