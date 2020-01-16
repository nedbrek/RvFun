#include "inst.hpp"
#include "arch_state.hpp"
#include "system.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace
{
	// 8 character mnemonics
	constexpr uint32_t MNE_WIDTH = 8;

	std::basic_ostream<char>& printReg(std::ostream &os, uint8_t r)
	{
		return os << 'r' << std::left << std::setw(2) << uint32_t(r);
	}
}

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
		os << "C.LI       ";
		printReg(os, rd_) << " = " << imm();
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
		case 0: os << "SUB     "; op = '-'; break;
		case 1: os << "XOR     "; op = '^'; break;
		case 2: os << "OR      "; op = '|'; break;
		case 3: os << "AND     "; op = '&'; break;
		}
		os << ' ';
		printReg(os, rsd_) << ' ' << op << "= r" << uint32_t(r2_);
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

		uint32_t v = 0; // TODO: sign extend to 64
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
		case 0: os << "SUBW    "; op = '-'; break;
		case 1: os << "ADDW    "; op = '+'; break;
		//case 2: //rsvd
		//case 3: //rsvd
		}
		os << ' ';
		printReg(os, rsd_) << ' ' << op << "= r" << uint32_t(r2_);
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
		os << "C.JR       ";
		printReg(os, rd_);
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
		os << "C.MV       ";
		printReg(os, rd_) << " = r" << uint32_t(rs_);
		return os.str();
	}

private:
	uint8_t rs_;
	uint8_t rd_;
};

/// Compressed Load (D)Word from Stack Pointer
class CompLdwSp : public Inst
{
public:
	CompLdwSp(uint64_t imm, uint8_t rd, uint8_t sz)
	: imm_(imm)
	, rd_(rd)
	, sz_(sz)
	{
	}

	void execute(ArchState &state) const override
	{
		const uint64_t ea = state.getReg(Reg::SP) + imm_;
		state.setReg(rd_, state.readMem(ea, sz_));
		state.incPc(2);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << "C.L"; // 1 char
		os << (sz_ == 4 ? 'W' : 'D'); // 2 chars
		os << std::left << std::setw(MNE_WIDTH-2) << "SP" << ' ';
		printReg(os, rd_) << " = [r2+" << imm_ << ']';
		return os.str();
	}

private:
	uint64_t imm_;
	uint8_t rd_;
	uint8_t sz_;
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
		os << "C.ADDI4SPN ";
		printReg(os, rd_) << " = r2+" << imm_;
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

/// Compressed Store (D)Word to Stack Pointer
class CompSdwSp : public Inst
{
public:
	CompSdwSp(uint32_t imm, uint8_t rs, uint8_t sz)
	: imm_(imm)
	, rs_(rs)
	, sz_(sz)
	{
	}

	void execute(ArchState &state) const override
	{
		const uint64_t sp = state.getReg(Reg::SP);
		const uint64_t val = state.getReg(rs_);
		state.writeMem(sp + imm_, sz_, val);
		state.incPc(2);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << "C.S" << (sz_ == 4 ? 'W' : 'D') // 2 chars of mnemonic
		   << std::left << std::setw(MNE_WIDTH-2) << "SP";
		os << " [SP+" << imm_ << "] = r" << uint32_t(rs_);
		return os.str();
	}

private:
	uint32_t imm_;
	uint8_t rs_;
	uint8_t sz_;
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
		os << "C.SLLI     ";
		printReg(os, rd_) << " <<= " << uint32_t(sft_);
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
		os << "C.ADD      ";
		printReg(os, rd_) << " += r" << uint32_t(rs_);
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
		os << "C.ADDI     ";
		printReg(os, rd_) << " += " << imm_;
		return os.str();
	}

private:
	int64_t imm_;
	uint8_t rd_;
};

/// Compressed Add Immediate Word
class CompAddIw : public Inst
{
public:
	CompAddIw(int64_t imm, uint8_t rd)
	: imm_(imm)
	, rd_(rd)
	{
	}

	void execute(ArchState &state) const override
	{
		const uint32_t vrd = state.getReg(rd_) + imm_;
		const int64_t svrd = int32_t(vrd);

		state.setReg(rd_, svrd);

		state.incPc(2);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << "C.ADDIW    ";
		printReg(os, rd_) << " += " << imm_;
		return os.str();
	}

private:
	int64_t imm_;
	uint8_t rd_;
};

/// Compressed Branch if (Not) Equal to Zero
class CompBz : public Inst
{
public:
	CompBz(bool eq, int64_t imm, uint8_t rs)
	: eq_(eq)
	, imm_(imm)
	, rs_(rs)
	{
	}

	void execute(ArchState &state) const override
	{
		const uint64_t val = state.getReg(rs_);
		const bool taken = (eq_ && val == 0) || (!eq_ && val != 0);
		if (taken)
		{
			state.incPc(imm_);
		}
		else
			state.incPc(2);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << "C.B";
		if (eq_) os << 'E' << 'Q';
		else     os << 'N' << 'E';
		os << "Z     ";
		printReg(os, rs_) << ',' << ' ' << imm_;
		return os.str();
	}

private:
	bool eq_;
	int64_t imm_;
	uint8_t rs_;
};

