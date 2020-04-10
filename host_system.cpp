#include "host_system.hpp"
#include "arch_state.hpp"
#include "sparse_mem.hpp"
#include <elf.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <sstream>

namespace
{
uint64_t padTo16(uint64_t begin, uint64_t alignment = 16)
{
	uint32_t pad = begin & (alignment - 1);
	if (pad)
		begin += alignment - pad;

	return begin;
}
}

namespace rvfun
{
HostSystem::HostSystem()
: mem_(new SparseMem)
{
}

HostSystem::~HostSystem()
{
}

ArchMem* HostSystem::getMem() { return mem_.get(); }

bool HostSystem::loadElf(const char *prog_name, ArchState &state)
{
	const int ifd = ::open(prog_name, O_RDONLY);
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
	uint8_t *elf_mem = static_cast<uint8_t*>(::mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, ifd, 0));
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

		// copy segment to specified VA
		const auto file_sz = phdr->p_filesz;
		std::cout << "Load block of size " << file_sz;
		auto tgt_sz = file_sz; // hopefully 1:1 from file to memory
		if (phdr->p_memsz > tgt_sz)
			tgt_sz = phdr->p_memsz;

		// expand block for alignment
		const uint64_t align_mask = phdr->p_align - 1;
		const uint64_t end_addr = phdr->p_vaddr + tgt_sz;
		const uint64_t spill = end_addr & align_mask;
		if (spill)
		{
			// grow target to meet alignment
			tgt_sz += phdr->p_align - spill;
		}

