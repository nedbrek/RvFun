#ifndef RVFUN_SYSTEM_HPP
#define RVFUN_SYSTEM_HPP

namespace rvfun
{
class ArchState;

/// Interface to the virtualized operating system
class System
{
public:
	virtual void sbrk(ArchState &state) = 0;
};
}

#endif

