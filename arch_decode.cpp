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

/// Compressed Jump to register
class CompJr : public Inst
{
public:
	CompJr(uint8_t rd)
	: rd_(rd)
	{
	}

	void execute(ArchState &state) const override
	{
		const uint64_t new_pc = state.getReg(rd_);
		state.setPc(new_pc);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << "C.JR r" << uint32_t(rd_);
		return os.str();
	}

private:
	uint8_t rd_;
};

/// Compressed Move (Reg to Reg)
class CompMv : public Inst
{
public:
	CompMv(uint8_t rs, uint8_t rd)
	: rs_(rs)
	, rd_(rd)
	{
	}

	void execute(ArchState &state) const override
	{
		state.setReg(rd_, state.getReg(rs_));
		state.incPc(2);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << "C.MV r" << uint32_t(rd_) << " = r" << uint32_t(rs_);
		return os.str();
	}

private:
	uint8_t rs_;
	uint8_t rd_;
};

/// Compressed Load DWord from Stack Pointer
class CompLdSp : public Inst
{
public:
	CompLdSp(uint64_t imm, uint8_t rd)
	: imm_(imm)
	, rd_(rd)
	{
	}

	void execute(ArchState &state) const override
	{
		const uint64_t ea = state.getReg(Reg::SP) + imm_;
		state.setReg(rd_, state.readMem(ea, 8));
		state.incPc(2);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << "C.LDSP r" << uint32_t(rd_) << " = [r2+" << imm_ << ']';
		return os.str();
	}

private:
	uint64_t imm_;
	uint8_t rd_;
};

/// Compressed Add Scaled Immediate to SP
class CompAddI4SpN : public Inst
{
public:
	CompAddI4SpN(uint64_t imm, uint8_t rd)
	: imm_(imm)
	, rd_(rd)
	{
	}

	void execute(ArchState &state) const override
	{
		const uint64_t sp = state.getReg(Reg::SP);
		state.setReg(rd_, sp + imm_);
		state.incPc(2);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << "C.ADDI4SPN r" << uint32_t(rd_) << " = r2+" << imm_;
		return os.str();
	}

private:
	uint64_t imm_;
	uint8_t rd_;
};

/// Compressed Add Scaled Immediate to SP
class CompAddI16Sp : public Inst
{
public:
	CompAddI16Sp(int64_t imm)
	: imm_(imm)
	{
	}

	void execute(ArchState &state) const override
	{
		const uint64_t sp = state.getReg(Reg::SP);
		state.setReg(Reg::SP, sp + imm_);
		state.incPc(2);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << "C.ADDI16SP SP += " << imm_;
		return os.str();
	}

private:
	int64_t imm_;
};

/// Compressed Store DWord to Stack Pointer
class CompSdSp : public Inst
{
public:
	CompSdSp(uint32_t imm, uint8_t rs)
	: imm_(imm)
	, rs_(rs)
	{
	}

	void execute(ArchState &state) const override
	{
		const uint64_t sp = state.getReg(Reg::SP);
		const uint64_t val = state.getReg(rs_);
		state.writeMem(sp + imm_, 8, val);
		state.incPc(2);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << "C.SDSP [SP+" << imm_ << "] = r" << uint32_t(rs_);
		return os.str();
	}

private:
	uint32_t imm_;
	uint8_t rs_;
};

/// Compressed Shift Left Immediate
class CompSllI : public Inst
{
public:
	CompSllI(uint8_t sft, uint8_t rd)
	: sft_(sft)
	, rd_(rd)
	{
	}

	void execute(ArchState &state) const override
	{
		const uint64_t val = state.getReg(rd_);
		state.setReg(rd_, val << sft_);
		state.incPc(2);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << "C.SLLI r" << uint32_t(rd_) << " <<= " << uint32_t(sft_);
		return os.str();
	}

private:
	uint8_t sft_;
	uint8_t rd_;
};

/// Compressed Add (reg to reg)
class CompAdd : public Inst
{
public:
	CompAdd(uint8_t rs, uint8_t rd)
	: rs_(rs)
	, rd_(rd)
	{
	}

	void execute(ArchState &state) const override
	{
		const uint64_t vrd = state.getReg(rd_);
		const uint64_t vrs = state.getReg(rs_);

		state.setReg(rd_, vrd + vrs);

		state.incPc(2);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << "C.ADD r" << uint32_t(rd_) << " += r" << uint32_t(rs_);
		return os.str();
	}

private:
	uint8_t rs_;
	uint8_t rd_;
};

