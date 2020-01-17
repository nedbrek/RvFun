#include "host_system.hpp"
#include "arch_state.hpp"
#include "sparse_mem.hpp"
#include <elf.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <iostream>

namespace rvfun
{
HostSystem::HostSystem()
: mem_(new SparseMem)
{
}

ArchMem* HostSystem::getMem() { return mem_.get(); }

bool HostSystem::loadElf(const char *prog_name, ArchState &state)
{
	const int ifd = open(prog_name, O_RDONLY);
	if (ifd < 0)
	{
		std::cerr << "Failed to open " << prog_name << std::endl;
		return true;
	}
	struct stat s;
	if (::fstat(ifd, &s) < 0)
	{
		std::cerr << "Failed to stat " << prog_name << std::endl;
		return true;
	}
	const size_t file_size = s.st_size;
	uint8_t *elf_mem = static_cast<uint8_t*>(mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, ifd, 0));
	if (!elf_mem)
	{
		std::cerr << "Failed to mmap " << prog_name << std::endl;
		return true;
	}

	// overlay ELF header structure
	const Elf64_Ehdr *const eh64 = reinterpret_cast<const Elf64_Ehdr*>(elf_mem);
	if (eh64->e_ident[0] != 0x7f ||
	    eh64->e_ident[1] != 'E' ||
	    eh64->e_ident[2] != 'L' ||
	    eh64->e_ident[3] != 'F')
	{
		std::cerr << "Badly formed ELF " << prog_name << std::endl;
		return true;
	}
	if (eh64->e_ident[EI_CLASS] != ELFCLASS64)
	{
		// TODO - handle 32 bit (code 1)
		std::cerr << "Not a 64 bit exe" << std::endl;
		return true;
	}
	// TODO check arch for RISCV

	for (Elf64_Half i = 0; i < eh64->e_phnum; ++i)
	{
		const Elf64_Phdr *const phdr = reinterpret_cast<const Elf64_Phdr*>(elf_mem + eh64->e_phoff + eh64->e_phentsize * i); // phdr[i]
		if (phdr->p_type != PT_LOAD)
			continue;

		// TODO expand block for alignment?
		// copy segment to specified VA
		std::cout << "Load block of size " << phdr->p_filesz;
		if (phdr->p_filesz < phdr->p_memsz)
		{
			// create bigger block of zeroes
			uint8_t *block = reinterpret_cast<uint8_t*>(calloc(phdr->p_memsz, 1));

			// copy in what we have
			memcpy(block, elf_mem + phdr->p_offset, phdr->p_filesz);

			// add it
			mem_->addBlock(phdr->p_vaddr, phdr->p_memsz, block);

			const uint64_t end_of_block = phdr->p_vaddr + phdr->p_memsz - 1;
			if (end_of_block > top_of_mem_)
				top_of_mem_ = end_of_block;

			free(block);

			std::cout << '(' << phdr->p_memsz << ')';
		}
		else
		{
			mem_->addBlock(phdr->p_vaddr, phdr->p_filesz, elf_mem + phdr->p_offset);

			const uint64_t end_of_block = phdr->p_vaddr + phdr->p_filesz - 1;
			if (end_of_block > top_of_mem_)
				top_of_mem_ = end_of_block;
		}

		std::cout << " from 0x"
		    << std::hex << phdr->p_offset
		    << " to VA 0x" << phdr->p_vaddr << std::dec
		    << std::endl;
	}
	std::cout << "Top of memory is 0x" << std::hex << top_of_mem_ << std::dec << std::endl;

	// allocate SP
	const uint32_t stack_sz = 32 * 1024;
	const uint64_t sp = 0x10000000;
	mem_->addBlock(sp, stack_sz);

	state.setReg(Reg::SP, sp + stack_sz/2); // put SP in the middle

	// set entry point
	state.setPc(eh64->e_entry);

	return false;
}

void HostSystem::exit(ArchState &state)
{
	const uint64_t status = state.getReg(10);
	if (status != 0)
		std::cerr << "Program exited with non-zero status: " << status << std::endl;

	exited_ = true;
}

void HostSystem::fstat(ArchState &state)
{
	const uint64_t fd = state.getReg(10);
	uint64_t path_p = state.getReg(11);
	//state.getReg(12) stat buf

	std::cerr << " fstat fd=" << fd
	          << " path='";
	if (path_p)
	{
		uint8_t val = state.readMem(path_p, 1);
		if (!val)
			std::cerr << "(null str)";

		while (val)
		{
			std::cerr << val;

			++path_p;
			val = state.readMem(path_p, 1);
		}
		std::cerr << '\'';
	}

	state.setReg(10, 0); // success!
}

void HostSystem::sbrk(ArchState &state)
{
	const uint64_t new_top_of_mem = state.getReg(15);
	if (new_top_of_mem == 0) // get current
	{
		state.setReg(10, top_of_mem_);
		return;
	}
	if (new_top_of_mem <= top_of_mem_)
	{
		// TODO: shrinkage
		state.setReg(10, top_of_mem_);
		return;
	}

	// alloc more mem
	const uint64_t delta = new_top_of_mem - top_of_mem_;
	mem_->addBlock(top_of_mem_+1, delta, nullptr);

	top_of_mem_ = new_top_of_mem;
	state.setReg(10, top_of_mem_);
}

void HostSystem::uname(ArchState &state)
{
	const uint64_t buf = state.getReg(10);
	if (buf == 0)
	{
		state.setReg(10, -1); // bad buf
		return;
	}

	constexpr uint32_t UTS_LEN = 65;
	constexpr uint32_t NUM_FIELDS = 6;
	// zero buf
	for (uint32_t i = 0; i < UTS_LEN*NUM_FIELDS; ++i)
	{
		state.writeMem(buf+i, 1, 0);
	}

	// first member: system name
	const std::string sysname("Linux");
	for (uint32_t i = 0; i < sysname.size(); ++i)
	{
		state.writeMem(buf+i, 1, sysname[i]);
	}

	// second member: node name
	// buf+UTS_LEN: leave blank

	// third member: release
	const std::string release("4.4.0");
	for (uint32_t i = 0; i < release.size(); ++i)
	{
		state.writeMem(buf+i+2*UTS_LEN, 1, release[i]);
	}

	// leave rest blank (TODO: if anyone cares)
	// version, machine, domainname

	state.setReg(10, 0); // success
}

void HostSystem::write(ArchState &state)
{
	const uint64_t fd = state.getReg(10);
	uint64_t buf = state.getReg(11);
	uint64_t ct = state.getReg(12);
	uint64_t bytes_written = 0;
	if (fd == 1) // stdout
	{
		std::cout << ' ' << '\'';
		while (ct)
		{
			const uint8_t v = state.readMem(buf, 1);
			std::cout << ' ' << uint32_t(v);
			++buf;
			--ct;
			++bytes_written;
		}
		std::cout << '\'';
	}
	state.setReg(10, bytes_written);
}

}

