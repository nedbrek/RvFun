#include "sparse_mem.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
using namespace rvfun;

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

/// Interface to one architected instruction
class Inst
{
public:
	/// update 'state' for execution of this
	virtual void execute(ArchState &state) const = 0;

	///@return assembly string of this
	virtual std::string disasm() const = 0;
};

/// (Compressed) Load Immediate (add rd = r0 + i)
class CompLI : public Inst
{
public:
	CompLI(uint8_t rd = 0, int8_t imm = 0)
	: imm_(imm)
	, rd_(rd)
	{
	}

	void execute(ArchState &state) const override
	{
		state.setReg(rd_, imm());
		state.incPc(2);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << "C.LI r" << uint32_t(rd_) << " = " << imm();
		return os.str();
	}

private: // methods
	int64_t imm() const
	{
		return imm_;
	}

private: // data
	int8_t imm_;
	uint8_t rd_;
};

/// Compressed ALU functions (C.SUB through C.AND)
class CompAlu : public Inst
{
public:
	CompAlu(uint8_t fun = 0, uint8_t r2 = 0, uint8_t rsd = 0)
	: fun_(fun)
	, r2_(r2)
	, rsd_(rsd)
	{
	}

	void execute(ArchState &state) const override
	{
		const uint64_t rs = state.getReg(rsd_);
		const uint64_t r2 = state.getReg(r2_);

		uint64_t v = 0;
		switch (fun_)
		{
		case 0: v = rs - r2; break;
		case 1: v = rs ^ r2; break;
		case 2: v = rs | r2; break;
		case 3: v = rs & r2; break;
		}

		state.setReg(rsd_, v);
		state.incPc(2);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << "C.";
		char op = ' ';
		switch (fun_)
		{
		case 0: os << "SUB"; op = '-'; break;
		case 1: os << "XOR"; op = '^'; break;
		case 2: os << "OR "; op = '|'; break;
		case 3: os << "AND"; op = '&'; break;
		}
		os << ' ' << 'r' << uint32_t(rsd_) << ' ' << op << "= r" << uint32_t(r2_);
		return os.str();
	}

private:
	uint8_t fun_;
	uint8_t r2_;
	uint8_t rsd_;
};

/// Compressed ALU Word functions (C.SUBW and C.ADDW)
class CompAluW : public Inst
{
public:
	CompAluW(uint8_t fun = 0, uint8_t r2 = 0, uint8_t rsd = 0)
	: fun_(fun)
	, r2_(r2)
	, rsd_(rsd)
	{
	}

	void execute(ArchState &state) const override
	{
		const uint32_t rs = state.getReg(rsd_);
		const uint32_t r2 = state.getReg(r2_);

		uint32_t v = 0;
		switch (fun_)
		{
		case 0: v = rs - r2; break;
		case 1: v = rs + r2; break;
		//case 2: //rsvd
		//case 3: //rsvd
		}

		state.setReg(rsd_, v);
		state.incPc(2);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << "C.";
		char op = ' ';
		switch (fun_)
		{
		case 0: os << "SUBW"; op = '-'; break;
		case 1: os << "ADDW"; op = '+'; break;
		//case 2: //rsvd
		//case 3: //rsvd
		}
		os << ' ' << 'r' << uint32_t(rsd_) << ' ' << op << "= r" << uint32_t(r2_);
		return os.str();
	}

private:
	uint8_t fun_;
	uint8_t r2_;
	uint8_t rsd_;
};

Inst* decode16(uint32_t opc)
{
	const uint8_t o10 = opc & 3;
	if (o10 == 1) // common compressed ops
	{
		const uint16_t o15_13 = opc & 0xe000;
		if (o15_13 == 0x4000)
		{
			// TODO check for hint
			const uint8_t rd = (opc >> 7) & 0x1f;

			uint8_t raw_bits = (opc >> 2) & 0x1f; // imm[4:0] = opc[6:2]
			if (opc & 0x1000)
				raw_bits |= 0xe0;

			const int8_t imm = raw_bits;
			return new CompLI(rd, imm);
		}
		else if (o15_13 == 0x8000)
		{
			const uint16_t op_11_10 = opc & 0x0c00; // opc[11:10]
			if (op_11_10 == 0x0c00)
			{
				// rd op= r2
				const uint8_t rsd = ((opc >> 7) & 7) + 8; // opc[9:7]
				const uint8_t rs2 = ((opc >> 2) & 7) + 8; // opc[4:2]
				const uint8_t fun = (opc >> 5) & 3; // opc[6:5]
				if (opc & 0x1000)
				{
					// 32 bit form
					return new CompAluW(fun, rs2, rsd);
				}
				else
				{
					return new CompAlu(fun, rs2, rsd);
				}
			}
		}
	}
	return nullptr;
}

Inst* decode32(uint32_t opc)
{
	return nullptr;
}

int main(int argc, char **argv)
{
	SparseMem mem;
	SimpleArchState state;

	// li a1, -4
	// li a2, 1
	// addw a2 = a2 + a1
	const uint16_t opcodes[] = {
		0x55f1,
		0x4605,
		0x9e2d,
		0
	};
	uint32_t opcodes_sz = 8;
	mem.addBlock(0, 8, &opcodes[0]);

	for (; state.getPc() < opcodes_sz;)
	{
		const uint64_t pc = state.getPc();
		const uint16_t opc = mem.readMem(pc, 2);
		uint32_t opc_sz = 2;

		uint32_t full_inst = opc;
		Inst *inst = nullptr;
		if ((opc & 3) == 3)
		{
			// 32 bit instruction
			opc_sz = 4;
			full_inst |= mem.readMem(pc + 2, 2) << 16;

			inst = decode32(full_inst);
		}
		else
			inst = decode16(full_inst);

		std::cout << std::hex
		          << std::setw(12) << pc << ' '
		          << std::setw(8) << full_inst << ' '
					 << std::dec;

		if (!inst)
		{
			std::cout << "(null inst)(" << std::hex << full_inst << std::dec << ')';
			state.incPc(opc_sz);
		}
		else
		{
			std::cout << inst->disasm();
			inst->execute(state);
		}

		std::cout << std::endl;
	}

	// dump architected state
	std::cout << std::endl << "Architected State" << std::endl;
	for (uint32_t i = 0; i < 32;)
	{
		for (uint32_t j = 0; j < 4; ++j, ++i)
		{
			std::cout << std::setw(2) << i << ' '
			          << std::hex << std::setw(16) << state.getReg(i) << std::dec << ' ';
		}
		std::cout << std::endl;
	}

	return 0;
}