/// Compressed Add Immediate
class CompAddI : public Inst
{
public:
	CompAddI(int64_t imm, uint8_t rd)
	: imm_(imm)
	, rd_(rd)
	{
	}

	void execute(ArchState &state) const override
	{
		const uint64_t vrd = state.getReg(rd_);

		state.setReg(rd_, vrd + imm_);

		state.incPc(2);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << "C.ADDI r" << uint32_t(rd_) << " += " << imm_;
		return os.str();
	}

private:
	int64_t imm_;
	uint8_t rd_;
};

Inst* decode16(uint32_t opc)
{
	const uint8_t o10 = opc & 3; // opc[1:0]
	const uint8_t rd = (opc >> 7) & 0x1f; // opc[11:7]
	const uint8_t rs = (opc >> 2) & 0x1f; // opc[6:2]
	const uint16_t o15_13 = opc & 0xe000; // opc[15:13]

	if (o10 == 0) // Memory
	{
		if (o15_13 == 0) // C.ADDI4SPN
		{
			const uint8_t rd = ((opc >> 2) & 7) + 8; // opc[4:2]
			uint64_t imm = (opc & 0x780) >> 1; // opc[10:7] -> imm[9:6]
			imm |= (opc & 0x1800) >> 7; // opc[12:11] -> imm[5:4]
			imm |= (opc & 0x40) ? 4 : 0; // opc[6] -> imm[2]
			imm |= (opc & 0x20) ? 8 : 0; // opc[5] -> imm[3]
			return new CompAddI4SpN(imm, rd);
		}
	}
	else if (o10 == 1) // common compressed ops
	{
		if (o15_13 == 0x0000) // C.ADDI (and C.NOP)
		{
			uint8_t raw_bits = (opc >> 2) & 0x1f; // opc[6:2] -> imm[4:0]
			if (opc & 0x1000) // opc[12] -> imm[31:5] (sign ex)
				raw_bits |= 0xe0;

			const int8_t imm = raw_bits;
			return new CompAddI(imm, rd);
		}
		if (o15_13 == 0x4000)
		{
			// TODO check for hint

			uint8_t raw_bits = (opc >> 2) & 0x1f; // imm[4:0] = opc[6:2]
			if (opc & 0x1000)
				raw_bits |= 0xe0;

			const int8_t imm = raw_bits;
			return new CompLI(rd, imm);
		}
		else if (o15_13 == 0x6000 || o15_13 == 0x7000) // C.LUI, C.ADDI16SP
		{
			if (rd == 2)
			{
				uint16_t imm = (opc & 0x18) << 4; // opc[4:3] -> imm[8:7]
				imm |= (opc & 0x20) ? 0x40 : 0; // opc[5] -> imm[6]
				imm |= (opc &    4) ? 0x20 : 0; // opc[2] -> imm[5]
				if (opc & 0x1000)
					imm |= 0xfe00; // sign ex
				const int16_t s_imm = int16_t(imm);
				return new CompAddI16Sp(s_imm);
			}
			//else
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
	else if (o10 == 2) // more ops
	{
		const uint16_t o15_12 = opc & 0xf000; // opc[15:12]

		if (o15_12 < 0x2000) // C.SLLI
		{
			uint8_t sft = (opc >> 2) & 0x1f; // opc[6:2] -> sft[4:0]
			sft |= (opc >> 7) & 0x20; // opc[12] -> sft[5]
			return new CompSllI(sft, rd);
		}
		else if (o15_12 == 0x6000 || o15_12 == 0x7000) // C.LDSP
		{
			uint64_t imm = (opc & 0x1c) << 4; // opc[4:2] -> imm[8:6]
			if (opc & 0x1000) // opc[12] -> imm[5]
				imm |= 0x10;

			return new CompLdSp(imm, rd);
		}
		else if (o15_12 == 0x8000) // C.JR and C.MV
		{
			if (rs == 0)
				return new CompJr(rd);
			//else
			return new CompMv(rs, rd);
		}
		else if (o15_12 == 0x9000) // C.EBREAK, C.JALR, C.ADD
		{
			if (rd != 0 && rs != 0)
				return new CompAdd(rs, rd);
		}
		else if (o15_12 == 0xe000 || o15_12 == 0xf000) // C.SDSP
		{
			uint16_t imm = (opc >> 1) & 0x1c0; // opc[9:7] -> imm[8:6]
			const uint16_t low_imm = (opc >> 10) & 7; // opc[12:10]
			imm |= low_imm << 3; // imm[5:3]
			return new CompSdSp(imm, rs);
		}
	}
	return nullptr;
}

/// Add Upper Immediate to PC
class Auipc : public Inst
{
public:
	Auipc(int32_t imm, uint8_t rd)
	: imm_(imm)
	, rd_(rd)
	{
	}

	void execute(ArchState &state) const override
	{
		const uint64_t pc = state.getPc();
		state.setReg(rd_, pc + imm_);
		state.incPc(4);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << "AUIPC r" << uint32_t(rd_) << " = PC " << std::hex;
		if (imm_ < 0)
			os << '-' << ' ' << -imm_;
		else
			os << '+' << ' ' << imm_;

		return os.str();
	}

private:
	int64_t imm_;
	uint8_t rd_;
};

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

class OpImm : public Inst
{
public:
	OpImm(uint8_t op, int16_t imm, uint8_t r1, uint8_t rd)
	: op_(op)
	, imm_(imm)
	, r1_(r1)
	, rd_(rd)
	{
	}

	void execute(ArchState &state) const override
	{
		uint64_t val = state.getReg(r1_);

		// signed/unsigned versions
		const int64_t ival = int64_t(val);
		const uint64_t uimm = uint64_t(imm_);

		switch (op_)
		{
		case 0: val  += imm_; break; // ADDI
		case 1: val <<= imm_; break; // SLLI
		case 4: val  ^= imm_; break; // XORI
		case 6: val  |= imm_; break; // ORI
		case 7: val  &= imm_; break; // ANDI

		case 2: val = (ival < imm_) ? 1 : 0; break; // SLTI
		case 3: val = (val < uimm) ? 1 : 0; break; // SLTIU

		case 5: // SRLI, SRAI
		{
			const uint8_t sft = imm_ & 0x3f;
			const bool arith = (imm_ & 0x400) != 0;
			if (arith)
				val = ival >> sft;
			else
				val >>= sft;

			break;
		}
		}

		state.setReg(rd_, val);
		state.incPc(4);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		int64_t imm = imm_;

		const char *op;
		switch (op_)
		{
		case 0: os << "ADDI"; op = "+"; break;
		case 1: os << "SLLI"; op = "<<"; break;
		case 4: os << "XORI"; op = "^"; break;
		case 6: os << " ORI"; op = "|"; break;
		case 7: os << "ANDI"; op = "&"; break;

		case 2: os << "SLTI "; op = "<i"; break;
		case 3: os << "SLTIU"; op = "<u"; break;

		case 5: // SRLI, SRAI
		{
			imm &= 0x3f;
			const bool arith = (imm_ & 0x400) != 0;
			if (arith)
			{
				os << "SRAI";
				op = ">>i";
			}
			else
			{
				os << "SRLI";
				op = ">>u";
			}

			break;
		}
		}

		os << ' ' << 'r' << uint32_t(rd_) << " = r" << uint32_t(r1_) << ' ' << op << ' ' << imm;

		return os.str();
	}

private:
	uint8_t op_;
	int64_t imm_;
	uint8_t r1_;
	uint8_t rd_;
};

/// Load Upper Immediate
class Lui : public Inst
{
public:
	Lui(int64_t imm, uint8_t rd)
	: imm_(imm)
	, rd_(rd)
	{
	}

	void execute(ArchState &state) const override
	{
		state.setReg(rd_, imm_);
		state.incPc(4);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << "LUI r" << uint32_t(rd_) << " = " << imm_;
		return os.str();
	}

private:
	int64_t imm_;
	uint8_t rd_;
};

/// Conditional Branch
class Branch : public Inst
{
public:
	Branch(int64_t imm, uint8_t op, uint8_t r2, uint8_t r1)
	: imm_(imm)
	, op_(op)
	, r2_(r2)
	, r1_(r1)
	{
	}

	void execute(ArchState &state) const override
	{
		const uint64_t r1 = state.getReg(r1_);
		const uint64_t r2 = state.getReg(r2_);

		bool taken = false;
		switch (op_)
		{
		case 0: taken = r1 == r2; break; // BEQ
		case 1: taken = r1 != r2; break; // BNE
		case 4: taken = int64_t(r1) < int64_t(r2); break; // BLT
		case 5: taken = int64_t(r1) >= int64_t(r2); break; // BGE
		case 6: taken = r1 < r2; break; // BLTU
		case 7: taken = r1 >= r2; break; // BGEU
		}
		// can reduce half the cases with: taken ^= (op & 1);

		const uint64_t pc = state.getPc();
		if (taken)
			state.setPc(pc + imm_);
		else
			state.setPc(pc + 4);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << 'B';
		switch (op_)
		{
		case 0: os << "EQ"; break; // BEQ
		case 1: os << "NE"; break; // BNE
		case 4: os << "LT"; break; // BLT
		case 5: os << "GE"; break; // BGE
		case 6: os << "LTU"; break; // BLTU
		case 7: os << "GEU"; break; // BGEU
		}
		os << ' ' << 'r' << uint32_t(r1_) << ", r" << uint32_t(r2_) << ", " << imm_;
		return os.str();
	}

private:
	int64_t imm_;
	uint8_t op_;
	uint8_t r2_;
	uint8_t r1_;
};

/// Store
class Store : public Inst
{
public:
	Store(uint8_t sz, int32_t imm, uint8_t r1, uint8_t r2)
	: sz_(sz)
	, imm_(imm)
	, r1_(r1)
	, r2_(r2)
	{
	}

	void execute(ArchState &state) const override
	{
		const uint64_t ea = state.getReg(r1_) + imm_;
		const uint64_t val = state.getReg(r2_);
		state.writeMem(ea, 1 << sz_, val);
		state.incPc(4);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		const char sz_str[] = {'B', 'H', 'W', 'D'};
		os << 'S' << sz_str[sz_] << " [r" << uint32_t(r1_);

		if (imm_ < 0)
			os << '-' << -imm_;
		else
			os << '+' << imm_;
		os << "] = r" << uint32_t(r2_);

		return os.str();
	}

private:
	uint8_t sz_;
	int64_t imm_;
	uint8_t r1_;
	uint8_t r2_;
};

Inst* decode32(uint32_t opc)
{
	// opc[1:0] == 2'b11
	const uint32_t group = opc & 0x7c; // opc[6:2]
	const uint8_t rd = (opc >> 7) & 0x1f; //opc[11:7]
	const uint8_t r1 = (opc >> 15) & 0x1f; // opc[19:15]
	const uint8_t r2 = (opc >> 20) & 0x1f; // opc[24:20]

	switch (group)
	{
	case   0: // load
	case   4: // load FP
	case  36: // store FP
	case  12: // misc MEM
		return nullptr; // TODO Mem

	case  32: // store
	{
		uint16_t imm = rd; // opc[11:7] -> imm[4:0]
		imm |= (opc >> 20) & 0xfe0; // opc[31:25] -> imm[11:5]
		if (imm & 0x800)
			imm |= 0xf000; // sign extend from bit 11

		const int16_t s_imm = int16_t(imm);
		const uint8_t sz = (opc >> 12) & 7; // opc[14:12]
		return new Store(sz, s_imm, r1, r2);
	}

	case  20: // AUIPC
	{
		const uint32_t imm = opc & 0xfffff000; // opc[31:12]
		return new Auipc(imm, rd);
	}

	case  16: // op imm
	{
		const uint8_t op = (opc >> 12) & 7; // opc[14:12]
		uint16_t imm = (opc >> 20) & 0xfff; // opc[31:20]
		if (imm & 0x800)
			imm |= 0xf000; // sign ex
		return new OpImm(op, imm, r1, rd);
	}

	case  24: // op imm32
	case  48: // op
	case  56: // op32
		return nullptr; // TODO int

	case  52: // LUI
	{
		const int32_t imm = opc & 0xfffff000; // opc[31:12]
		return new Lui(imm, rd);
	}

	case  64: // MADD
	case  68: // MSUB
	case  72: // NMSUB
	case  76: // NMADD
	case  80: // fp op
		return nullptr; // TODO FP

	case  96: // branch
	{
		uint32_t imm = (opc >> 7) & 0x1e; // opc[11:8] -> imm[4:1]
		imm |= (opc >> 20) & 0x7e0; // opc[30:25] -> imm[10:5]
		if (opc & 0x80)
			imm |= 0x800; // opc[7] -> imm[11]
		if (opc & 0x80000000)
			imm |= 0xfffff000; // sign ex imm[31:12] from opc[31]

		const int32_t s_imm = imm;
		const uint8_t op = (opc >> 12) & 7; // opc[14:12]
		return new Branch(s_imm, op, r2, r1);
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

