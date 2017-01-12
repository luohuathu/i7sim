#ifndef SIMPLECACHEHIER_H
#define SIMPLECACHEHIER_H

#include "SimpleCache.h"
#include <memory.h>
#include <string>
using namespace std;

void Indent(int n=1);

class SimpleCacheHier
{
public:
	SimpleCacheHier(int hierarchy, SimpleCache * cachePtrs []);
	~SimpleCacheHier();
	void ReportOn() { report = 1; }
	void SetNames(const string *);
	void SetInclusive() { Inclusive = 1; }
	void SetExclusive() { Inclusive = 0; }

	int Access(uint64_t addr, Operation op);
	void Refill();

	int (*HackyFunctionPtr) ();

private:
	SimpleCache ** CachePtrs;
	string * Names;

	int Hierarchy;
	int HitLevel;
	bool Inclusive;

	bool * RefilledLvls;	// if a level is refilled. True for inclusive; possibly cascaded eviction from a previous level for exclusive
	bool * EvictedLvls;		// if a line is evicted from a level during refill. 

	void RefillInclusive();
	void RefillExclusive();
	
	bool report;
};

#endif
