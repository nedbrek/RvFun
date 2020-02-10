#ifndef RVFUN_SYSTEM_HPP
#define RVFUN_SYSTEM_HPP

namespace rvfun
{
class ArchState;

/// Interface to the virtualized operating system
class System
{
public:
	virtual void exit(ArchState &state) = 0;
	virtual void fstat(ArchState &state) = 0;
	virtual void open(ArchState &state) = 0;
	virtual void readlinkat(ArchState &state) = 0;
	virtual void sbrk(ArchState &state) = 0;
	virtual void uname(ArchState &state) = 0;
	virtual void read(ArchState &state) = 0;
	virtual void write(ArchState &state) = 0;
	virtual void writev(ArchState &state) = 0;
};
}

#endif

