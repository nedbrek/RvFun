#ifndef RVFUN_INST_HPP
#define RVFUN_INST_HPP

#include <string>

namespace rvfun
{
class ArchState;

/// Interface to one architected instruction
class Inst
{
public:
	/// update 'state' for execution of this
	virtual void execute(ArchState &state) const = 0;

	///@return assembly string of this
	virtual std::string disasm() const = 0;
};

Inst* decode16(uint32_t opc);
Inst* decode32(uint32_t opc);

} // namespace

#endif

