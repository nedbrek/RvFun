#include "simple_arch_state.hpp"
#include "arch_mem.hpp"

namespace
{
enum
{
	FFLAGS = 1,
	FRM = 2,
	FCSR = 3
};

union IntFloat
{
	uint32_t wa[2];
	uint64_t dw;
	float f;
	double d;
};
}

namespace rvfun
{
SimpleArchState::SimpleArchState()
{
	// TODO: preload some CREGS
}

float SimpleArchState::getFloat(uint32_t num) const
{
	IntFloat tmp;
	tmp.dw = freg[num];
	return tmp.f;
}

void SimpleArchState::setFloat(uint32_t num, float val)
{
	IntFloat tmp;
	tmp.f = val;
	tmp.wa[1] = 0xffffffff;

	freg[num] = tmp.dw;
	if (debug_)
		std::cout << " setFloat " << num << ' ' << val << ' ';
}

double SimpleArchState::getDouble(uint32_t num) const
{
	IntFloat tmp;
	tmp.dw = freg[num];
	return tmp.d;
}

void SimpleArchState::setDouble(uint32_t num, double val)
{
	IntFloat tmp;
	tmp.d = val;
	freg[num] = tmp.dw;
	if (debug_)
		std::cout << " setFloat " << num << ' ' << val << ' ';
}

uint16_t SimpleArchState::parentCsr(uint16_t csr) const
{
	uint16_t actual_csr = csr;
	if (csr == FFLAGS || csr == FRM) // subfields of fcsr
		actual_csr = FCSR;

	return actual_csr;
}

uint64_t SimpleArchState::getCr(const uint32_t csr) const
{
	auto i = cregs_.find(parentCsr(csr));
	if (i == cregs_.end())
		return 0;

	if (csr == FRM)
		return (i->second >> 5) & 7; // bits[7:5]
	else if (csr == FFLAGS)
		return i->second & 0x1f; // bits[4:0]

	return i->second;
}

void SimpleArchState::setCr(const uint32_t csr, uint64_t val)
{
	const uint16_t actual_csr = parentCsr(csr);
	auto i = cregs_.find(actual_csr);
	const bool found = i != cregs_.end();

	bool partial = false;
	uint64_t mask = ~uint64_t(-1);
	if (csr == FRM)
	{
		val = (val & 7) << 5; // mask and shift
		partial = true;
		mask = ~uint64_t(0xe0); // [7:5]
	}
	else if (csr == FFLAGS)
	{
		val &= 0x1f; // bottom bits
		partial = true;
		mask = ~uint64_t(0x1f); // [4:0]
	}

	if (found && partial)
	{
		uint64_t cur_val = i->second;
		cur_val &= mask; // clear target bits
		val |= cur_val;
	}

	if (!found)
	{
		cregs_.insert({actual_csr, val});
	}
	else
	{
		i->second = val;
	}
}

uint64_t SimpleArchState::readImem(uint64_t va, uint32_t sz) const
{
	return mem_->readMem(va, sz);
}

uint64_t SimpleArchState::readMem(uint64_t va, uint32_t sz) const
{
	const uint64_t val = mem_->readMem(va, sz);
	if (debug_)
		std::cout << " readMem " << std::hex << va << ' ' << sz << ' ' << val << std::dec;
	return val;
}

void SimpleArchState::writeMem(uint64_t va, uint32_t sz, uint64_t val)
{
	if (debug_)
		std::cout << " writeMem " << std::hex << va << ' ' << sz << ' ' << val << std::dec;
	mem_->writeMem(va, sz, val);
}

}

