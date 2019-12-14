#include "inst.hpp"
#include "arch_state.hpp"
#include <sstream>

namespace rvfun
{

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

/// Jump and Link (imm)
class Jal : public Inst
{
public:
	Jal(int64_t imm, uint8_t rd)
	: imm_(imm)
	, rd_(rd)
	{
	}

	void execute(ArchState &state) const override
	{
		const uint64_t pc = state.getPc();
		state.setReg(rd_, pc + 4);
		state.incPc(imm_);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << "JAL r" << uint32_t(rd_) << ", " << imm_;
		return os.str();
	}

private:
	int64_t imm_;
	uint8_t rd_;
};

class Jalr : public Inst
{
public:

private:
};

class Branch : public Inst
{
public:

private:
};

Inst* decode32(uint32_t opc)
{
	// opc[1:0] == 2'b11
	const uint32_t group = opc & 0x7c; // opc[6:2]
	switch (group)
	{
	case   0: // load
	case   4: // load FP
	case  32: // store
	case  36: // store FP
	case  12: // misc MEM
		return nullptr; // TODO Mem

	case  16: // op imm
	case  24: // op imm32
	case  48: // op
	case  56: // op32
	case  20: // AUIPC
	case  52: // LUI
		return nullptr; // TODO int

	case  64: // MADD
	case  68: // MSUB
	case  72: // NMSUB
	case  76: // NMADD
	case  80: // fp op
		return nullptr; // TODO FP

	case  96: // branch
	{
		//return new Branch();
		return nullptr; // TODO
	}

	case 100: // JALR
	{
		//return new Jalr;
		return nullptr; // TODO
	}

	case 108: // JAL
	{
		uint64_t imm = 0; // imm[0] = 0
		const uint32_t opc30_21 = (opc >> 21) & 0x3ff; // opc[30:21] -> imm[10:1]
		imm |= opc30_21 << 1;
		imm |= ((opc >> 20) & 1) << 11; // opc[20] -> imm[11]
		const uint32_t opc19_12 = (opc >> 12) & 0xff; // opc[19:12] -> imm[19:12]
		imm |= opc19_12 << 12;
		if ((opc >> 31) & 1) // opc[31] sign bit
			imm |= 0xfffffffffff00000;

		const uint8_t rd = (opc >> 7) & 0x1f; //opc[11:7]
		return new Jal(imm, rd);
	}

	case  44: // AMO (atomics)
	case 112: // system
		return nullptr; // TODO

	case   8: // custom0
	case  40: // custom1
	case  88: // custom2
	case 120: // custom3
	case  28: // >32 bit opcode
	case  60: // >32 bit opcode
	case  92: // >32 bit opcode
	case 124: // >32 bit opcode
	case  84: // (rsvd)
	case 104: // (rsvd)
	case 116: // (rsvd)
		return nullptr; // TODO InvalidOp
	}

	return nullptr;
}

} // namespace

