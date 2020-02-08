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

	std::basic_ostream<char>& printReg(std::ostream &os, uint8_t r, bool is_float = false)
	{
		if (is_float)
			os << 'f';
		else
			os << 'r';

		return os << std::left << std::setw(2) << uint32_t(r);
	}

union IntFloat
{
	uint64_t dw;
	int64_t sdw;
	int32_t sw;
	float    f;
	double   d;
};
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

	std::vector<RegDep> dsts() const override { return {RegNum(rd_)}; }
	// no sources
	std::vector<RegDep> srcs() const override { return {}; }

	uint32_t opSize() const override { return 1; } // one byte immediate

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

	OpType opType() const override { return OT_MOVI; }

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

	std::vector<RegDep> dsts() const override { return {RegNum(rsd_)}; }
	std::vector<RegDep> srcs() const override { return {RegNum(rsd_), RegNum(r2_)}; }

	uint32_t opSize() const override { return 8; }

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

	OpType opType() const override { return OT_ALU; }

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

	std::vector<RegDep> dsts() const override { return {RegNum(rsd_)}; }
	std::vector<RegDep> srcs() const override { return {RegNum(rsd_), RegNum(r2_)}; }

	uint32_t opSize() const override { return 4; }

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

	OpType opType() const override { return OT_ALU; }

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

	// no dests
	std::vector<RegDep> dsts() const override { return {}; }
	std::vector<RegDep> srcs() const override { return {RegNum(rd_)}; }

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

	OpType opType() const override { return OT_BRANCH; }

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

	std::vector<RegDep> dsts() const override { return {RegNum(rd_)}; }
	std::vector<RegDep> srcs() const override { return {RegNum(rs_)}; }

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

	OpType opType() const override { return OT_MOV; }

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

	std::vector<RegDep> dsts() const override { return {RegNum(rd_)}; }
	std::vector<RegDep> srcs() const override { return {RegNum(Reg::SP)}; }

	uint64_t calcEa(ArchState &state) const override { return state.getReg(Reg::SP) + imm_; }
	uint32_t opSize() const override { return sz_; }

	void execute(ArchState &state) const override
	{
		const uint64_t ea = calcEa(state);

		// sign-extend from word
		uint64_t val = 0;
		if (sz_ == 4)
		{
			const int32_t mval = state.readMem(ea, 4);
			val = int64_t(mval);
		}
		else
		{
			val = state.readMem(ea, sz_);
		}

		state.setReg(rd_, val);
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

	OpType opType() const override { return OT_LOAD; }

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

	std::vector<RegDep> dsts() const override { return {RegNum(rd_)}; }
	std::vector<RegDep> srcs() const override { return {RegNum(Reg::SP)}; }

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

	OpType opType() const override { return OT_ALU; }

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

	std::vector<RegDep> dsts() const override { return {RegNum(Reg::SP)}; }
	std::vector<RegDep> srcs() const override { return {RegNum(Reg::SP)}; }

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

	OpType opType() const override { return OT_ALU; }

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

	// no dests
	std::vector<RegDep> dsts() const override { return {}; }

	// sources are EA and store data
	std::vector<RegDep> srcs() const override { return {RegNum(Reg::SP), RegNum(rs_)}; }
	// help consumers figure out which is store data
	RegDep stdSrc() const override { return RegNum(rs_); }

	uint64_t calcEa(ArchState &state) const override { return state.getReg(Reg::SP) + imm_; }
	uint32_t opSize() const override { return sz_; }

	void execute(ArchState &state) const override
	{
		const uint64_t val = state.getReg(rs_);
		state.writeMem(calcEa(state), sz_, val);
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

	OpType opType() const override { return OT_STORE; }

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

	// read-modify-write
	std::vector<RegDep> dsts() const override { return {RegNum(rd_)}; }
	std::vector<RegDep> srcs() const override { return {RegNum(rd_)}; }

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

	OpType opType() const override { return OT_SHIFT; }

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

	// reg += reg
	std::vector<RegDep> dsts() const override { return {RegNum(rd_)}; }
	std::vector<RegDep> srcs() const override { return {RegNum(rd_), RegNum(rs_)}; }

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

	OpType opType() const override { return OT_ALU; }

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

	// reg += imm
	std::vector<RegDep> dsts() const override { return {RegNum(rd_)}; }
	std::vector<RegDep> srcs() const override { return {RegNum(rd_)}; }

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

	OpType opType() const override { return OT_ALU; }

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

	// reg += imm
	std::vector<RegDep> dsts() const override { return {RegNum(rd_)}; }
	std::vector<RegDep> srcs() const override { return {RegNum(rd_)}; }

	uint32_t opSize() const override { return 4; }

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

	OpType opType() const override { return OT_ALU; }

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

	// no dests
	std::vector<RegDep> dsts() const override { return {}; }

	std::vector<RegDep> srcs() const override { return {RegNum(rs_)}; }

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

	OpType opType() const override { return OT_BRANCH; }

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

	std::vector<RegDep> dsts() const override { return {RegNum(rd_)}; }
	std::vector<RegDep> srcs() const override { return {RegNum(rs_)}; }

	uint64_t calcEa(ArchState &state) const override { return state.getReg(rs_) + imm_; }
	uint32_t opSize() const override { return 8; }

	void execute(ArchState &state) const override
	{
		state.setReg(rd_, state.readMem(calcEa(state), 8));
		state.incPc(2);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << "C.LD       ";
		printReg(os, rd_) << " = [r" << uint32_t(rs_) << '+' << imm_ << ']';
		return os.str();
	}

	OpType opType() const override { return OT_LOAD; }

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

	// no regs
	std::vector<RegDep> dsts() const override { return {}; }
	std::vector<RegDep> srcs() const override { return {}; }

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

	OpType opType() const override { return OT_BRANCH; }

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

	// no dests
	std::vector<RegDep> dsts() const override { return {}; }

	std::vector<RegDep> srcs() const override { return {RegNum(rbase_), RegNum(rsrc_)}; }

	// help consumers figure out which is store data
	RegDep stdSrc() const override { return RegNum(rsrc_); }

	virtual uint64_t calcEa(ArchState &state) const { return state.getReg(rbase_) + imm_; }
	virtual uint32_t opSize() const { return sz_; }

	void execute(ArchState &state) const override
	{
		const uint64_t val = state.getReg(rsrc_);
		state.writeMem(calcEa(state), sz_, val);
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

	OpType opType() const override { return OT_STORE; }

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

	std::vector<RegDep> dsts() const override { return {RegNum(rd_)}; }
	// no src
	std::vector<RegDep> srcs() const override { return {}; }

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

	OpType opType() const override { return OT_MOVI; }

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

	std::vector<RegDep> dsts() const override { return {RegNum(rd_)}; }
	std::vector<RegDep> srcs() const override { return {RegNum(rbase_)}; }

	uint64_t calcEa(ArchState &state) const override { return state.getReg(rbase_) + imm_; }
	uint32_t opSize() const override { return 4; }

	void execute(ArchState &state) const override
	{
		const uint64_t ea = calcEa(state);
		const int32_t mval = state.readMem(ea, 4);
		state.setReg(rd_, int64_t(mval));
		state.incPc(2);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << "C.LW       ";
		printReg(os, rd_) << " = [r" << uint32_t(rbase_) << '+' << uint32_t(imm_) << ']';
		return os.str();
	}

	OpType opType() const override { return OT_LOAD; }

private:
	uint8_t imm_;
	uint8_t rbase_;
	uint8_t rd_;
};

/// Compressed And Immediate
class Candi : public Inst
{
public:
	Candi(int32_t imm, uint8_t rsd)
	: imm_(imm)
	, rsd_(rsd)
	{
	}

	/// reg &= imm
	std::vector<RegDep> dsts() const override { return {RegNum(rsd_)}; }
	std::vector<RegDep> srcs() const override { return {RegNum(rsd_)}; }

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

	OpType opType() const override { return OT_ALU; }

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

	std::vector<RegDep> dsts() const override { return {RegNum(Reg::RA)}; }
	std::vector<RegDep> srcs() const override { return {RegNum(rs_)}; }

	void execute(ArchState &state) const override
	{
		const uint64_t pc = state.getPc();
		state.setReg(Reg::RA, pc + 2); // Link Reg

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

	OpType opType() const override { return OT_BRANCH; }

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

	// reg >>= imm
	std::vector<RegDep> dsts() const override { return {RegNum(rsd_)}; }
	std::vector<RegDep> srcs() const override { return {RegNum(rsd_)}; }

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

	OpType opType() const override { return OT_SHIFT; }

private:
	uint8_t imm_;
	uint8_t rsd_;
	bool arith_;
};

/// Compressed Float Store Double
class CompFsd : public Inst
{
public:
	CompFsd(uint8_t imm, uint8_t rbase, uint8_t rsrc)
	: imm_(imm)
	, rbase_(rbase)
	, rsrc_(rsrc)
	{
	}

	// no dests
	std::vector<RegDep> dsts() const override { return {}; }

	std::vector<RegDep> srcs() const override { return {RegNum(rbase_), stdSrc()}; }

	// help consumers figure out which is store data
	RegDep stdSrc() const override { return RegDep(RegNum(rsrc_), RegFile::FLOAT); }

	void execute(ArchState &state) const override
	{
		const uint64_t base = state.getReg(rbase_);
		const double val = state.getFloat(rsrc_);
		state.writeMem(base + imm_, 8, val);
		state.incPc(2);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << 'C' << '.' << std::left << std::setw(MNE_WIDTH) << "FSD";
		os << " [r" << uint32_t(rbase_) << '+' << uint32_t(imm_) << "] = f" << uint32_t(rsrc_);

		return os.str();
	}

	OpType opType() const override { return OT_STORE_FP; }

private:
	uint8_t imm_;
	uint8_t rbase_;
	uint8_t rsrc_;
};

/// Compressed Float Load Double
class CompFpLd : public Inst
{
public:
	CompFpLd(uint32_t imm, uint8_t rs, uint8_t rd)
	: imm_(imm)
	, rs_(rs)
	, rd_(rd)
	{
	}

	std::vector<RegDep> dsts() const override { return {RegDep(RegNum(rd_), RegFile::FLOAT)}; }
	std::vector<RegDep> srcs() const override { return {RegNum(rs_)}; }
	uint64_t calcEa(ArchState &state) const override { return state.getReg(rs_) + imm_; }
	uint32_t opSize() const override { return 8; }

	void execute(ArchState &state) const override
	{
		// pull 8 bytes as-is
		IntFloat tmp;
		tmp.dw = state.readMem(calcEa(state), 8);

		state.setFloat(rd_, tmp.d);
		state.incPc(2);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << 'C' << '.';
		os << std::left << std::setw(MNE_WIDTH) << "FLD" << ' ';
		printReg(os, rd_, true) << " = [r" << uint32_t(rs_) << '+' << imm_ << ']';
		return os.str();
	}

	OpType opType() const override { return OT_LOAD_FP; }

private:
	uint32_t imm_;
	uint8_t rs_;
	uint8_t rd_;
};

Inst* decode16(uint32_t opc)
{
	const uint8_t o10 = opc & 3; // opc[1:0]
	const uint8_t rd = (opc >> 7) & 0x1f; // opc[11:7]
	const uint8_t rs = (opc >> 2) & 0x1f; // opc[6:2]
	const uint8_t rsd = ((opc >> 7) & 7) + 8; // opc[9:7]
	const uint16_t o15_13 = opc & 0xe000; // opc[15:13]
	const uint8_t r1p = ((opc >> 7) & 7) + 8; // opc[9:7]
	const uint8_t r2p = ((opc >> 2) & 7) + 8; // opc[4:2]

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
		if (o15_13 == 0x2000) // C.FLD
		{
			// zero extended imm
			uint8_t imm = (opc >> (10 - 3)) & (7 << 3); // opc[12:10] -> imm[5:3]
			imm |= (opc << (6 - 5)) & (3 << 6); // opc[6:5] -> imm[7:6]

			const uint8_t rsp = r1p; // opc[9:7]
			const uint8_t rdp = r2p; // opc[4:2]
			return new CompFpLd(imm, rsp, rdp);
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
		// 0x8000 is rsvd
		if (o15_13 == 0xa000) // C.FSD
		{
			// zero extended imm
			uint8_t imm = (opc >> (10 - 3)) & (7 << 3); // opc[12:10] -> imm[5:3]
			imm |= (opc & 0x20) ? 0x40 : 0; // opc[5] -> imm[6]
			imm |= (opc & 0x40) ? 0x80 : 0; // opc[6] -> imm[7]

			const uint8_t rbase = r1p;
			const uint8_t rsrc = r2p;
			return new CompFsd(imm, rbase, rsrc);
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

	std::vector<RegDep> dsts() const override { return {RegNum(rd_)}; }
	// no source
	std::vector<RegDep> srcs() const override { return {}; }

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

	OpType opType() const override { return OT_MOVI; }

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

	std::vector<RegDep> dsts() const override
	{
		std::vector<RegDep> ret;
		if (rd_ == 0)
			return ret; // no dst

		ret.emplace_back(RegNum(rd_));
		return ret;
	}

	// no sources
	std::vector<RegDep> srcs() const override { return {}; }

	void execute(ArchState &state) const override
	{
		const uint64_t pc = state.getPc();
		state.setReg(rd_, pc + 4);
		state.incPc(imm_);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << std::left << std::setw(MNE_WIDTH);

		if (rd_ == 0)
		{
			os << 'J'; // link destination is discarded
		}
		else
		{
			os << "JAL" << ' ' << 'r' << uint32_t(rd_) << ',';
		}
		os << ' ' << imm_;

		return os.str();
	}

	OpType opType() const override { return OT_BRANCH; }

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

	// rd_ = PC+4; PC = r1_ + imm
	std::vector<RegDep> dsts() const override { return {RegNum(rd_)}; }
	std::vector<RegDep> srcs() const override { return {RegNum(r1_)}; }

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

	OpType opType() const override { return OT_BRANCH; }

private:
	int32_t imm_;
	uint8_t r1_;
	uint8_t rd_;
};

/// Register Op Immediate
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

	// rd = r1 op imm
	std::vector<RegDep> dsts() const override { return {RegNum(rd_)}; }
	std::vector<RegDep> srcs() const override { return {RegNum(r1_)}; }

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

	OpType opType() const override
	{
		if (op_ == 1 || op_ == 5)
			return OT_SHIFT;
		return OT_ALU;
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

	std::vector<RegDep> dsts() const override { return {RegNum(rd_)}; }
	// no sources
	std::vector<RegDep> srcs() const override { return {}; }

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

	OpType opType() const override { return OT_MOVI; }

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

	// no dests
	std::vector<RegDep> dsts() const override { return {}; }
	std::vector<RegDep> srcs() const override { return {RegNum(r1_), RegNum(r2_)}; }

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

	OpType opType() const override { return OT_BRANCH; }

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

	// no dests
	std::vector<RegDep> dsts() const override { return {}; }
	std::vector<RegDep> srcs() const override { return {RegNum(r1_), RegNum(r2_)}; }
	// help consumers figure out which is store data
	RegDep stdSrc() const override { return RegNum(r2_); }

	uint64_t calcEa(ArchState &state) const override { return state.getReg(r1_) + imm_; }
	uint32_t opSize() const override { return sz_; }

	void execute(ArchState &state) const override
	{
		const uint64_t val = state.getReg(r2_);
		state.writeMem(calcEa(state), 1 << sz_, val);
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

	OpType opType() const override { return OT_STORE; }

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

	std::vector<RegDep> dsts() const override { return {RegNum(rd_)}; }
	std::vector<RegDep> srcs() const override { return {RegNum(r1_)}; }

	uint64_t calcEa(ArchState &state) const override { return state.getReg(r1_) + imm_; }
	uint32_t opSize() const override { return 1 << (op_ & 3); }

	void execute(ArchState &state) const override
	{
		const uint8_t sz = opSize();

		const uint64_t mval = state.readMem(calcEa(state), sz);

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

	OpType opType() const override { return OT_LOAD; }

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

	std::vector<RegDep> dsts() const override { return {RegNum(rd_)}; }
	std::vector<RegDep> srcs() const override { return {RegNum(r1_), RegNum(r2_)}; }

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

	OpType opType() const override
	{
		return (op_ < 4) ? OT_MUL : OT_DIV;
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

	std::vector<RegDep> dsts() const override { return {RegNum(rd_)}; }
	std::vector<RegDep> srcs() const override { return {RegNum(r1_)}; }

	uint32_t opSize() const override { return 4; }

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

	OpType opType() const override { return OT_ALU; }

private:
	int16_t imm_;
	uint8_t r1_;
	uint8_t rd_;
};

/// Environment (Operating system) Call
class Ecall : public Inst
{
public:
	Ecall()
	{
	}

	// no standard dependencies
	std::vector<RegDep> dsts() const override { return {}; }
	std::vector<RegDep> srcs() const override { return {}; }

	void execute(ArchState &state) const override
	{
		const uint64_t syscall = state.getReg(17); // r17 is syscall id
		switch (syscall)
		{
		case 56: // openat
			state.getSys()->open(state);
			break;

		case 57: // close
			state.setReg(10, 0); // TODO
			break;

		case 64: // write
			state.getSys()->write(state);
			break;

		case 66: // writev
			state.getSys()->writev(state);
			break;

		case 78: // readlinkat
			state.getSys()->readlinkat(state);
			break;

		case 80: // fstat
			state.getSys()->fstat(state);
			break;

		case 93: // exit
		case 94: // exit_group (TODO: exit all threads)
			state.getSys()->exit(state);
			break;

		case 160: // uname
			state.getSys()->uname(state);
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
			state.setReg(10, 0); // return value
		}

		state.incPc(4);
	}

	std::string disasm() const override
	{
		return "ECALL";
	}

	OpType opType() const override { return OT_SYSTEM; }
};

/// Alu op with two register sources
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

	std::vector<RegDep> dsts() const override { return {RegNum(rd_)}; }
	std::vector<RegDep> srcs() const override { return {RegNum(r1_), RegNum(r2_)}; }

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

	OpType opType() const override
	{
		if (op_ == 1 || op_ == 5)
			return OT_SHIFT;
		return OT_ALU;
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

	std::vector<RegDep> dsts() const override { return {RegNum(rd_)}; }
	std::vector<RegDep> srcs() const override { return {RegNum(r1_)}; }

	uint32_t opSize() const override { return 4; }

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

	OpType opType() const override { return OT_SHIFT; }

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

	std::vector<RegDep> dsts() const override { return {RegNum(rd_)}; }
	std::vector<RegDep> srcs() const override { return {RegNum(r1_)}; }

	uint32_t opSize() const override { return 4; }

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

	OpType opType() const override { return OT_SHIFT; }

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

	// rd = r1 << r2
	std::vector<RegDep> dsts() const override { return {RegNum(rd_)}; }
	std::vector<RegDep> srcs() const override { return {RegNum(r1_), RegNum(r2_)}; }

	uint32_t opSize() const override { return 4; }

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

	OpType opType() const override { return OT_SHIFT; }

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

	// rd = r1 +/- r2
	std::vector<RegDep> dsts() const override { return {RegNum(rd_)}; }
	std::vector<RegDep> srcs() const override { return {RegNum(r1_), RegNum(r2_)}; }

	uint32_t opSize() const override { return 4; }

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

	OpType opType() const override { return OT_ALU; }

private:
	uint8_t r2_;
	uint8_t r1_;
	uint8_t rd_;
	bool sub_;
};

/// Multiply and Divide Word
class MulDivW : public Inst
{
public:
	MulDivW(uint8_t op, uint8_t r2, uint8_t r1, uint8_t rd)
	: op_(op)
	, r2_(r2)
	, r1_(r1)
	, rd_(rd)
	{
	}

	std::vector<RegDep> dsts() const override { return {RegNum(rd_)}; }
	std::vector<RegDep> srcs() const override { return {RegNum(r1_), RegNum(r2_)}; }

	uint32_t opSize() const override { return 4; }

	void execute(ArchState &state) const override
	{
		const uint32_t v2 = state.getReg(r2_);
		const uint32_t v1 = state.getReg(r1_);
		int32_t val = 0;

		switch (op_)
		{
		case 0: // MULW
			val = int32_t(v1) * int32_t(v2);
			break;

		case 4: // DIVW
			if (v2 != 0)
				val = int32_t(v1) / int32_t(v2);
			break;

		case 5: // DIVUW
			if (v2 != 0)
				val = v1 / v2;
			break;

		case 6: // REMW
			if (v2 != 0)
				val = int32_t(v1) % int32_t(v2);
			break;

		case 7: // REMUW
			if (v2 != 0)
				val = v1 % v2;
			break;
		}

		state.setReg(rd_, int64_t(val)); // sign extend
		state.incPc(4);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << std::left << std::setw(MNE_WIDTH);
		char op = ' ';
		switch (op_)
		{
		case 0: os << "MULW"; op = '*'; break;
		case 4: os << "DIVW"; op = '/'; break;
		case 5: os << "DIVUW"; op = '/'; break;
		case 6: os << "REMW"; op = '%'; break;
		case 7: os << "REMUW"; op = '%'; break;
		}
		os << ' ';

		printReg(os, rd_) << " = r" << uint32_t(r1_) << ' ' << op
		   << ' ' << 'r' << uint32_t(r2_);

		return os.str();
	}

	OpType opType() const override
	{
		return op_ == 0 ? OT_MUL : OT_DIV;
	}

private:
	uint8_t op_;
	uint8_t r2_;
	uint8_t r1_;
	uint8_t rd_;
};

/// Load-reserve and Store-conditional
class LoadReserveStoreCond : public Inst
{
public:
	LoadReserveStoreCond(bool is_store, bool dword, bool aq, bool rl, uint8_t r2, uint8_t ar, uint8_t rd)
	: is_store_(is_store)
	, dword_(dword)
	, aq_(aq)
	, rl_(rl)
	, r2_(r2)
	, ar_(ar)
	, rd_(rd)
	{
	}

	// always write rd
	std::vector<RegDep> dsts() const override { return {RegNum(rd_)}; }

	std::vector<RegDep> srcs() const override
	{
		std::vector<RegDep> ret;
		ret.emplace_back(RegNum(ar_)); // always read address

		if (is_store_)
			ret.emplace_back(RegNum(r2_)); // store data

		return ret;
	}

	// help consumers figure out which is store data
	RegDep stdSrc() const override
	{
		if (is_store_)
			return RegNum(r2_);
		//else load

		return RegDep(RegNum(0), RegFile::NONE);
	}

	uint64_t calcEa(ArchState &state) const override { return state.getReg(ar_); }
	uint32_t opSize() const override { return dword_ ? 8 : 4; }

	void execute(ArchState &state) const override
	{
		// TODO monitor reservation
		const uint16_t sz = opSize();
		const uint64_t addr = calcEa(state);
		if (is_store_)
		{
			const uint64_t write_val = state.getReg(r2_);
			state.writeMem(addr, sz, write_val);
			state.setReg(rd_, 0); // success!
		}
		else
		{
			state.setReg(rd_, state.readMem(addr, sz));
		}

		state.incPc(4);
	}

	std::string disasm() const override
	{
		// hard to predict length of mnemonic
		std::ostringstream mne;
		mne << (is_store_ ? "SC" : "LR") << '.' << (dword_ ? 'D' : 'W');
		if (aq_)
			mne << ".aq";
		if (rl_)
			mne << ".rl";

		std::ostringstream os;
		os << std::left << std::setw(MNE_WIDTH) << mne.str() << ' ';

		printReg(os, rd_) << " = [r" << uint32_t(ar_) << ']';
		if (is_store_)
			os << "<- r" << uint32_t(r2_);

		return os.str();
	}

	OpType opType() const override
	{
		return is_store_ ? OT_STORE : OT_LOAD;
	}

private:
	bool is_store_; ///< else load
	bool dword_; ///< size of memory operation
	bool aq_; ///< acquire semantic
	bool rl_; ///< release semantic
	uint8_t r2_; ///< store data register
	uint8_t ar_; ///< address register
	uint8_t rd_; ///< destination (for load and store)
};

/// Atomic Operation
class AmoOp : public Inst
{
public:
	AmoOp(uint8_t o31_27, bool dword, bool aq, bool rel, uint8_t r2, uint8_t r1, uint8_t rd)
	: o31_27_(o31_27)
	, dword_(dword)
	, aq_(aq)
	, rel_(rel)
	, r2_(r2)
	, r1_(r1)
	, rd_(rd)
	{
	}

	std::vector<RegDep> dsts() const override { return {RegNum(rd_)}; }
	std::vector<RegDep> srcs() const override { return {RegNum(r1_), RegNum(r2_)}; }

	// actually r2_ op mem
	RegDep stdSrc() const override { return RegNum(r2_); }

	uint64_t calcEa(ArchState &state) const override { return state.getReg(r1_); }
	uint32_t opSize() const override { return dword_ ? 8 : 4; }

	void execute(ArchState &state) const override
	{
		const uint64_t ea = calcEa(state);
		const uint64_t vr2 = state.getReg(r2_);
		const uint16_t sz = opSize();
		// TODO: acquire and release

		// initial value in memory (not needed for SWAP into r0)
		const uint64_t init_val =
		   (o31_27_ == 1 && rd_ == 0) ? 0 : state.readMem(ea, sz);

		uint64_t val = 0; // value to store back

		// TODO: word versions need to sign extend to register
		switch (o31_27_)
		{
		case 0: val = init_val + vr2; break; // ADD
		case 1: val = vr2; break; // SWAP
		//case 2: // LR
		//case 3: // SC
		case 4: val = init_val ^ vr2; break; // XOR
		// 5..7 rsvd
		case 8: val = init_val | vr2; break; // OR
		// 9..1 rsvd
		case 12: val = init_val & vr2; break; // AND
		// 13..15 rsvd
		case 16: val = std::min(int64_t(init_val), int64_t(vr2)); break; // MIN
		// 17..19 rsvd
		case 20: val = std::max(int64_t(init_val), int64_t(vr2)); break; // MAX
		// 21..23 rsvd
		case 24: val = std::min(init_val, vr2); break; // MINU
		// 25..27 rsvd
		case 28: val = std::max(init_val, vr2); break; // MAXU
		// 29..31 rsvd
		}

		state.setReg(rd_, init_val);
		state.writeMem(ea, sz, val);

		state.incPc(4);
	}

	std::string disasm() const override
	{
		// hard to predict length of mnemonic
		std::ostringstream mne;
		mne << "AMO";
		switch (o31_27_)
		{
		case 0: mne << "ADD"; break;
		case 1: mne << "SWAP"; break;
		//case 2: // LR
		//case 3: // SC
		case 4: mne << "XOR"; break;
		// 5..7 rsvd
		case 8: mne << "OR"; break;
		// 9..1 rsvd
		case 12: mne << "AND"; break;
		// 13..15 rsvd
		case 16: mne << "MIN"; break;
		// 17..19 rsvd
		case 20: mne << "MAX"; break;
		// 21..23 rsvd
		case 24: mne << "MINU"; break;
		// 25..27 rsvd
		case 28: mne << "MAXU"; break;
		// 29..31 rsvd

		default: mne << "(ERR)";
		}
		mne << '.' << (dword_ ? 'D' : 'W');

		if (aq_)
			mne << ".aq";
		if (rel_)
			mne << ".rl";

		std::ostringstream os;
		os << std::left << std::setw(MNE_WIDTH) << mne.str() << ' ';

		printReg(os, rd_) << " = [r" << uint32_t(r1_) << "], r" << uint32_t(r2_);

		return os.str();
	}

	OpType opType() const override { return OT_ATOMIC; }

private:
	uint8_t o31_27_;
	bool dword_;
	bool aq_;
	bool rel_;
	uint8_t r2_;
	uint8_t r1_;
	uint8_t rd_;
};

/// Store Floating Point
class StoreFp : public Inst
{
public:
	StoreFp(int32_t imm, uint8_t rbase, uint8_t rsrc, uint16_t sz)
	: imm_(imm)
	, rbase_(rbase)
	, rsrc_(rsrc)
	, sz_(sz)
	{
	}

	// no dests
	std::vector<RegDep> dsts() const override { return {}; }
	std::vector<RegDep> srcs() const override { return {RegNum(rbase_), stdSrc()}; }
	// help consumers figure out which is store data
	RegDep stdSrc() const override { return RegDep(RegNum(rsrc_), RegFile::FLOAT); }

	void execute(ArchState &state) const override
	{
		const uint64_t base = state.getReg(rbase_);
		const double val = state.getFloat(rsrc_);
		state.writeMem(base + imm_, sz_, val);
		state.incPc(4);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		os << 'F' << 'S';
		os << std::left << std::setw(MNE_WIDTH-2) << (sz_ == 4 ? 'W' : 'D');
		os << " [r" << uint32_t(rbase_) << '+' << uint32_t(imm_) << "] = f" << uint32_t(rsrc_);

		return os.str();
	}

	OpType opType() const override { return OT_STORE_FP; }

private:
	int32_t imm_;
	uint8_t rbase_;
	uint8_t rsrc_;
	uint16_t sz_;
};

/// Move between int and float register files
class Fmove : public Inst
{
public:
	Fmove(bool dword, bool to_float, uint8_t r1, uint8_t rd)
	: dword_(dword)
	, to_float_(to_float)
	, r1_(r1)
	, rd_(rd)
	{
	}

	std::vector<RegDep> dsts() const override
	{
		if (to_float_)
			return {RegDep(RegNum(rd_), RegFile::FLOAT)};

		// int is default
		return {RegNum(rd_)};
	}

	std::vector<RegDep> srcs() const override
	{
		if (to_float_)
			return {RegNum(r1_)}; // int is default

		return {RegDep(RegNum(r1_), RegFile::FLOAT)};
	}

	uint32_t opSize() const override { return dword_ ? 8 : 4; }

	void execute(ArchState &state) const override
	{
		IntFloat tmp;

		if (to_float_)
		{
			// read int file
			tmp.dw = state.getReg(r1_);

			double val = 0;
			if (dword_)
				val = tmp.d; // double to double
			else
				val = tmp.f; // float to double

			state.setFloat(rd_, val);
		}
		else // float to int
		{
			// read float file
			if (dword_)
			{
				// double to dword
				tmp.d = state.getFloat(r1_);
				state.setReg(rd_, tmp.dw);
			}
			else // word
			{
				// read single precision
				tmp.f = state.getFloat(r1_);

				// sign extend (how is this meaningful?)
				state.setReg(rd_, int64_t(tmp.sw));
			}
		}

		state.incPc(4);
	}

	std::string disasm() const override
	{
		std::ostringstream os;

		// assemble mnemonic
		std::ostringstream mne;
		mne << "FMV.";

		if (to_float_)
		{
			mne << (dword_ ? 'D' : 'W') << '.' << 'X';
		}
		else // float to int
		{
			mne << 'X' << '.' << (dword_ ? 'D' : 'W');
		}

		os << std::left << std::setw(MNE_WIDTH) << mne.str() << ' ';

		const char src_rf = to_float_ ? 'r' : 'f';
		printReg(os, rd_, to_float_) << " = " << src_rf << uint32_t(r1_);

		return os.str();
	}

	OpType opType() const override { return OT_MOV; }

private:
	bool dword_;
	bool to_float_;
	uint8_t r1_;
	uint8_t rd_;
};

/// Load floating point
class LoadFp : public Inst
{
public:
	LoadFp(uint8_t op, int32_t s_imm, uint8_t r1, uint8_t rd)
	: op_(op)
	, imm_(s_imm)
	, r1_(r1)
	, rd_(rd)
	{
	}

	std::vector<RegDep> dsts() const override { return {RegDep(RegNum(rd_), RegFile::FLOAT)}; }
	std::vector<RegDep> srcs() const override { return {RegNum(r1_)}; }

	uint64_t calcEa(ArchState &state) const override { return state.getReg(r1_) + imm_; }
	uint32_t opSize() const override { return 1 << (op_ & 3); }

	void execute(ArchState &state) const override
	{
		const uint8_t sz = opSize();
		const uint64_t ea = calcEa(state);

		double val = 0;
		IntFloat tmp;
		if (sz == 8)
		{
			tmp.sdw = state.readMem(ea, 8);
			val = tmp.d;
		}
		else
		{
			tmp.sw = state.readMem(ea, 4);
			val = tmp.f;
		}

		state.setFloat(rd_, val);
		state.incPc(4);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		std::ostringstream mne;
		mne << 'F' << 'L' << (opSize() == 8 ? 'D' : 'W');
		os << std::left << std::setw(MNE_WIDTH) << mne.str() << ' ';
		printReg(os, rd_, true) << " = [r" << uint32_t(r1_) << '+' << imm_ << ']';

		return os.str();
	}

	OpType opType() const override { return OT_LOAD_FP; }

private:
	uint8_t op_;
	int64_t imm_;
	uint8_t r1_;
	uint8_t rd_;
};

/// Convert between float and int
class FcvtInt : public Inst
{
public:
	FcvtInt(bool dbl, bool to_float, uint8_t int_sz, uint8_t round, uint8_t r1, uint8_t rd)
	: dbl_(dbl)
	, to_float_(to_float)
	, int_sz_(int_sz)
	, round_(round)
	, r1_(r1)
	, rd_(rd)
	{
	}

	std::vector<RegDep> dsts() const override
	{
		if (to_float_)
			return {RegDep(RegNum(rd_), RegFile::FLOAT)};

		// int is default
		return {RegNum(rd_)};
	}

	std::vector<RegDep> srcs() const override
	{
		if (to_float_)
			return {RegNum(r1_)}; // int is default

		return {RegDep(RegNum(r1_), RegFile::FLOAT)};
	}

	uint32_t opSize() const override
	{
		// we can have different sizes for source and dest
		// TODO: decide how to convey that
		// For now, we'll return 8 if either is 8, and 4 only if both are 4
		return (dbl_ || int_sz_ > 1) ? 8 : 4;
	}

	void execute(ArchState &state) const override
	{
		// TODO: rounding modes
		if (to_float_)
		{
			double val = 0;
			const uint64_t rval = state.getReg(r1_);
			switch (int_sz_)
			{
			case 0: // signed word
			{
				const int32_t ival = rval;
				if (dbl_)
					val = ival;
				else
					val = float(ival);
				break;
			}

			case 1: // unsigned word
			{
				const uint32_t ival = rval;
				if (dbl_)
					val = ival;
				else
					val = float(ival);
				break;
			}

			case 2: // signed dword
			{
				const int64_t ival = rval;
				if (dbl_)
					val = ival;
				else
					val = float(ival);
				break;
			}

			case 3: // unsigned dword
			{
				const uint64_t ival = rval;
				if (dbl_)
					val = ival;
				else
					val = float(ival);
				break;
			}
			}

			state.setFloat(rd_, val);
		}
		else // to int
		{
			const double dval = state.getFloat(r1_);
			const float fval = dval;

			uint64_t val = 0;
			switch (int_sz_)
			{
			case 0: // signed word
			{
				int32_t tmp = 0;
				if (dbl_)
					tmp = dval;
				else
					tmp = fval;

				// sign-extend
				val = int64_t(tmp);
				break;
			}

			case 1: // unsigned word
			{
				uint32_t tmp = 0;
				if (dbl_)
					tmp = dval;
				else
					tmp = fval;

				// still need to sign-extend
				val = int64_t(int32_t(tmp));
				break;
			}

			case 2: // signed dword
			{
				int64_t tmp = 0;
				if (dbl_)
					tmp = dval;
				else
					tmp = fval;
				val = tmp;
				break;
			}

			case 3: // unsigned dword
			{
				if (dbl_)
					val = dval;
				else
					val = fval;
				break;
			}
			}

			state.setReg(rd_, val);
		}

		state.incPc(4);
	}

	std::string disasm() const override
	{
		std::ostringstream os;

		// assemble mnemonic
		std::ostringstream mne;
		mne << "FCVT.";

		if (to_float_)
		{
			mne << (dbl_ ? 'D' : 'S') << '.';
			mneInt(mne);
		}
		else
		{
			mneInt(mne) << '.' << (dbl_ ? 'D' : 'S');
		}

		os << std::left << std::setw(MNE_WIDTH) << mne.str() << ' ';

		const char src_rf = to_float_ ? 'r' : 'f';
		printReg(os, rd_, to_float_) << " = " << src_rf << uint32_t(r1_);

		return os.str();
	}

	OpType opType() const override { return OT_FP; }

private: // methods
	std::ostream& mneInt(std::ostream &os) const
	{
		os << ((int_sz_ & 2) ? 'L' : 'W');
		if (int_sz_ & 1)
			os << 'U';

		return os;
	}

private: // data
	bool dbl_;
	bool to_float_;
	uint8_t int_sz_;
	uint8_t round_;
	uint8_t r1_;
	uint8_t rd_;
};

/// Float sign manipulate
class Fsign : public Inst
{
public:
	Fsign(bool dbl, uint8_t op, uint8_t r2, uint8_t r1, uint8_t rd)
	: dbl_(dbl)
	, op_(op)
	, r2_(r2)
	, r1_(r1)
	, rd_(rd)
	{
	}

	std::vector<RegDep> dsts() const override { return {RegDep(RegNum(rd_), RegFile::FLOAT)}; }
	std::vector<RegDep> srcs() const override
	{
		return {
			RegDep(RegNum(r1_), RegFile::FLOAT),
			RegDep(RegNum(r2_), RegFile::FLOAT)
		};
	}

	uint32_t opSize() const override { return dbl_ ? 8 : 4; }

	void execute(ArchState &state) const override
	{
		double val = 0;

		if (dbl_)
		{
			const double v1 = state.getFloat(r1_);
			const bool v1_neg = v1 < 0;
			const bool v2_neg = state.getFloat(r2_) < 0;

			val = v1;
			bool invert = false;
			if (op_ == 0) // use r2 sign
			{
				invert = v1_neg != v2_neg; // invert to get them to match
			}
			else if (op_ == 1) // use opposite of r2
			{
				invert = v1_neg == v2_neg; // invert to get them to not match
			}
			else if (op_ == 2) // xor r1 and r2
			{
				// v1 v2 = val (currently v1)
				//  0  0   0 (keep v1)
				//  0  1   1 (invert)
				//  1  0   1 (keep v1)
				//  1  1   0 (invert)
				invert = v2_neg;
			}

			if (invert)
				val = -val;
		}
		else
		{
			const float v1 = state.getFloat(r1_);
			const bool v1_neg = v1 < 0;
			const bool v2_neg = state.getFloat(r2_) < 0;

			bool invert = false;
			if (op_ == 0) // use r2 sign
			{
				invert = v1_neg != v2_neg; // invert to get them to match
			}
			else if (op_ == 1) // use opposite of r2
			{
				invert = v1_neg == v2_neg; // invert to get them to not match
			}
			else if (op_ == 2) // xor r1 and r2
			{
				// v1 v2 = val (currently v1)
				//  0  0   0 (keep v1)
				//  0  1   1 (invert)
				//  1  0   1 (keep v1)
				//  1  1   0 (invert)
				invert = v2_neg;
			}

			if (invert)
				val = -v1;
			else
				val = v1;
		}

		state.setFloat(rd_, val);

		state.incPc(4);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		// assemble mnemonic
		std::ostringstream mne;

		if (r1_ == r2_)
		{
			// pseudo op
			if (op_ == 0)
				mne << "FMV";
			else if (op_ == 1)
				mne << "FNEG";
			else if (op_ == 2)
				mne << "FABS";
			//else op_ == 3 rsvd
		}
		else
		{
			mne << "FSGNJ";
			if (op_ == 1)
				mne << 'N';
			else if (op_ == 2)
				mne << 'X';
		}
		mne << '.' << (dbl_ ? 'D' : 'S');

		os << std::left << std::setw(MNE_WIDTH) << mne.str() << ' ';

		printReg(os, rd_, true) << " = f" << uint32_t(r1_);
		if (r1_ != r2_)
			os << ", f" << uint32_t(r2_);

		return os.str();
	}

	OpType opType() const override { return OT_FP; }

private:
	bool dbl_;
	uint8_t op_;
	uint8_t r2_;
	uint8_t r1_;
	uint8_t rd_;
};

/// Float Multiply and Add (and variants)
class Fmadd : public Inst
{
public:
	Fmadd(bool dbl, uint8_t rm, uint8_t op, uint8_t r3, uint8_t r2, uint8_t r1, uint8_t rd)
	: dbl_(dbl)
	, rm_(rm)
	, op_(op)
	, r3_(r3)
	, r2_(r2)
	, r1_(r1)
	, rd_(rd)
	{
	}

	std::vector<RegDep> dsts() const override { return {RegDep(RegNum(rd_), RegFile::FLOAT)}; }
	std::vector<RegDep> srcs() const override
	{
		return {
			RegDep(RegNum(r1_), RegFile::FLOAT),
			RegDep(RegNum(r2_), RegFile::FLOAT),
			RegDep(RegNum(r3_), RegFile::FLOAT)
		};
	}

	uint32_t opSize() const override { return dbl_ ? 8 : 4; }

	void execute(ArchState &state) const override
	{
		// TODO: rounding modes
		double val = 0;
		if (dbl_)
		{
			const double v1 = state.getFloat(r1_);
			const double v2 = state.getFloat(r2_);
			const double v3 = state.getFloat(r3_);
			switch (op_)
			{
			case 0: val = v1 * v2 + v3; break; // FMADD
			case 1: val = v1 * v2 - v3; break; // FMSUB
			case 2: val = -(v1 * v2) + v3; break; // FNMSUB
			case 3: val = -(v1 * v2) - v3; break; // FNMADD
			}
		}
		else
		{
			const float v1 = state.getFloat(r1_);
			const float v2 = state.getFloat(r2_);
			const float v3 = state.getFloat(r3_);
			switch (op_)
			{
			case 0: val = v1 * v2 + v3; break; // FMADD
			case 1: val = v1 * v2 - v3; break; // FMSUB
			case 2: val = -(v1 * v2) + v3; break; // FNMSUB
			case 3: val = -(v1 * v2) - v3; break; // FNMADD
			}
		}

		state.setFloat(rd_, val);
		state.incPc(4);
	}

	std::string disasm() const override
	{
		std::ostringstream os;
		// assemble mnemonic
		std::ostringstream mne;
		mne << 'F';
		bool is_add = (op_ & 1) != 0;
		if (op_ & 2)
		{
			mne << 'N';
		}
		else
		{
			is_add = !is_add;
		}
		mne << 'M' << (is_add ? "ADD" : "SUB");

		mne << '.' << (dbl_ ? 'D' : 'S');
		os << std::left << std::setw(MNE_WIDTH) << mne.str() << ' ';

		printReg(os, rd_, true) << " = f" << uint32_t(r1_)
		   << ", f" << uint32_t(r2_)
		   << ", f" << uint32_t(r3_);

		return os.str();
	}

	OpType opType() const override { return OT_FP; }

private:
	bool dbl_;
	uint8_t rm_;
	uint8_t op_;
	uint8_t r3_;
	uint8_t r2_;
	uint8_t r1_;
	uint8_t rd_;
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
	{
		uint16_t imm = (opc >> 20) & 0xfff; // opc[31:20]
		if (imm & 0x800)
			imm |= 0xf000; // sign ex
		int16_t s_imm = imm;

		return new LoadFp(op, s_imm, r1, rd);
	}

	case  12: // misc MEM
		return nullptr; // TODO Mem

	case  16: // op imm
	{
		uint16_t imm = (opc >> 20) & 0xfff; // opc[31:20]
		if (imm & 0x800)
			imm |= 0xf000; // sign ex
		return new OpImm(op, imm, r1, rd);
	}

	case  20: // AUIPC
	{
		const uint32_t imm = opc & 0xfffff000; // opc[31:12]
		return new Auipc(imm, rd);
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

	case  36: // store FP
	{
		uint16_t imm = rd; // opc[11:7] -> imm[4:0]
		imm |= (opc >> 20) & 0xfe0; // opc[31:25] -> imm[11:5]
		if (imm & 0x800)
			imm |= 0xf000; // sign extend from bit 11
		const int16_t s_imm = int16_t(imm);

		const uint8_t sz = op == 2 ? 4 : 8;
		const uint8_t rbase = r1;
		const uint8_t rsrc = r2;
		return new StoreFp(s_imm, rbase, rsrc, sz);
	}

	case  44: // AMO (atomics)
	{
		const bool dword = op == 3; // else word
		const bool o27 = (opc & 0x08000000) != 0; // opc[27]
		const bool aq  = (opc & 0x04000000) != 0; // opc[26]
		const bool rel = (opc & 0x02000000) != 0; // opc[25]

		if (opc & 0x10000000) // opc[28]
		{
			// Load-reserve (LR) + Store-conditional (SC)
			return new LoadReserveStoreCond(o27, dword, aq, rel, r2, r1, rd);
		}
		// atomic op
		const uint8_t o31_27 = (opc >> 27) & 0x1f; // opc[31:27]
		return new AmoOp(o31_27, dword, aq, rel, r2, r1, rd);
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

	case  52: // LUI
	{
		const int32_t imm = opc & 0xfffff000; // opc[31:12]
		return new Lui(imm, rd);
	}

	case  56: // op32
	{
		if (opc & 0x2000000) // opc[25]
		{
			return new MulDivW(op, r2, r1, rd); // DIV/MUL word
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

	case  64: // FMADD
	case  68: // FMSUB
	case  72: // FNMSUB
	case  76: // FNMADD
	{
		const bool dbl = (opc & 0x02000000) != 0; // opc[25]
		const uint8_t r3 = (opc >> 27) & 0x1f; // opc[31:27]
		const uint8_t rm = op;
		const uint8_t op2 = (opc >> 2) & 3; // opc[3:2]
		return new Fmadd(dbl, rm, op2, r3, r2, r1, rd);
	}

	case  80: // fp op
	{
		const uint8_t op2 = (opc >> 25) & 0xff; // opc[31:25]
		const uint8_t mask1 = op2 & 0x7e; // upper op bits[6:1]
		const uint8_t mask2 = op2 & 0x76; // ignore bit 3
		if (mask2 == 0x70) // FMV
		{
			const bool dword = (op2 & 1) != 0;
			const bool to_float = (op2 & 8) != 0;
			return new Fmove(dword, to_float, r1, rd);
		}
		if (mask2 == 0x60) // FCVT
		{
			const bool dbl = (op2 & 1) != 0; // float bar
			const bool to_float = (op2 & 8) != 0; // to_int bar
			const uint8_t int_sz = r2; // sw, w, sx, x
			const uint8_t round = op;
			return new FcvtInt(dbl, to_float, int_sz, round, r1, rd);
		}
		if (mask1 == 0x10) // FSGN
		{
			const bool dbl = (op2 & 1) != 0; // float bar
			return new Fsign(dbl, op, r2, r1, rd);
		}
		return nullptr; // TODO FP
	}

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