/// Compressed Load DWord
class CompLd : public Inst
{
public:
	CompLd(uint64_t imm, uint8_t rs, uint8_t rd)
	: imm_(imm)
	, rs_(rs)
	, rd_(rd)
	{
	}

	void execute(ArchState &state) const override
	{
		const uint64_t ea = state.getReg(rs_) + imm_;
		state.setReg(rd_, state.readMem(ea, 8));
		state.incPc(2);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << "C.LD       ";
		printReg(os, rd_) << " = [r" << uint32_t(rs_) << '+' << imm_ << ']';
		return os.str();
	}

private:
	uint64_t imm_;
	uint8_t rs_;
	uint8_t rd_;
};

/// Compressed Jump
class CompJ : public Inst
{
public:
	explicit CompJ(int64_t imm)
	: imm_(imm)
	{
	}

	void execute(ArchState &state) const override
	{
		// unconditional
		state.setPc(state.getPc() + imm_);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << "C.J        " << imm_;
		return os.str();
	}

private:
	int64_t imm_;
};

/// Compressed Store (D)Word
class CompSdw : public Inst
{
public:
	CompSdw(uint8_t imm, uint8_t rbase, uint8_t rsrc, uint8_t sz)
	: imm_(imm)
	, rbase_(rbase)
	, rsrc_(rsrc)
	, sz_(sz)
	{
	}

	void execute(ArchState &state) const override
	{
		const uint64_t base = state.getReg(rbase_);
		const uint64_t val = state.getReg(rsrc_);
		state.writeMem(base + imm_, sz_, val);
		state.incPc(2);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << "C.S"; // one character of mnemonic
		os << std::left << std::setw(MNE_WIDTH-1) << (sz_ == 4 ? 'W' : 'D');

		os << " [r" << uint32_t(rbase_) << '+' << uint32_t(imm_) << "] = r" << uint32_t(rsrc_);
		return os.str();
	}

private:
	uint8_t imm_;
	uint8_t rbase_;
	uint8_t rsrc_;
	uint8_t sz_;
};

/// Compressed Load Upper Immediate
class CompLui : public Inst
{
public:
	CompLui(int32_t imm, uint8_t rd)
	: imm_(imm)
	, rd_(rd)
	{
	}

	void execute(ArchState &state) const override
	{
		state.setReg(rd_, int64_t(imm_));
		state.incPc(2);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << "C.LUI      ";
		printReg(os, rd_) << " = " << imm_;
		return os.str();
	}

private:
	int32_t imm_;
	uint8_t rd_;
};

/// Compressed Load Word
class CompLw : public Inst
{
public:
	CompLw(uint8_t imm, uint8_t rs, uint8_t rd)
	: imm_(imm)
	, rbase_(rs)
	, rd_(rd)
	{
	}

	void execute(ArchState &state) const override
	{
		const uint64_t base = state.getReg(rbase_) + imm_;
		state.setReg(rd_, state.readMem(base, 4));
		state.incPc(2);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << "C.LW       ";
		printReg(os, rd_) << " = [r" << uint32_t(rbase_) << '+' << uint32_t(imm_) << ']';
		return os.str();
	}

private:
	uint8_t imm_;
	uint8_t rbase_;
	uint8_t rd_;
};

class Candi : public Inst
{
public:
	Candi(int32_t imm, uint8_t rsd)
	: imm_(imm)
	, rsd_(rsd)
	{
	}

	void execute(ArchState &state) const override
	{
		const uint64_t val = state.getReg(rsd_) & int64_t(imm_);
		state.setReg(rsd_, val);
		state.incPc(2);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << 'C' << '.' << std::left << std::setw(MNE_WIDTH) << "ANDI" << ' ';
		printReg(os, rsd_) << " &= " << imm_;
		return os.str();
	}

private:
	int32_t imm_;
	uint8_t rsd_;
};

/// Compressed Jump and Link to Register target
class CompJalr : public Inst
{
public:
	CompJalr(uint8_t rs)
	: rs_(rs)
	{
	}

	void execute(ArchState &state) const override
	{
		const uint64_t pc = state.getPc();
		state.setReg(1, pc + 2); // Link Reg

		uint64_t new_pc = state.getReg(rs_);
		if (new_pc & 1)
			--new_pc; // clear bottom bit

		state.setPc(new_pc);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << 'C' << '.'
		   << std::left << std::setw(MNE_WIDTH) << "JALR"
		   << " r1, r" << uint32_t(rs_);
		return os.str();
	}

private:
	uint8_t rs_;
};

