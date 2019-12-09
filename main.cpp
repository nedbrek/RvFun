#include <iostream>
#include <sstream>
#include <iomanip>

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

Inst* decode16(uint32_t opc)
{
	const uint8_t o21 = opc & 3;
	if (o21 == 1) // common compressed ops
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
	}
	return nullptr;
}

Inst* decode32(uint32_t opc)
{
	return nullptr;
}

int main(int argc, char **argv)
{
	SimpleArchState state;

	// li a1, -4
	// li a2, 1
	// addw a2 = a2 + a1
	const uint16_t opcodes[] = {
		0x55f1,
		0x4605,
		0x9e2d
	};
	uint32_t opcodes_sz = 6;

	for (; state.getPc() < opcodes_sz;)
	{
		const uint64_t pc = state.getPc();
		const uint16_t opc = opcodes[pc >> 1];
		uint32_t opc_sz = 2;

		uint32_t full_inst = opc;
		Inst *inst = nullptr;
		if ((opc & 3) == 3)
		{
			// 32 bit instruction
			opc_sz = 4;
			full_inst |= opcodes[(pc+2) >> 1] << 16;

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
			std::cout << "(null inst)";
			state.incPc(opc_sz);
		}
		else
		{
			std::cout << inst->disasm();
			inst->execute(state);
		}

		std::cout << std::endl;
	}

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

