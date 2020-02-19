#ifndef RVFUN_INST_HPP
#define RVFUN_INST_HPP

#include <string>
#include <vector>

namespace rvfun
{
class ArchState;

/// Interface to one architected instruction
class Inst
{
public:
	/// Register file for dependency info
	enum class RegFile
	{
		NONE,  ///!< no dependency
		INT,   ///!< integer file
		FLOAT, ///!< floating point file
		MAX_RF
	};

	/// Prevent explicit int to RegNum conversion
	enum class RegNum : uint8_t {};

	/// Register dependency
	struct RegDep
	{
		RegFile  rf;  ///!< register file
		RegNum  reg; ///!< register number

		RegDep(RegNum n, RegFile f = RegFile::INT)
		: rf(f)
		, reg(n)
		{
		}
	};

	///@return all the registers written by this
	virtual std::vector<RegDep> dsts() const = 0;

	///@return all the registers read by this
	virtual std::vector<RegDep> srcs() const = 0;

	///@return register dependency for store data
	virtual RegDep stdSrc() const { return RegDep(RegNum(0), RegFile::NONE); }

	virtual uint64_t calcEa(ArchState &) const { return 0; }
	virtual uint32_t opSize() const { return 8; }

	/// update 'state' for execution of this
	virtual void execute(ArchState &state) const = 0;

	///@return assembly string of this
	virtual std::string disasm() const = 0;

	enum OpType
	{
		OT_MOV, ///< register to register (execute at rename)
		OT_MOVI, ///< register gets immediate
		OT_ALU,
		OT_SHIFT,
		OT_MUL,
		OT_DIV,
		OT_FP, // TODO: more FP types
		OT_LOAD,
		OT_STORE,
		OT_LOAD_FP,
		OT_STORE_FP,
		OT_ATOMIC,
		OT_BRANCH,
		OT_SYSTEM
	};
	virtual OpType opType() const = 0;
};

Inst* decode16(uint32_t opc);
Inst* decode32(uint32_t opc);
Inst* decode(ArchState &state, uint32_t &opc_sz, uint32_t &full_inst, bool debug);

} // namespace

#endif

