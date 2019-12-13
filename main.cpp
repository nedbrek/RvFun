#include "inst.hpp"
#include "sparse_mem.hpp"
#include "simple_arch_state.hpp"
#include <iostream>
#include <iomanip>
using namespace rvfun;

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

