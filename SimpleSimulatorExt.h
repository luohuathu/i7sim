#ifndef SIMPLESIMULATOR
#define SIMPLESIMULATOR

#include "SimpleCacheExt.h"
#include <map>
using namespace std;

const int CachelineSize = 64;
const int PhyAddrSpace = 16;
const int LogPageSize = 12;
const int InstrSize = 8;

const float BranchTakenPercent = 0.1;
const float rVPHitPercent = 0.8;
const float rVL3CHitPercent = 0.7;

enum IDType { Instr, Data };

class SimpleSimulator
{
public:
	SimpleSimulator();
	~SimpleSimulator();
	int Run();
	uint32_t SimulateLength;
	bool report;

private:
	uint64_t Time;
	SimpleCacheExt *iTLB, *dTLB, *TLB, *iL1C, *dL1C, *L2C, *L3C, *Memory, *Disk;
	SimpleCacheExt **iTLBHier, **dTLBHier, **iCacheHier, **dCacheHier;
	int MemoryAccess(uint64_t virAddr, Operation op, IDType IDtype);	
	int CacheOrTLBHierAccess(SimpleCacheExt **L, uint64_t addr, Operation op);
	int HackyPTorL3Sim(SimpleCacheExt **L);

	map<uint64_t, uint64_t> MagicPageTable;
	uint64_t VirAddr, PhyAddr, VirPageIdx, PhyPageIdx; // to get results from MagicPageTableLookup()
	int MagicPageTableLookup(uint64_t virAddr);
};

#endif
