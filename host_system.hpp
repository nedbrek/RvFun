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

	ArchMem* getMem();
	bool loadElf(const char *prog_name, ArchState &state);
	bool hadExit() const { return exited_; }

	//---from System
	void exit(ArchState &state) override;
	void fstat(ArchState &state) override;
	void sbrk(ArchState &state) override;
	void uname(ArchState &state) override;
	void write(ArchState &state) override;

private:
	std::unique_ptr<SparseMem> mem_;
	uint64_t top_of_mem_ = 0;
	bool exited_ = false;
};

}

#endif

