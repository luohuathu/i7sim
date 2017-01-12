#include <cstdio>
#include <cstdlib>
#include <map>
#include "SimpleCacheHier.h"	// if something similar to this line causes errors, you must be forgetting #ifndef #define #endif 
#include "SimpleISA.h"

const int CachelineSize = 64;
const int PhyAddrSpace = 16;
const int LogPageSize = 12;
const int InstrSize = 8;

uint64_t MagicPageTableLookup(uint64_t VirPageIdx)
{	
	static std::map<uint64_t, uint64_t> MagicPageTable;
	
	int oldSize = MagicPageTable.size();
	uint64_t PhyPageIdx = MagicPageTable[VirPageIdx];
	int newSize = MagicPageTable.size();
	if (newSize > oldSize)		// a new element was just created
		PhyPageIdx = MagicPageTable[VirPageIdx] = rand() % (1 << PhyAddrSpace);
	return 	PhyPageIdx;
}

int HackyPageTable()
{
	Indent(2), printf("Hit in [PT]\n");
	bool rVPMiss = 0;		// TODO: randomize the outcome
	// TOOD: if page fault, access disk.	
	// no page fault can occur on TLB hit, because the TLB entries will be invalidated if a page is swapped out.
	return 0;			// TODO: return latency, maybe calculate latency as well?
}

int HackyMemoryDisk()
{
	bool rVL3CacheMiss = 0;		// TODO: randomize the outcome
	Indent(2), printf("%s in [L3]\n", rVL3CacheMiss ? "Miss" : "Hit");		
	
	return 0;			// TODO: return latency, maybe calculate latency as well?
}

int main()
{
	SimpleCache * iTLB = new SimpleCache (128, 1, 4);	// 128 entries, 4 way
	SimpleCache * dTLB = new SimpleCache (64, 1, 4);	// 64 entries, 4 way
	SimpleCache * TLB = new SimpleCache (512, 1, 4);	// 512 entries, 4 way

	SimpleCache * iL1 = new SimpleCache (32 * 1024, 64, 4);	// 32KB, 64B block, 4 way
	SimpleCache * dL1 = new SimpleCache (32 * 1024, 64, 8);	// 32KB, 64B block, 8 way
	SimpleCache * L2 = new SimpleCache (256 * 1024, 64, 8);	// 256KB, 64B block, 8 way

	SimpleCache * instrTLB[2] = {iTLB, TLB};
	SimpleCache * dataTLB[2] = {dTLB, TLB};
	SimpleCache * instrCache[2] = {iL1, L2};
	SimpleCache * dataCache[2] = {dL1, L2};	

	SimpleCacheHier *instrTLBHier = new SimpleCacheHier(2, instrTLB);
	SimpleCacheHier *dataTLBHier = new SimpleCacheHier(2, dataTLB);
	SimpleCacheHier *instrCacheHier = new SimpleCacheHier(2, instrCache);
	SimpleCacheHier *dataCacheHier = new SimpleCacheHier(2, dataCache);

	string names [3] = {"iTLB", "TLB", "PT"};
	instrTLBHier->SetNames(names);
	names[0] = "dTLB";
	dataTLBHier->SetNames(names);
	names[0] = "iL1";
	names[1] = "L2";
	names[2] = "Memory";
	instrCacheHier->SetNames(names);
	names[0] = "dL1";
	dataCacheHier->SetNames(names);

	instrTLBHier->SetInclusive();
	dataTLBHier->SetInclusive();
	instrCacheHier->SetInclusive();
	dataCacheHier->SetInclusive();

	instrTLBHier->ReportOn();
	dataTLBHier->ReportOn();
	instrCacheHier->ReportOn();
	dataCacheHier->ReportOn();
	
	// register hacky callback functions
	instrTLBHier->HackyFunctionPtr = dataTLBHier->HackyFunctionPtr = & HackyPageTable;
	instrCacheHier->HackyFunctionPtr = dataCacheHier->HackyFunctionPtr = & HackyMemoryDisk;

/*
	//iTLB->DebugOn();
	//TLB->DebugOn();	
	
	instrTLBHier->Access(0, Read);
	instrTLBHier->Access(0, Read);
	instrTLBHier->Access(32, Read);
	instrTLBHier->Access(64, Read);
	instrTLBHier->Access(128, Read);
	instrTLBHier->Access(256, Read);	
	instrTLBHier->Access(0, Read);
	instrTLBHier->Access(512, Read);	
	instrTLBHier->Access(384, Read);	

	for (int i=0; i < 1000; i++)
		int hit_at = instrTLBHier->Access(rand(), Read);
*/

	uint64_t PC = 65536;		// initial PC, just a random choice
	int SimulatedInstr = 0;
	int SimulateLength = 100;	// # instructions to simulate
	
	SimpleInstruction CurrentInstr;
	uint64_t CurrentTime = 0;
	
	uint64_t VirAddr, PhyAddr, VirPageIdx, PhyPageIdx;
	while (SimulatedInstr < SimulateLength)
	{
		// fetch two instructions
		printf("\nInstruction fetch at 0x%lx\n", PC);
		VirAddr = PC;
		VirPageIdx = VirAddr >> LogPageSize;		
		PhyPageIdx = MagicPageTableLookup(VirPageIdx);			// a magic page table, does not affect timing
		PhyAddr = (PhyPageIdx << LogPageSize) + VirAddr % (1 << LogPageSize);
		
		Indent(), printf("Address translation for 0x%lx\n", VirAddr);
		instrTLBHier->Access(VirPageIdx, Read);
		Indent(), printf("Access physical address 0x%lx\n", PhyAddr);
		instrCacheHier->Access(PhyAddr / CachelineSize, Read);  // input is cacheline index
		
		PC = PC + 2 * InstrSize;
		for (int k = 0; k < 2; k++)
		{
			CurrentInstr = RandInstruction();
			SimulatedInstr++;
			
			bool BranchTaken = false;		
			switch (CurrentInstr.opcode)
			{
				case Load:
				case Store:
					for (int j = 0; j < CurrentInstr.oprcount; j++)
					{
						printf("\nOprand %s at 0x%lx\n", CurrentInstr.opcode == Load ? "fetch" : "store", CurrentInstr.opraddr[j]);
						VirAddr = CurrentInstr.opraddr[j];
						VirPageIdx = VirAddr >> LogPageSize;		
						PhyPageIdx = MagicPageTableLookup(VirPageIdx);	
						PhyAddr = (PhyPageIdx << LogPageSize) + VirAddr % (1 << LogPageSize);
		
						Indent(), printf("Address translation for 0x%lx\n", VirAddr);
						dataTLBHier->Access(VirPageIdx, Read);
						Indent(), printf("Access physical address 0x%lx\n", PhyAddr);
						dataCacheHier->Access(PhyAddr / CachelineSize, CurrentInstr.opcode == Load ? Read: Write);
					}				
					break;
					
				case Branch:
					// TODO: randomize the branch outcome
					BranchTaken = true;
					PC = PC - 16 + CurrentInstr.opraddr[0] * InstrSize;		// jump a multiple of instructions
					printf("\nBranch to 0x%lx \n\t\t%s\n", PC, BranchTaken ? "Taken" : "Not taken");
					break;
					
				default:
					break;	
			}
			
			if (BranchTaken)
				break;		// do not execute the next instruction; go to branch		
		}		
	}	
	return 0;
}
