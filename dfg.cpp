#include "inst.hpp"
#include <getopt.h>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <sstream>

class DotPrinter
{
public:
	~DotPrinter()
	{
		if (do_print_)
			f_ << '}' << std::endl;
	}

	void setPrint() { do_print_ = true; }

	void start()
	{
		if (do_print_)
		{
			f_.open("dfg.dot");
			f_ << "strict digraph {" << std::endl;
		}
	}

	void print(uint32_t node, const std::string &label)
	{
		if (do_print_)
		{
			f_ << node << " [label =\"" << label << "\"]" << std::endl;
		}
	}

	void printEdge(uint32_t p, uint32_t c)
	{
		if (do_print_)
			f_ << p << " -> " << c << std::endl;
	}

private:
	std::ofstream f_;
	bool do_print_ = false;
};

int main(int argc, char **argv)
{
	if (argc == 1)
	{
		std::cerr << "Usage: " << argv[0] << " [-f opcode_file] [elf_file]" << std::endl;
		return 1;
	}

	DotPrinter dp;
	const char *op_file = nullptr;
	const char *optstring = "+f:p";
	int optc = getopt_long(argc, argv, optstring, nullptr, nullptr);
	while (optc != -1)
	{
		if (optc == 'f')
		{
			op_file = optarg;
		}
		else if (optc == 'p')
		{
			dp.setPrint();
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
	dp.start();

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
		const std::string disasm = inst->disasm();
		std::ostringstream los;
		los << icount << ' ' << disasm;
		std::string label(los.str());
		std::cout << disasm;

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
				if (first)
				{
					dp.print(icount, label);
					std::cout << '\t' << '[';
				}
				else
				{
					std::cout << ',';
				}
				std::cout << srci;
				first = false;
				dp.printEdge(srci, icount);
			}
		}
		if (!first) std::cout << ']';
		else dp.print(icount, label);
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