/// Compressed Shift Right (Logical and Arithmetic)
class CompShiftRight : public Inst
{
public:
	CompShiftRight(uint8_t imm, uint8_t rsd, bool arith)
	: imm_(imm)
	, rsd_(rsd)
	, arith_(arith)
	{
	}

	void execute(ArchState &state) const override
	{
		const uint64_t val = state.getReg(rsd_);
		if (arith_)
			state.setReg(rsd_, int64_t(val) >> imm_);
		else
			state.setReg(rsd_, val >> imm_);

		state.incPc(2);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		// SR. are 3 chars of mnemonic
		os << "C.SR"
		   << (arith_ ? 'A' : 'L')
		   << std::left << std::setw(MNE_WIDTH-3)
		   << 'I' << ' ';

		printReg(os, rsd_) << " >>= " << uint32_t(imm_);

		return os.str();
	}

private:
	uint8_t imm_;
	uint8_t rsd_;
	bool arith_;
};

Inst* decode16(uint32_t opc)
{
	const uint8_t o10 = opc & 3; // opc[1:0]
	const uint8_t rd = (opc >> 7) & 0x1f; // opc[11:7]
	const uint8_t rs = (opc >> 2) & 0x1f; // opc[6:2]
	const uint8_t rsd = ((opc >> 7) & 7) + 8; // opc[9:7]
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
		if (o15_13 == 0x4000) // C.LW
		{
			// zero extended imm
			uint8_t imm = (opc >> (10 - 3)) & (7 << 3); // opc[12:10] -> imm[5:3]
			imm |= (opc & 0x20) ? 0x40 : 0; // opc[5] -> imm[6]
			imm |= (opc & 0x40) ?    4 : 0; // opc[6] -> imm[2]

			const uint8_t rsp = ((opc >> 7) & 7) + 8; // opc[9:7]
			const uint8_t rdp = ((opc >> 2) & 7) + 8; // opc[4:2]
			return new CompLw(imm, rsp, rdp);
		}
		if (o15_13 == 0x6000) // C.LD
		{
			// zero extended imm
			uint8_t imm = (opc >> (10 - 3)) & (7 << 3); // opc[12:10] -> imm[5:3]
			imm |= (opc << (6 - 5)) & (3 << 6); // opc[6:5] -> imm[7:6]

			const uint8_t rsp = ((opc >> 7) & 7) + 8; // opc[9:7]
			const uint8_t rdp = ((opc >> 2) & 7) + 8; // opc[4:2]
			return new CompLd(imm, rsp, rdp);
		}
		if (o15_13 == 0xc000) // C.SW
		{
			// zero extended imm
			uint8_t imm = (opc >> (10 - 3)) & (7 << 3); // opc[12:10] -> imm[5:3]
			imm |= (opc & 0x20) ? 0x40 : 0; // opc[5] -> imm[6]
			imm |= (opc & 0x40) ?    4 : 0; // opc[6] -> imm[2]

			const uint8_t rbase = ((opc >> 7) & 7) + 8; // opc[9:7]
			const uint8_t rsrc  = ((opc >> 2) & 7) + 8; // opc[4:2]
			return new CompSdw(imm, rbase, rsrc, 4);
		}
		if (o15_13 == 0xe000) // C.SD
		{
			// zero extended imm
			uint8_t imm = (opc >> (10 - 3)) & (7 << 3); // opc[12:10] -> imm[5:3]
			imm |= (opc & 0x20) ? 0x40 : 0; // opc[5] -> imm[6]
			imm |= (opc & 0x40) ? 0x80 : 0; // opc[6] -> imm[7]

			const uint8_t rbase = ((opc >> 7) & 7) + 8; // opc[9:7]
			const uint8_t rsrc = ((opc >> 2) & 7) + 8; // opc[4:2]
			return new CompSdw(imm, rbase, rsrc, 8);
		}
		return nullptr;
	}

	if (o10 == 1) // common compressed ops
	{
		if (o15_13 == 0x0000) // C.ADDI (and C.NOP)
		{
			uint8_t raw_bits = (opc >> 2) & 0x1f; // opc[6:2] -> imm[4:0]
			if (opc & 0x1000) // opc[12] -> imm[31:5] (sign ex)
				raw_bits |= 0xe0;

			const int8_t imm = raw_bits;
			return new CompAddI(imm, rd);
		}
		if (o15_13 == 0x2000) // C.ADDIW
		{
			uint8_t raw_bits = (opc >> 2) & 0x1f; // opc[6:2] -> imm[4:0]
			if (opc & 0x1000) // opc[12] -> imm[31:5] (sign ex)
				raw_bits |= 0xe0;

			const int8_t imm = raw_bits;
			return new CompAddIw(imm, rd);
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
		else if (o15_13 == 0x6000) // C.LUI, C.ADDI16SP
		{
			if (rd == 2)
			{
				uint16_t imm = (opc & 0x18) << 4; // opc[4:3] -> imm[8:7]
				imm |= (opc & 0x40) ? 0x10 : 0; // opc[6] -> imm[4]
				imm |= (opc & 0x20) ? 0x40 : 0; // opc[5] -> imm[6]
				imm |= (opc &    4) ? 0x20 : 0; // opc[2] -> imm[5]
				if (opc & 0x1000)
					imm |= 0xfe00; // sign ex
				const int16_t s_imm = int16_t(imm);
				return new CompAddI16Sp(s_imm);
			}
			//else
			uint32_t imm = (opc & 0x7c) << 10; // opc[6:2] -> imm[16:12]
			if (opc & 0x1000) // opc[12] -> sign ex
				imm |= 0xfffe0000;

			return new CompLui(imm, rd);
		}
		else if (o15_13 == 0x8000)
		{
			const uint16_t op_11_10 = opc & 0x0c00; // opc[11:10]
			if (op_11_10 == 0 || op_11_10 == 0x0400) // 00 - C.SRLI, 01 - C.SRAI
			{
				uint8_t imm = (opc >> 2) & 0x1f; // opc[6:2] -> imm[4:0]
				if (opc & 0x1000) // opc[12] -> imm[5]
					imm |= 0x20;
				return new CompShiftRight(imm, rsd, op_11_10 == 0x0400);
			}
			if (op_11_10 == 0x0800) // 10 - C.ANDI
			{
				uint8_t raw_bits = (opc >> 2) & 0x1f; // opc[6:2] -> imm[4:0]
				if (opc & 0x1000) // opc[12]
					raw_bits |= 0xe0; // sign-ex imm[31:5]
				const int8_t imm = raw_bits;

				return new Candi(imm, rsd);

			}
			if (op_11_10 == 0x0c00) // 11
			{
				// rd op= r2
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
		else if (o15_13 == 0xa000) // C.J
		{
			uint16_t imm = (opc & 0x600) >> 1; // opc[10:9] -> imm[9:8]
			imm |= (opc &      4) ? 0x020 : 0; // opc[ 2] -> imm[5]
			imm |= (opc &      8) ? 0x002 : 0; // opc[ 3] -> imm[1]
			imm |= (opc & 0x0010) ? 0x004 : 0; // opc[ 4] -> imm[2]
			imm |= (opc & 0x0020) ? 0x008 : 0; // opc[ 5] -> imm[3]
			imm |= (opc & 0x0040) ? 0x080 : 0; // opc[ 6] -> imm[7]
			imm |= (opc & 0x0080) ? 0x040 : 0; // opc[ 7] -> imm[6]
			imm |= (opc & 0x0100) ? 0x400 : 0; // opc[ 8] -> imm[10]
			imm |= (opc & 0x0800) ? 0x010 : 0; // opc[11] -> imm[4]
			if (opc & 0x1000) // opc[12] -> imm[15:11]
				imm |= 0xf800;
			const int16_t s_imm = imm;
			return new CompJ(s_imm);
		}
		else if (o15_13 == 0xc000 || o15_13 == 0xe000) // C.BEQZ, C.BNEZ
		{
			uint16_t imm = (opc & 0x60) << 1; // opc[6:5] -> imm[7:6]
			imm |= (opc &      4) ? 0x20 : 0; // opc[ 2] -> imm[5]
			imm |= (opc &      8) ? 0x02 : 0; // opc[ 3] -> imm[1]
			imm |= (opc & 0x0010) ? 0x04 : 0; // opc[ 4] -> imm[2]
			imm |= (opc & 0x0400) ? 0x08 : 0; // opc[10] -> imm[3]
			imm |= (opc & 0x0800) ? 0x10 : 0; // opc[11] -> imm[4]
			if (opc & 0x1000) // opc[12] -> imm[15:8]
				imm |= 0xff00;
			const int16_t s_imm = imm;

			const uint8_t rs = ((opc >> 7) & 7) + 8; // opc[9:7]
			const bool eq = o15_13 == 0xc000; // else NE
			return new CompBz(eq, s_imm, rs);
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
		else if (o15_12 == 0x4000 || o15_12 == 0x5000) // C.LWSP
		{
			// zero extended
			uint64_t imm = (opc & 0xc) << 4; // opc[3:2] -> imm[7:6]
			if (opc & 0x1000) // opc[12] -> imm[5]
				imm |= 0x20;
			imm |= (opc & 0x60) >> 2; // opc[6:5] -> imm[4:3]

			return new CompLdwSp(imm, rd, 4);
		}
		else if (o15_12 == 0x6000 || o15_12 == 0x7000) // C.LDSP
		{
			uint64_t imm = (opc & 0x1c) << 4; // opc[4:2] -> imm[8:6]
			if (opc & 0x1000) // opc[12] -> imm[5]
				imm |= 0x20;
			imm |= (opc & 0x60) >> 2; // opc[6:5] -> imm[4:3]

			return new CompLdwSp(imm, rd, 8);
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
			if (rd == 0) // C.EBREAK and C.HINT
			{
				return nullptr; // TODO
			}

			if (rs == 0) // C.JALR
				return new CompJalr(rd);

			return new CompAdd(rs, rd);
		}
		else if (o15_12 == 0xc000 || o15_12 == 0xd000) // C.SWSP
		{
			uint16_t imm = (opc >> 1) & 0xc0; // opc[8:7] -> imm[7:6]

			const uint16_t low_imm = (opc >> 9) & 0xf; // opc[12:9]
			imm |= low_imm << 2; // imm[5:2]

			return new CompSdwSp(imm, rs, 4);
		}
		else if (o15_12 == 0xe000 || o15_12 == 0xf000) // C.SDSP
		{
			uint16_t imm = (opc >> 1) & 0x1c0; // opc[9:7] -> imm[8:6]
			const uint16_t low_imm = (opc >> 10) & 7; // opc[12:10]
			imm |= low_imm << 3; // imm[5:3]
			return new CompSdwSp(imm, rs, 8);
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
		os << "AUIPC    ";
		printReg(os, rd_) << " = PC " << std::hex;
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
		os << "JAL      r" << uint32_t(rd_) << ", " << imm_;
		return os.str();
	}

private:
	int64_t imm_;
	uint8_t rd_;
};

/// Jump and Link to register target
class Jalr : public Inst
{
public:
	Jalr(int32_t imm, uint8_t r1, uint8_t rd)
	: imm_(imm)
	, r1_(r1)
	, rd_(rd)
	{
	}

	void execute(ArchState &state) const override
	{
		const uint64_t pc = state.getPc();
		state.setReg(rd_, pc + 4);

		uint64_t new_pc = state.getReg(r1_) + imm_;
		if (new_pc & 1)
			--new_pc; // clear bottom bit

		state.setPc(new_pc);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << std::left << std::setw(MNE_WIDTH) << "JALR"
		   << ' ' << 'r' << uint32_t(rd_)
		   << ", r" << uint32_t(r1_) << " + " << imm_;
		return os.str();
	}

private:
	int32_t imm_;
	uint8_t r1_;
	uint8_t rd_;
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
		case 0: os << "ADDI    "; op = "+"; break;
		case 1: os << "SLLI    "; op = "<<"; break;
		case 4: os << "XORI    "; op = "^"; break;
		case 6: os << " ORI    "; op = "|"; break;
		case 7: os << "ANDI    "; op = "&"; break;

		case 2: os << "SLTI    "; op = "<i"; break;
		case 3: os << "SLTIU   "; op = "<u"; break;

		case 5: // SRLI, SRAI
		{
			imm &= 0x3f;
			const bool arith = (imm_ & 0x400) != 0;
			if (arith)
			{
				os << "SRAI    ";
				op = ">>i";
			}
			else
			{
				os << "SRLI    ";
				op = ">>u";
			}

			break;
		}
		}

		os << ' ';
		printReg(os, rd_) << " = r" << uint32_t(r1_) << ' ' << op << ' ' << imm;

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
		os << "LUI      ";
		printReg(os, rd_) << " = " << imm_;
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
		case 0: os << "EQ     "; break; // BEQ
		case 1: os << "NE     "; break; // BNE
		case 4: os << "LT     "; break; // BLT
		case 5: os << "GE     "; break; // BGE
		case 6: os << "LTU    "; break; // BLTU
		case 7: os << "GEU    "; break; // BGEU
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
		os << 'S' << sz_str[sz_] << "       [r" << uint32_t(r1_);

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

/// Load
class Load : public Inst
{
public:
	Load(uint8_t op, int64_t imm, uint8_t r1, uint8_t rd)
	: op_(op)
	, imm_(imm)
	, r1_(r1)
	, rd_(rd)
	{
	}

	void execute(ArchState &state) const override
	{
		const uint64_t ea = state.getReg(r1_) + imm_;
		const uint8_t sz = 1 << (op_ & 3);

		const uint64_t mval = state.readMem(ea, sz);

		if (op_ > 3) // unsigned
		{
			state.setReg(rd_, mval);
			state.incPc(4);
			return;
		}
		//else sign extend
		uint64_t val = 0;
		if (op_ == 0) // byte
		{
			const int64_t sv = int8_t(mval);
			val = sv;
		}
		else if (op_ == 1) // halfword
		{
			const int64_t sv = int16_t(mval);
			val = sv;
		}
		else if (op_ == 2) // word
		{
			const int64_t sv = int32_t(mval);
			val = sv;
		}
		else if (op_ == 3) // dword
		{
			val = mval;
		}

		state.setReg(rd_, val);
		state.incPc(4);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		const char sz_str[] = {'B', 'H', 'W', 'D'};
		const uint8_t sz = (op_ & 3);

		os << 'L' << sz_str[sz];
		if (op_ > 3)
			os << 'U';
		else
			os << ' ';

		os << "      ";
		printReg(os, rd_) << " = [r" << uint32_t(r1_);

		if (imm_ < 0)
			os << '-' << -imm_;
		else
			os << '+' << imm_;

		os << ']';

		return os.str();
	}

private:
	uint8_t op_;
	int64_t imm_;
	uint8_t r1_;
	uint8_t rd_;
};

/// Integer Multiply and Divide (and Remainder)
class ImulDiv : public Inst
{
public:
	ImulDiv(uint8_t op, uint8_t r2, uint8_t r1, uint8_t rd)
	: op_(op)
	, r2_(r2)
	, r1_(r1)
	, rd_(rd)
	{
	}

	void execute(ArchState &state) const override
	{
		const uint64_t vr1 = state.getReg(r1_);
		const uint64_t vr2 = state.getReg(r2_);
		uint64_t val = 0;

		switch (op_)
		{
		case 0: // MUL (lower)
			val = vr1 * vr2;
			break;

		case 1: // MULH (signed x signed)
		{
			const __int128 tmp = int64_t(vr1) * int64_t(vr2);
			val = tmp >> 64;
			break;
		}

		case 2: // MULHSU (unsigned x unsigned)
		{
			const unsigned __int128 tmp = vr1 * vr2;
			val = tmp >> 64;
			break;
		}

		case 3: // MULHU (signed x unsigned)
		{
			const __int128 tmp = int64_t(vr1) * vr2;
			val = tmp >> 64;
			break;
		}

		case 4: // DIV (signed)
			val = int64_t(vr1) / int64_t(vr2);
			break;

		case 5: // DIVU (unsigned)
			val = vr1 / vr2;
			break;

		case 6: // REM (signeD)
			val = int64_t(vr1) % int64_t(vr2);
			break;

		case 7: // REMU
			val = vr1 % vr2;
			break;
		}

		state.setReg(rd_, val);
		state.incPc(4);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		switch (op_)
		{
		case 0: os << "MUL     "; break;
		case 1: os << "MULH    "; break;
		case 2: os << "MULHSU  "; break;
		case 3: os << "MULHU   "; break;
		case 4: os << "DIV     "; break;
		case 5: os << "DIVU    "; break;
		case 6: os << "REM     "; break;
		case 7: os << "REMU    "; break;
		}
		os << ' ';
		printReg(os, rd_)
		   << " = r" << uint32_t(r1_) << ", r" << uint32_t(r2_);

		return os.str();
	}

private:
	uint8_t op_;
	uint8_t r2_;
	uint8_t r1_;
	uint8_t rd_;
};

/// Add Immediate Word
class AddIw : public Inst
{
public:
	AddIw(int16_t imm, uint8_t r1, uint8_t rd)
	: imm_(imm)
	, r1_(r1)
	, rd_(rd)
	{
	}

	void execute(ArchState &state) const override
	{
		const uint32_t vrd = state.getReg(r1_) + imm_;
		const int64_t svrd = int32_t(vrd);

		state.setReg(rd_, svrd);

		state.incPc(4);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << "ADDIW    ";
		printReg(os, rd_) << " = r" << uint32_t(r1_) << '+' << imm_;
		return os.str();
	}

private:
	int16_t imm_;
	uint8_t r1_;
	uint8_t rd_;
};

class Ecall : public Inst
{
public:
	Ecall()
	{
	}

	void execute(ArchState &state) const override
	{
		const uint64_t syscall = state.getReg(17); // r17 is syscall id
		switch (syscall)
		{
		case 57: // close
			state.setReg(10, 0); // TODO
			break;

		case 64: // write
			state.getSys()->write(state);
			break;

		case 80: // fstat
			state.getSys()->fstat(state);
			break;

		case 174: // getuid
		case 175: // geteuid
		case 176: // getgid
		case 177: // getegid
			state.setReg(10, 3); // return value
			break;

		case 214: // sbrk
			state.getSys()->sbrk(state);
			break;

		default:
			std::cerr << " Unimplemented system call " << syscall << std::endl;
		}

		state.incPc(4);
	}

	std::string disasm() const override
	{
		return "ECALL";
	}
};

class OpRegReg : public Inst
{
public:
	OpRegReg(uint8_t op, bool op30, uint8_t r2, uint8_t r1, uint8_t rd)
	: op_(op)
	, op30_(op30)
	, r2_(r2)
	, r1_(r1)
	, rd_(rd)
	{
	}

	void execute(ArchState &state) const override
	{
		const uint64_t vr1 = state.getReg(r1_);
		const uint64_t vr2 = state.getReg(r2_);

		uint64_t vd = 0;
		switch (op_)
		{
		case 0: // ADD/SUB
			if (op30_)
				vd = vr1 - vr2;
			else
				vd = vr1 + vr2;
			break;

		case 1: // SLL
			if (vr2 < 63)
				vd = vr1 << vr2;
			else
				vd = 0;
			break;

		case 2: // SLT
			vd = int64_t(vr1) < int64_t(vr2) ? 1 : 0;
			break;

		case 3: // SLTU
			vd = vr1 < vr2 ? 1 : 0;
			break;

		case 4: // XOR
			vd = vr1 ^ vr2;
			break;

		case 5:
			if (op30_) // SRA
			{
				if (vr2 < 63)
					vd = int64_t(vr1) >> vr2;
				else
					vd = int64_t(vr1) < 0 ? -1 : 0;
			}
			else // SRL
			{
				if (vr2 < 63)
					vd = vr1 >> vr2;
				else
					vd = 0;
			}
			break;

		case 6:
			vd = vr1 | vr2;
			break;

		case 7:
			vd = vr1 & vr2;
			break;
		}

		state.setReg(rd_, vd);
		state.incPc(4);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		const char *op = "";

		os << std::left;
		switch (op_)
		{
		case 0: // ADD/SUB
			if (op30_)
			{
				os << std::setw(MNE_WIDTH) << "SUB";
				op = "-";
			}
			else
			{
				os << std::setw(MNE_WIDTH) << "ADD";
				op = "+";
			}
			break;

		case 1: // SLL
			os << std::setw(MNE_WIDTH) << "SLL";
			op = "<<";
			break;

		case 2:
			os << std::setw(MNE_WIDTH) << "SLT";
			op = "<";
			break;

		case 3:
			os << std::setw(MNE_WIDTH) << "SLTU";
			op = "<u";
			break;

		case 4:
			os << std::setw(MNE_WIDTH) << "XOR";
			op = "^";
			break;

		case 5:
			if (op30_)
			{
				os << std::setw(MNE_WIDTH) << "SRA";
				op = ">>";
			}
			else
			{
				os << std::setw(MNE_WIDTH) << "SRL";
				op = ">>u";
			}
			break;

		case 6:
			os << std::setw(MNE_WIDTH) << "OR";
			op = "|";
			break;

		case 7:
			os << std::setw(MNE_WIDTH) << "AND";
			op = "&";
			break;
		}

		os << ' ';
		printReg(os, rd_);

		os << " = r"
		   << uint32_t(r1_)
			<< ' '
			<< op
			<< ' ' << 'r'
			<< uint32_t(r2_);
		return os.str();
	}

private:
	uint8_t op_;
	bool op30_;
	uint8_t r2_;
	uint8_t r1_;
	uint8_t rd_;
};

/// Shift Left Logical Immediate Word
class Slliw : public Inst
{
public:
	Slliw(uint8_t imm, uint8_t r1, uint8_t rd)
	: imm_(imm)
	, r1_(r1)
	, rd_(rd)
	{
	}

	void execute(ArchState &state) const override
	{
		const uint32_t vrd = state.getReg(r1_) << imm_;
		const int64_t svrd = int32_t(vrd); // signed extend

		state.setReg(rd_, svrd);

		state.incPc(4);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << std::left << std::setw(MNE_WIDTH) << "SLLIW" << ' ';
		printReg(os, rd_) << " = r" << uint32_t(r1_) << " << " << uint32_t(imm_);
		return os.str();
	}

private:
	uint8_t imm_;
	uint8_t r1_;
	uint8_t rd_;
};

/// Shift Right Arithmetic and Logical Immediate Word
class Sraliw : public Inst
{
public:
	Sraliw(uint8_t imm, uint8_t r1, uint8_t rd, bool arith)
	: imm_(imm)
	, r1_(r1)
	, rd_(rd)
	, arith_(arith)
	{
	}

	void execute(ArchState &state) const override
	{
		const uint64_t val = state.getReg(r1_);

		int32_t tmp;
		if (arith_)
		{
			// calculate signed 32 bit value
			tmp = int64_t(val) >> imm_;
		}
		else
		{
			tmp = val >> imm_;
		}
		// extend back to 64 bit
		state.setReg(rd_, int64_t(tmp));

		state.incPc(4);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		// SR. are 3 chars of mnemonic
		os << "SR"
		   << (arith_ ? 'A' : 'L')
		   << std::left << std::setw(MNE_WIDTH-3)
		   << "IW" << ' ';

		printReg(os, rd_) << " = r" << uint32_t(r1_) << " << " << uint32_t(imm_);

		return os.str();
	}

private:
	uint8_t imm_;
	uint8_t r1_;
	uint8_t rd_;
	bool arith_;
};

/// Shift Left Logical Word
class Sllw : public Inst
{
public:
	Sllw(uint8_t r2, uint8_t r1, uint8_t rd)
	: r2_(r2)
	, r1_(r1)
	, rd_(rd)
	{
	}

	void execute(ArchState &state) const override
	{
		const uint64_t amt = state.getReg(r2_) & 0x3f; // use low 6 bits
		const uint32_t vrd = amt < 31 ? (state.getReg(r1_) << amt) : 0;
		const int64_t svrd = int32_t(vrd); // signed extend

		state.setReg(rd_, svrd);

		state.incPc(4);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << std::left << std::setw(MNE_WIDTH) << "SLLW" << ' ';
		printReg(os, rd_) << " = r" << uint32_t(r1_) << " << r" << uint32_t(r2_);
		return os.str();
	}

private:
	uint8_t r2_;
	uint8_t r1_;
	uint8_t rd_;
};

/// Add and Subtract Word
class AddSubW : public Inst
{
public:
	AddSubW(uint8_t r2, uint8_t r1, uint8_t rd, bool sub)
	: r2_(r2)
	, r1_(r1)
	, rd_(rd)
	, sub_(sub)
	{
	}

	void execute(ArchState &state) const override
	{
		const uint64_t v2 = state.getReg(r2_);
		uint64_t tmp = state.getReg(r1_);
		if (sub_)
			tmp -= v2;
		else
			tmp += v2;

		const uint32_t vrd = tmp; // truncate
		const int64_t svrd = int32_t(vrd); // sign extend
		state.setReg(rd_, svrd);

		state.incPc(4);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		const char op = sub_ ? '-' : '+';

		os << (sub_ ? "SUB" : "ADD") // first 3 chars
		   << std::left << std::setw(MNE_WIDTH-3) << 'W' << ' ';

		printReg(os, rd_) << " = r" << uint32_t(r1_)
		   << ' ' << op
		   << ' ' <<  'r' << uint32_t(r2_);

		return os.str();
	}

private:
	uint8_t r2_;
	uint8_t r1_;
	uint8_t rd_;
	bool sub_;
};

Inst* decode32(uint32_t opc)
{
	// opc[1:0] == 2'b11
	const uint32_t group = opc & 0x7c; // opc[6:2]
	const uint8_t rd = (opc >> 7) & 0x1f; //opc[11:7]
	const uint8_t r1 = (opc >> 15) & 0x1f; // opc[19:15]
	const uint8_t r2 = (opc >> 20) & 0x1f; // opc[24:20]
	const uint8_t op = (opc >> 12) & 7; // opc[14:12]

	switch (group)
	{
	case   0: // load
	{

		uint16_t imm = (opc >> 20) & 0xfff; // opc[31:20]
		if (imm & 0x800)
			imm |= 0xf000; // sign ex
		int16_t s_imm = imm;

		return new Load(op, s_imm, r1, rd);
	}

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
		const uint8_t sz = op; // opc[14:12]
		return new Store(sz, s_imm, r1, r2);
	}

	case  20: // AUIPC
	{
		const uint32_t imm = opc & 0xfffff000; // opc[31:12]
		return new Auipc(imm, rd);
	}

	case  16: // op imm
	{
		uint16_t imm = (opc >> 20) & 0xfff; // opc[31:20]
		if (imm & 0x800)
			imm |= 0xf000; // sign ex
		return new OpImm(op, imm, r1, rd);
	}

	case  24: // op imm32
	{
		if (op == 0) // ADDIW
		{
			uint16_t imm = (opc >> 20) & 0xfff; // opc[31:20]
			if (imm & 0x800)
				imm |= 0xf000; // sign ex
			return new AddIw(imm, r1, rd);
		}
		if (op == 1) // SLLIW
		{
			const uint16_t imm = (opc >> 20) & 0xfff; // opc[31:20]
			return new Slliw(imm, r1, rd);
		}
		if (op == 5) // SR(AL)IW
		{
			const uint16_t imm = (opc >> 20) & 0x01f; // opc[24:20]
			const bool op30 = opc & 0x40000000; // opc[30]
			return new Sraliw(imm, r1, rd, op30);
		}

		return nullptr; // TODO
	}

	case  48: // op reg,reg
	{
		if (opc & 0x2000000) // opc[25]
		{
			return new ImulDiv(op, r2, r1, rd);
		}
		//else int reg,reg
		const bool op30 = opc & 0x40000000; // opc[30]

		return new OpRegReg(op, op30, r2, r1, rd);
	}

	case  56: // op32
	{
		if (opc & 0x2000000) // opc[25]
		{
			return nullptr; // TODO: DIV/MUL word
		}
		//else ADD/Shift word
		const bool op30 = opc & 0x40000000; // opc[30]
		if (op == 0) // ADD+SUB
		{
			return new AddSubW(r2, r1, rd, op30);
		}
		if (op == 1) // SLLW
		{
			return new Sllw(r2, r1, rd);
		}
		if (op == 5) // SR(AL)W
		{
			return nullptr; // TODO
		}

		return nullptr; // TODO
	}

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
		return new Branch(s_imm, op, r2, r1);
	}

	case 100: // JALR
	{
		uint16_t imm = (opc >> 20) & 0xfff; // opc[31:20] -> imm[11:0]
		if (imm & 0x800)
			imm |= 0xf000; // sign ex imm[11]
		const int16_t s_imm = imm;
		return new Jalr(s_imm, r1, rd);
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
		return nullptr; // TODO

	case 112: // system
	{
		if (op == 0)
		{
			// ECALL and EBREAK
			// TODO: capture opc[20]
			return new Ecall();
		}
		return nullptr; // TODO
	}

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

