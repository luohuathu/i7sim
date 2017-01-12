#ifndef SIMPLECACHEEXT_H
#define SIMPLECACHEEXT_H

#include "SimpleCache.h"
#include <string>
using namespace std;

extern void Indent(int);

class SimpleCacheExt : public SimpleCache
{
public:
	SimpleCacheExt(int capacity, int blockSize, int ways, string name="")
		: SimpleCache(capacity, blockSize, ways)
	{
		LatAccess = LatRefill = LatInvalidate = 0;
		Name = name;
		report = true;
		FakeCache = capacity == 0;
	}
	~SimpleCacheExt() {}
	void SetLatency(int LatAccess, int LatRefill, int LatInvalidate)
	{
		this->LatAccess = LatAccess;
		this->LatRefill = LatRefill;
		this->LatInvalidate = LatInvalidate;
	}	

	int LatAccess, LatRefill, LatInvalidate;
	string Name;
	bool report;
	bool FakeCache;

	// advance #1: extend the methods in SimpleCache to do more work 
	bool Access(uint64_t addr, Operation op)
	{
		bool Hit = SimpleCache::Access(addr, op) > -1;
		TimeTemp += LatAccess;
		if (report)
			Indent(2), printf("%s in [%s]\n", Hit? "Hit" : "Miss", Name.c_str());
		return Hit;
	}

	bool Refill(SimpleCacheline * refilled_line)
	{
		bool Eviction = SimpleCache::Refill(refilled_line);
		TimeTemp += LatRefill;
		if (report)
			Indent(2), printf("Refill [%s]\n", Name.c_str());
		if (Eviction && report)	
			Indent(3), printf("Evict 0x%lx from [%s]\n", evicted->tag, Name.c_str());	
		return Eviction;
	}

	void Writeback(SimpleCacheline * dirty_line)
	{
		if (!FakeCache)	// do not write back for fake cache
			SimpleCache::Refill(dirty_line);
		TimeTemp += LatRefill;
		if (report)
			Indent(4), printf("Written back to [%s]\n", Name.c_str());				
	}

	// advance #2: accumulate time among different obj: require static 
	static int TimeTemp;
	static void ResetTimeTemp() { TimeTemp = 0; }
};

#endif

