#include <iostream>
#include <sstream>
#include <iomanip>

class ArchState
{
public:
	virtual uint64_t getReg() const = 0;
	virtual uint64_t setReg() const = 0;
};

class Inst
{
public:
	virtual void execute(ArchState &state) const = 0;
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
		// TODO
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
	// li a1, -4
	// li a2, 1
	// addw a2 = a2 + a1
	const uint16_t opcodes[] = {
		0x55f1,
		0x4605,
		0x9e2d
	};
	uint32_t opcodes_sz = 3;

	uint64_t pc = 0; // pc[63:1]
	for (; pc < opcodes_sz; ++pc)
	{
		const uint16_t opc = opcodes[pc];

		uint32_t full_inst = opc;
		Inst *inst = nullptr;
		if ((opc & 3) == 3)
		{
			// 32 bit instruction
			++pc;
			full_inst |= opcodes[pc] << 16;

			inst = decode32(full_inst);
		}
		else
			inst = decode16(full_inst);

		std::cout << std::hex << std::setw(8) << full_inst << std::dec << ' ';

		if (!inst)
			std::cout << "(null inst)";
		else
			std::cout << inst->disasm();

		std::cout << std::endl;
	}

	return 0;
}

