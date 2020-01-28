#include "inst.hpp"
#include "sparse_mem.hpp"
#include "simple_arch_state.hpp"
#include "host_system.hpp"
#include <cstring>
#include <iostream>
#include <iomanip>
#include <getopt.h>

using namespace rvfun;

int main(int argc, char **argv)
{
	if (argc == 1)
	{
		std::cerr << "Usage: " << argv[0] << "[-d][-i instruction_count] <elf file>" << std::endl;
		return 1;
	}

	bool debug = false;
	uint64_t max_icount = 0;
	int optc = getopt_long(argc, argv, "di:", nullptr, nullptr);
	while (optc != -1)
	{
		if (optc == 'd')
		{
			debug = true;
		}
		else if (optc == 'i')
		{
			max_icount = strtoll(optarg, nullptr, 10);
		}

		optc = getopt_long(argc, argv, "i:", nullptr, nullptr);
	}

	// pull unused arg from getopt
	const char *prog_name = argv[optind];

	std::cout << "Run program " << prog_name;
	if (max_icount != 0)
	{
		std::cout << " for " << max_icount << " instructions";
	}
	std::cout << '.' << std::endl;

	HostSystem host;
	SimpleArchState state;

	if (host.loadElf(prog_name, state))
	{
		std::cerr << "Failure loading ELF." << std::endl;
		return 1;
	}

	state.setSys(&host);
	state.setMem(host.getMem());

	uint64_t icount = 0;
	while (1)
	{
		if (host.hadExit())
		{
			std::cout << "Program exited." << std::endl;
			break;
		}

		const uint64_t pc = state.getPc();
		const uint16_t opc = state.readMem(pc, 2);
		uint32_t opc_sz = 2;

		uint32_t full_inst = opc;
		Inst *inst = nullptr;
		if ((opc & 3) == 3)
		{
			// 32 bit instruction
			opc_sz = 4;
			full_inst |= state.readMem(pc + 2, 2) << 16;

			inst = decode32(full_inst);
		}
		else
			inst = decode16(full_inst);

		if (debug)
			std::cout
			          << std::setw(12) << icount << ' '
			          << std::hex
			          << std::setw(12) << pc << ' '
			          << std::setw(8) << full_inst << ' '
						 << std::dec;

		if (!inst)
		{
			std::cout << "(null inst)(" << std::hex << full_inst << std::dec << ')' << '\n';
			state.incPc(opc_sz);
		}
		else
		{
			if (debug)
			{
				if (opc_sz == 4)
					std::cout << ' ' << ' '; // shift for C.
				std::cout << inst->disasm();
			}

			inst->execute(state);
		}

		if (debug)
			std::cout << std::endl;

		++icount;
		if (max_icount != 0 && icount >= max_icount)
			break;
	}

	// dump architected state
	if (debug)
	{
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
	}
	std::cout << "Executed " << icount << " instructions." << std::endl;

	return 0;
}

