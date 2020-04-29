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
	bool verbose = false;
	uint64_t max_icount = 0;
	const char *optstring = "+di:v";
	int optc = getopt_long(argc, argv, optstring, nullptr, nullptr);
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
		else if (optc == 'v')
		{
			verbose = true;
		}

		optc = getopt_long(argc, argv, optstring, nullptr, nullptr);
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

	state.setSys(&host);
	state.setMem(host.getMem());
	state.setDebug(verbose);

	if (host.loadElf(prog_name, state))
	{
		std::cerr << "Failure loading ELF." << std::endl;
		return 1;
	}

	++optind;
	while (optind < argc)
	{
		const char *arg = argv[optind];
		std::cout << "Add argument: " << arg << std::endl;
		host.addArg(arg);
		++optind;
	}

	host.setStdin(std::string(prog_name) + ".stdin"); // TODO: make an option
	host.completeEnv(state);

	uint64_t icount = 0;
	while (1)
	{
		if (host.hadExit())
		{
			std::cout << "Program exited after " << icount << " instructions." << std::endl;
			break;
		}
		const uint64_t pc = state.getPc();
		if ((pc & -63ll) == 0)
		{
			std::cout << "Program returned to shell after " << icount << " instructions." << std::endl;
			break;
		}

		uint32_t opc_sz = 2;
		uint32_t full_inst = 0;

		if (debug)
			std::cout << std::setw(12) << icount << ' ';

		std::unique_ptr<Inst> inst(decode(state, opc_sz, full_inst, debug));
		if (!inst)
		{
			state.incPc(opc_sz);
		}
		else
		{
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

