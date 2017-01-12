#ifndef SIMPLESIMULATOR
#define SIMPLESIMULATOR

#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include "SimpleCache.h"
using namespace std;

const int CachelineSize = 64;
const int PhyAddrSpace = 16;
const int LogPageSize = 12;
const int InstrSize = 8;

const float BranchTakenPercent = 0.1;
const float rVPMissPercent = 0.2;
const float rVL3CMissPercent = 0.3; 

enum IDType { Instr, Data };
enum PMType { Page, PhyMem };

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
	int TimeTemp;
	string *Names, *PTNameI, *PTNameD, *CacheNameI, *CacheNameD;

	SimpleCache *iTLB, *dTLB, *TLB, *iL1C, *dL1C, *L2C;
	SimpleCache *L1, *L2, *OtherL1;	// temp variables used in CacheOrTLBHierAccess()
	int *Latency, *LatencyRefill, LatencyInvalidate;
	int *LatencyC, *LateCRefill, LatencyCInvalidate, *LatencyPT, *LatePTRefill, LatencyPInvalidate, LatencyMem, LatencyDisk;

	IDType IDtype;	
	PMType PMtype;
	int MemoryAccess(uint64_t virAddr, Operation op);	
	int CacheOrTLBHierAccess(uint64_t addr, Operation op);
	int PTorL3Sim();

	map<uint64_t, uint64_t> MagicPageTable;
	uint64_t VirAddr, PhyAddr, VirPageIdx, PhyPageIdx; // to get results from MagicPageTableLookup()
	int MagicPageTableLookup(uint64_t virAddr);
};

#endif
