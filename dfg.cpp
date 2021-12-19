#include "inst.hpp"
#include <getopt.h>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <sstream>

int main(int argc, char **argv)
{
	if (argc == 1)
	{
		std::cerr << "Usage: " << argv[0] << " [-f opcode_file] [elf_file]" << std::endl;
		return 1;
	}

	const char *op_file = nullptr;
	const char *optstring = "+f:";
	int optc = getopt_long(argc, argv, optstring, nullptr, nullptr);
	while (optc != -1)
	{
		if (optc == 'f')
		{
			op_file = optarg;
		}
		optc = getopt_long(argc, argv, optstring, nullptr, nullptr);
	}

	if (!op_file)
	{
		// TODO support ELF
		std::cout << "Only support file right now" << std::endl;
		return 1;
	}

	// open the file
	std::fstream ifile(op_file);

	std::map<uint32_t, uint32_t> prod_int;
	std::map<uint32_t, uint32_t> prod_fp;

	//foreach line in the file
	uint64_t icount = 0;
	std::string line;
	while (std::getline(ifile, line))
	{
		++icount;

		// pull opcode from hex string
		std::istringstream is(line);
		uint32_t opc = 0;
		is >> std::hex >> opc;

		// decode
		bool is_compressed = false;
		rvfun::Inst *inst = nullptr;
		if ((opc & 3) == 3)
		{
			inst = rvfun::decode32(opc);
		}
		else
		{
			inst = rvfun::decode16(opc);
			is_compressed = true;
		}

		if (!inst)
		{
			std::cout << "No decode for " << std::hex << opc << std::dec << std::endl;
			continue;
		}
		//else
		std::cout << icount << '\t';
		if (!is_compressed) std::cout << ' ' << ' ';
		std::cout << inst->disasm();

		// pull producers for sources
		bool first = true;
		std::vector<rvfun::Inst::RegDep> srcs = inst->srcs();
		for (const auto &rd : srcs)
		{
			const uint32_t rn = uint32_t(rd.reg);

			uint32_t srci = 0;
			if (rd.rf == rvfun::Inst::RegFile::INT)
			{
				srci = prod_int[rn];
			}
			else if (rd.rf == rvfun::Inst::RegFile::FLOAT)
			{
				srci = prod_fp[rn];
			}

			if (srci != 0)
			{
				if (first) std::cout << '\t' << '[';
				else       std::cout << ',';
				std::cout << srci;
				first = false;
			}
		}
		if (!first) std::cout << ']';
		std::cout << std::endl;

		// update dests with this instruction as producer
		std::vector<rvfun::Inst::RegDep> dsts = inst->dsts();
		for (const auto &rd : dsts)
		{
			const uint32_t rn = uint32_t(rd.reg);

			if (rd.rf == rvfun::Inst::RegFile::INT)
			{
				prod_int[rn] = icount;
			}
			else if (rd.rf == rvfun::Inst::RegFile::FLOAT)
			{
				prod_fp[rn] = icount;
			}
		}
	}

	return 0;
}

