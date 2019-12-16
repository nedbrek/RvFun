#include "inst.hpp"
#include "sparse_mem.hpp"
#include "simple_arch_state.hpp"
#include <elf.h>
#include <iostream>
#include <iomanip>
#include <getopt.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

using namespace rvfun;

int main(int argc, char **argv)
{
	if (argc == 1)
	{
		std::cerr << "Usage: " << argv[0] << "[-i instruction_count] <elf file>" << std::endl;
		return 1;
	}

	uint64_t max_icount = 0;
	int optc = getopt_long(argc, argv, "i:", nullptr, nullptr);
	while (optc != -1)
	{
		if (optc == 'i')
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

	const int ifd = open(prog_name, O_RDONLY);
	if (ifd < 0)
	{
		std::cerr << "Failed to open " << prog_name << std::endl;
		return 2;
	}
	struct stat s;
	if (fstat(ifd, &s) < 0)
	{
		std::cerr << "Failed to stat " << prog_name << std::endl;
		return 3;
	}
	const size_t file_size = s.st_size;
	uint8_t *elf_mem = static_cast<uint8_t*>(mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, ifd, 0));
	if (!elf_mem)
	{
		std::cerr << "Failed to mmap " << prog_name << std::endl;
		return 4;
	}

	// overlay ELF header structure
	const Elf64_Ehdr *const eh64 = reinterpret_cast<const Elf64_Ehdr*>(elf_mem);
	if (eh64->e_ident[0] != 0x7f ||
	    eh64->e_ident[1] != 'E' ||
	    eh64->e_ident[2] != 'L' ||
	    eh64->e_ident[3] != 'F')
	{
		std::cerr << "Badly formed ELF " << prog_name << std::endl;
		return 5;
	}
	if (eh64->e_ident[EI_CLASS] != ELFCLASS64)
	{
		// TODO - handle 32 bit (code 1)
		std::cerr << "Not a 64 bit exe" << std::endl;
		return 6;
	}
	// TODO check arch for RISCV

	SparseMem mem;
	for (Elf64_Half i = 0; i < eh64->e_phnum; ++i)
	{
		const Elf64_Phdr *const phdr = reinterpret_cast<const Elf64_Phdr*>(elf_mem + eh64->e_phoff + eh64->e_phentsize * i); // phdr[i]
		if (phdr->p_type == PT_LOAD)
		{
			// TODO expand block for alignment?
			// copy segment to specified VA 
			std::cout << "Load block of size "
			    << phdr->p_filesz
			    << " from 0x"
			    << std::hex << phdr->p_offset
				 << " to VA 0x" << phdr->p_vaddr << std::dec
				 << std::endl;
			mem.addBlock(phdr->p_vaddr, phdr->p_filesz, elf_mem + phdr->p_offset);
		}
	}

	SimpleArchState state;
	state.setMem(&mem);

	// allocate SP
	const uint32_t stack_sz = 32 * 1024;
	const uint64_t sp = 0x10000000;
	mem.addBlock(sp, stack_sz);

	state.setReg(Reg::SP, sp + stack_sz/2); // put SP in the middle

	// set entry point
	state.setPc(eh64->e_entry);

	uint64_t icount = 0;
	while (1)
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

		++icount;
		if (max_icount != 0 && icount >= max_icount)
			break;
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

