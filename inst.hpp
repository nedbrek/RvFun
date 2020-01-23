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

	/// update 'state' for execution of this
	virtual void execute(ArchState &state) const = 0;

	///@return assembly string of this
	virtual std::string disasm() const = 0;
};

Inst* decode16(uint32_t opc);
Inst* decode32(uint32_t opc);

} // namespace

#endif

