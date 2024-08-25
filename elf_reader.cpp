#include <iostream>
#include <libelf.h>
#include <unistd.h>
#include <fcntl.h>
#include <iomanip>

///@return string corresponding to header p_type
const char* hdrName(uint32_t p_type)
{
	switch (p_type)
	{
		case PT_LOAD        : return " LOAD";
		case PT_DYNAMIC     : return "  DYN";
		case PT_INTERP      : return "INTRP";
		case PT_NOTE        : return " NOTE";
		case PT_SHLIB       : return "SHLIB";
		case PT_PHDR        : return " PHDR";
		case PT_TLS         : return "  TLS";
		case PT_NUM         : return "  NUM";
		case PT_GNU_EH_FRAME: return "   EH";
		case PT_GNU_STACK   : return "STACK";
		case PT_GNU_RELRO   : return "RELRO";
	}
	return "UNKNOWN";
}

int main(int argc, char **argv)
{
	// check arg
	if (argc != 2)
	{
		std::cerr << "Usage: " << argv[0] << ' ' << "<elf file>" << std::endl;
		return 1;
	}

	// check ELF lib
	if (elf_version(EV_CURRENT) == EV_NONE)
	{
		std::cerr << "Error: Failed to load libelf" << std::endl;
		return 2;
	}

	// open the file
	const int fd = open(argv[1], O_RDONLY, 0);
	if (fd < 0)
	{
		std::cerr << "Error: Failed to open " << argv[1] << std::endl;
		return 3;
	}

	// pass it to ELF lib
	Elf *const e_hnd = elf_begin(fd, ELF_C_READ, nullptr);
	if (!e_hnd)
	{
		std::cerr << "Error: libelf failed to load " << argv[1] << std::endl;
		close(fd);
		return 4;
	}

	// check for exe
	if (elf_kind(e_hnd) != ELF_K_ELF)
	{
		std::cerr << "Error: File is not well formed: " << argv[1] << std::endl;
		elf_end(e_hnd);
		close(fd);
		return 5;
	}
	std::cout << "Opened " << argv[1] << std::endl;

	// pull ELF header
	Elf64_Ehdr *elf_hdr = elf64_getehdr(e_hnd);
	if (elf_hdr->e_ident[EI_CLASS] != 2)
	{
		std::cerr << "Error: 32 bit binary " << uint32_t(elf_hdr->e_ident[EI_CLASS]) << std::endl; // TODO - handle
		return 7;
	}

	// pull pheaders
	size_t num_headers = 0;
	if (elf_getphdrnum(e_hnd, &num_headers) != 0)
	{
		std::cerr << "Error: No headers!" << std::endl;
		elf_end(e_hnd);
		close(fd);
		return 6;
	}
	std::cout << "Found " << num_headers << " headers." << std::endl;

	// dump the header info
	Elf64_Phdr *hdrs = elf64_getphdr(e_hnd); // array of headers
	for (size_t i = 0; i < num_headers; ++i)
	{
		std::cout << std::setw(2) << i
		    << "   "
		    << hdrName(hdrs[i].p_type) << ' '
		    << std::hex
			 << std::setw(8) << hdrs[i].p_offset << ' '
			 << std::setw(8) << hdrs[i].p_vaddr << ' '
			 << std::setw(8) << hdrs[i].p_paddr << ' '
			 << std::setw(8) << hdrs[i].p_align << ' '
			 << std::setw(8) << hdrs[i].p_filesz << ' '
			 << std::setw(8) << hdrs[i].p_memsz << ' '
			 << std::setw(8) << uint32_t(hdrs[i].p_flags)
			 << std::dec
		    << std::endl;
	}

	elf_end(e_hnd);
	close(fd);
	return 0;
}

