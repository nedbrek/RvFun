#ifndef RVFUN_HOST_SYSTEM_HPP
#define RVFUN_HOST_SYSTEM_HPP

#include "system.hpp"
#include <cstdint>
#include <memory>

namespace rvfun
{
class ArchMem;
class SparseMem;

class HostSystem : public System
{
public:
	HostSystem();
	~HostSystem();

	ArchMem* getMem();
	bool loadElf(const char *prog_name, ArchState &state);
	bool hadExit() const { return exited_; }

	//---from System
	void exit(ArchState &state) override;
	void fstat(ArchState &state) override;
	void open(ArchState &state) override;
	void readlinkat(ArchState &state) override;
	void sbrk(ArchState &state) override;
	void uname(ArchState &state) override;
	void write(ArchState &state) override;
	void writev(ArchState &state) override;

private: // methods
	uint64_t writeBuf(ArchState &state, uint64_t buf, uint64_t ct);

private: // data
	std::unique_ptr<SparseMem> mem_; ///< memory image
	std::vector<uint32_t> fds_; ///< open file descriptors
	uint64_t top_of_mem_ = 0; ///< cache highest block in mem image
	bool exited_ = false; ///< track calls to exit()
};

}

#endif