		if (file_sz < tgt_sz)
		{
			// create bigger block of zeroes
			uint8_t *block = reinterpret_cast<uint8_t*>(calloc(tgt_sz, 1));

			// copy in what we have
			memcpy(block, elf_mem + phdr->p_offset, phdr->p_filesz);

			// add it
			mem_->addBlock(phdr->p_vaddr, tgt_sz, block);

			const uint64_t end_of_block = phdr->p_vaddr + tgt_sz - 1;
			if (end_of_block > top_of_mem_)
				top_of_mem_ = end_of_block;

			free(block);

			std::cout << '(' << tgt_sz << ')';
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

	// set entry point
	state.setPc(eh64->e_entry);

	prog_name_ = prog_name; // save program name
	return false; // no errors
}

void HostSystem::addArg(const std::string &s)
{
	args_.emplace_back(s);
}

void HostSystem::completeEnv(ArchState &state)
{
	// allocate SP
	const uint32_t stack_sz = 4096 * 1024; // 4 MB
	const uint64_t sp = 0x10000000;
	mem_->addBlock(sp, stack_sz);

	// complete environment
	const uint32_t sim_argc = args_.size() + 1; // include argv[0] (program name)
	std::vector<uint64_t> sim_argv;

	//---figure out how big it all is
	uint32_t env_sz = padTo16(prog_name_.size() + 1); // argv[0] (program name)

	for (const auto &arg : args_)
	{
		env_sz += padTo16(arg.size()+1);
	}

	const uint64_t top_of_stack = sp + stack_sz;
	mmap_zone_ = top_of_stack + sp; // push mmap way up
	const uint64_t start_pt = top_of_stack - env_sz - 16; // leave 16B near top of stack
	std::cout << "Copying environment to "
	    << std::hex << start_pt << std::dec
	    << ' ' << env_sz << " bytes."
	    << std::endl;

	// copy the args into virtual environment
	uint64_t ptr = start_pt;
	sim_argv.emplace_back(ptr);
	for (const auto &c : prog_name_)
		state.writeMem(ptr++, 1, c);
	state.writeMem(ptr++, 1, 0);
	ptr = padTo16(ptr);

	for(const auto &arg : args_)
	{
		sim_argv.emplace_back(ptr);
		for (const auto &c : arg)
			state.writeMem(ptr++, 1, c);
		state.writeMem(ptr++, 1, 0);
		ptr = padTo16(ptr);
	}

	// TODO envp
	std::cout << "Environment configured. End ptr: " << std::hex << ptr << std::dec << std::endl;

	const uint64_t final_sp = sp + stack_sz/2;

	// write args to stack
	ptr = final_sp;
	//--- argc
	state.writeMem(ptr, 8, sim_argc);
	ptr += 8;

	//--- argv[]
	for (const auto &a : sim_argv)
	{
		state.writeMem(ptr, 8, a);
		ptr += 8;
	}
	//--- TODO envp

	state.setReg(Reg::SP, final_sp); // put SP in the middle
	state.setReg(10, sim_argc);
	state.setReg(11, final_sp); // argv

	// set up standard file descriptors
	if (stdin_file_.empty())
	{
		fds_.push_back(-1); // block access to stdin
	}
	else
	{
		const int sim_stdin = ::open(stdin_file_.c_str(), O_RDONLY);
		if (sim_stdin < 0)
		{
			std::cerr << "No stdin " << stdin_file_ << std::endl;
			fds_.push_back(-1); // block access to stdin
		}
		else
		{
			std::cerr << "Using stdin " << stdin_file_ << std::endl;
			fds_.push_back(sim_stdin);
		}
	}

	// remap stdout
	std::ostringstream os;
	os << "stdout." << getpid();
	const uint32_t stdout_fd = ::open(os.str().c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0666);
	fds_.push_back(stdout_fd);

	// remap stderr
	os.str("");
	os << "stderr." << getpid();
	const uint32_t stderr_fd = ::open(os.str().c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0666);
	fds_.push_back(stderr_fd);
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
	const uint64_t buf = state.getReg(12);

	if (fd == 1) // stdout
	{
		//state.writeMem(buf+16, 4, 0x81b6); // mode: block device
		state.writeMem(buf+16, 4, 0x2190); // mode: char device
		state.writeMem(buf+56, 8, 8192); // block size

		state.setReg(10, 0); // success!
		return;
	}

	if (!path_p)
	{
		std::cerr << " fstat fd=" << fd
		    << " path=null ptr";
		state.setReg(10, -1); // error
		return;
	}

	bool bad_chars = false;
	std::string pathname;
	char val = state.readMem(path_p, 1);
	while (val)
	{
		if (val < 32 || val > 127)
			bad_chars = true;
		else
			pathname.push_back(val);

		++path_p;
		val = state.readMem(path_p, 1);
	}

	if (pathname.empty())
	{
		// pass through
		//--- check open files
		if (fd >= fds_.size())
		{
			state.setReg(10, -1); // error
			return;
		}

		struct stat s;
		const int ret = ::fstat(fds_[fd], &s);

		if (ret == 0)
		{
			state.writeMem(buf+16, 4, s.st_mode); // mode
			state.writeMem(buf+56, 8, s.st_blksize); // block size
			// TODO: other fields?
		}

		state.setReg(10, ret);
		return;
	}
	//else
	std::cerr << " fstat fd=" << fd
	          << " path='";
	if (!bad_chars)
		std::cerr << pathname;
	else
		std::cerr << "(bad path)";
	std::cerr << '\'';

	state.setReg(10, 0); // success!
}

void HostSystem::mmap(ArchState &state)
{
	const uint64_t addr = state.getReg(10);
	const uint64_t len = state.getReg(11);
	const uint64_t prot = state.getReg(12);
	const uint64_t flags = state.getReg(13);
	const uint64_t fd = state.getReg(14);
	const uint64_t offset = state.getReg(15);

	const uint64_t out_addr = mmap_zone_;
	mmap_zone_ += padTo16(len, 4096);
	if (flags & 0x20)
	{
		// annonymous (no file)
		mem_->addBlock(out_addr, len);
		state.setReg(10, out_addr); // success!
		return; // done
	}

	std::cerr << ' ' << __FUNCTION__
	    << ' ' << addr
	    << ' ' << len
	    << ' ' << prot
	    << ' ' << flags
	    << ' ' << fd
	    << ' ' << offset
	;
	if (fd <= 2 || fd >= fds_.size() || len == 0)
	{
		state.setReg(10, -1); // error
		return;
	}

	void *ptr = ::mmap(nullptr, len, prot, flags, fds_[fd], offset);
	mem_->addBlock(out_addr, len, ptr);
	::munmap(ptr, len);

	state.setReg(10, out_addr); // success!
}

void HostSystem::open(ArchState &state)
{
	const uint64_t dirfd = state.getReg(10);
	const uint64_t path = state.getReg(11);
	const uint64_t flags = state.getReg(12);
	const uint64_t mode = state.getReg(13);

	if (path == 0)
	{
		state.setReg(10, -1); // bad path
		return;
	}

	bool bad_chars = false;
	std::ostringstream pns;

	uint32_t off = 0;
	uint8_t pval = state.readMem(path+off, 1);
	while (pval)
	{
		if (pval < 32 || pval > 127)
			bad_chars = true;
		else
			pns << pval;

		++off;
		pval = state.readMem(path+off, 1);
	}

	std::string pathname = pns.str();
	if (pathname == "/dev/tty")
	{
		state.setReg(10, 1); // stdout
		return;
	}

	std::cerr << " openat " << dirfd << ' ';
	if (!bad_chars)
		std::cerr << '\'' << pathname << '\'';
	else
		std::cerr << "(bad path)";
	std:: cerr << ' ' << flags << ' ' << mode;

	// TODO: whitelist file access and redirect writes
	int host_flags = O_RDONLY;
	if (flags != 0)
	{
		host_flags = O_WRONLY | O_CREAT | O_TRUNC;
		std::ostringstream os;
		os << pathname << '.' << getpid();
		pathname = os.str();
		std::cerr << " openat write file " << pathname << std::endl;
	}
	const uint32_t new_fd = ::open(pathname.c_str(), host_flags, 0666);

	const uint32_t sim_fd = fds_.size();
	fds_.emplace_back(new_fd);

	state.setReg(10, sim_fd);
}

void HostSystem::readlinkat(ArchState &state)
{
	const uint64_t dirfd = state.getReg(10);
	const uint64_t path = state.getReg(11);
	const uint64_t buf = state.getReg(12);
	const uint64_t buf_sz = state.getReg(13);

	if (!path || !buf || !buf_sz)
	{
		state.setReg(10, -1); // error: null ptr
		return;
	}

	std::ostringstream pns; // pathname string
	uint32_t off = 0;
	char pval = state.readMem(path + off, 1);
	bool bad_chars = false;
	while (pval)
	{
		if (pval < 32 || pval > 127)
			bad_chars = true;
		else
			pns << pval;

		++off;
		pval = state.readMem(path + off, 1);
	}

	const std::string pathname = pns.str();

	// TODO: /proc/self/exe should resolve to program name (full path)
	if (pathname != "/proc/self/exe")
	{
		std::cerr << " readlinkat " << dirfd << ' ';
		if (!bad_chars)
			std::cerr << '\'' << pathname << '\'';
		else
			std::cerr << "(bad path)";
		std:: cerr << ' ' << buf << ' ' << buf_sz;

		uint32_t bytes_copied = 0;
		state.setReg(10, bytes_copied);
		return;
	}

	off = 0;
	for (const auto &v : pathname)
	{
		state.writeMem(buf + off, 1, v);
		++off;
		if (off == buf_sz)
			break;
	}
	state.setReg(10, off);
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
	const std::string release("4.15.0");
	for (uint32_t i = 0; i < release.size(); ++i)
	{
		state.writeMem(buf+i+2*UTS_LEN, 1, release[i]);
	}

	// leave rest blank (TODO: if anyone cares)
	// version, machine, domainname

	state.setReg(10, 0); // success
}

void HostSystem::read(ArchState &state)
{
	const uint64_t fd = state.getReg(10);
	const uint64_t buf = state.getReg(11);
	const uint64_t ct = state.getReg(12);

	if (fd >= fds_.size() || buf == 0)
	{
		state.setReg(10, -1); // error
		return;
	}

	const auto sim_fd = fds_[fd];
	const std::unique_ptr<uint8_t[]> sim_buf(new uint8_t[ct ? ct : 1]);

	const int ret = ::read(sim_fd, sim_buf.get(), ct);
	for (int b = 0; b < ret; ++b)
	{
		state.writeMem(buf+b, 1, sim_buf[b]);
	}

	state.setReg(10, ret);
}

uint64_t HostSystem::writeBuf(ArchState &state, uint64_t buf, uint64_t ct)
{
	uint64_t bytes_written = 0;
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
	return bytes_written;
}

void HostSystem::write(ArchState &state)
{
	const uint64_t fd = state.getReg(10);
	if (fd >= fds_.size())
	{
		state.setReg(10, -1); // error
		return;
	}

	const uint64_t buf = state.getReg(11);
	const uint64_t ct = state.getReg(12);

	const uint32_t sim_fd = fds_[fd];
	for (uint32_t i = 0; i < ct; ++i)
	{
		// TODO: read/write larger blocks
		const uint8_t byte = state.readImem(buf+i, 1);
		const size_t ret = ::write(sim_fd, &byte, 1);
		if (ret != 1)
		{
			state.setReg(10, -1);
			return;
		}
	}
	state.setReg(10, ct);
}

void HostSystem::writev(ArchState &state)
{
	uint64_t iovec = state.getReg(11);
	if (iovec == 0)
	{
		state.setReg(10, -1);
		return;
	}

	const uint64_t fd = state.getReg(10);
	uint64_t iovct = state.getReg(12);
	uint64_t bytes_written = 0;
	if (fd == 1) // stdout
	{
		while (iovct)
		{
			const uint64_t buf = state.readMem(iovec, 8);
			const uint64_t ct = state.readMem(iovec+8, 8);
			bytes_written += writeBuf(state, buf, ct);

			--iovct;
			iovec += 16;
		}
	}
	else
	{
		std::cerr << " Writev to fd " << fd << std::endl;
	}
	state.setReg(10, bytes_written);
}

}

