#include "SimpleSimulatorExt.h"
#include "SimpleISA.h"

int SimpleCacheExt::TimeTemp = 0;
void Indent(int n=1)
{
	for (int i = 0; i < n; i++)
		printf("\t");
}

SimpleSimulator::SimpleSimulator()
{
	dTLB = new SimpleCacheExt (64, 1, 4, "dTLB");	// 64 entries, 4 way
	iTLB = new SimpleCacheExt (128, 1, 4, "iTLB");	// 128 entries, 4 way
	TLB = new SimpleCacheExt (512, 1, 4, "TLB");	// 512 entries, 4 way
	iL1C = new SimpleCacheExt (32 * 1024, 64, 4, "iL1C");	// 32KB, 64B block, 4 way
	dL1C = new SimpleCacheExt (32 * 1024, 64, 8, "dL1C");	// 32KB, 64B block, 8 way
	L2C = new SimpleCacheExt (256 * 1024, 64, 8, "L2C");	// 256KB, 64B block, 8 way
	
	dTLB->SetLatency(4, 4, 1);
	iTLB->SetLatency(4, 4, 1);
	TLB->SetLatency(8, 8, 1);
	iL1C->SetLatency(4, 4, 1);
	dL1C->SetLatency(4, 4, 1);
	L2C->SetLatency(8, 8, 1);

	// just need to use their name and latency field, not actual caches
	L3C = new SimpleCacheExt (0, 1, 1, "L3C");	// capacity = 0 means fake cache
	Memory = new SimpleCacheExt (0, 1, 1, "Memory");
	Disk = new SimpleCacheExt (0, 1, 1, "Disk");
	L3C->SetLatency(16, 16, 1);
	Memory->SetLatency(100, 100, 0);	// will not invalidate
	Disk->SetLatency(100000, 100000, 0);

	iTLBHier = new SimpleCacheExt * [5] {dTLB, iTLB, TLB, Memory, Disk};
	dTLBHier = new SimpleCacheExt * [5] {iTLB, dTLB, TLB, Memory, Disk};
	iCacheHier = new SimpleCacheExt * [5] {dL1C, iL1C, L2C, L3C, Memory};
	dCacheHier = new SimpleCacheExt * [5] {iL1C, dL1C, L2C, L3C, Memory};

	SimulateLength = 0;
	Time = 0;
	report = true;
}

SimpleSimulator::~SimpleSimulator()
{
	delete dTLB, iTLB, TLB, iL1C, dL1C, L2C, L3C, Memory, Disk;
}

int SimpleSimulator::MagicPageTableLookup(uint64_t virAddr)
{
	VirAddr = virAddr;
	VirPageIdx = VirAddr >> LogPageSize;		

	if (MagicPageTable.find(VirPageIdx) == MagicPageTable.end())// this is the correct way
		PhyPageIdx = MagicPageTable[VirPageIdx] = rand() % (1 << PhyAddrSpace);  // create a random mapping
	else PhyPageIdx = MagicPageTable[VirPageIdx];
	
	PhyAddr = (PhyPageIdx << LogPageSize) + VirAddr % (1 << LogPageSize);
	return 0;
}

int SimpleSimulator::HackyPTorL3Sim(SimpleCacheExt **L)
{
	bool rVPMiss = Randomize2(rVPHitPercent);
	bool rVL3CacheMiss = Randomize2(rVL3CHitPercent);
	bool L3Hit = L[3]->Name == "L3C" ? rVL3CacheMiss : rVPMiss;

	if (report)
		Indent(2), printf("%s in [%s]\n", L3Hit ? "Hit" : "Miss", L[3]->Name.c_str());
	if (report && !L3Hit)
		Indent(2), printf("%s in [%s]\n", "Hit", L[4]->Name.c_str());
	return L[3]->LatAccess + (!L3Hit) * L[4]->LatAccess;
}

int SimpleSimulator::CacheOrTLBHierAccess(SimpleCacheExt **L, uint64_t addr, Operation op)
{	
	int TimeTemp = 0;
	SimpleCacheExt::ResetTimeTemp();	
	bool L1Hit = L[1]->Access(addr, op);	// look up L1
	if (L1Hit)
		return 0;

	bool L2Hit = L[2]->Access(addr, op);	// if miss, look up L2
	
	if (!L2Hit)
	{
		TimeTemp += HackyPTorL3Sim(L);  // handle the fake simulation	
		
		bool Eviction = L[2]->Refill(L[2]->missed);	// refill L2		
		if (Eviction)
		{			
			bool dirty = L[2]->evicted->dirty;
			// back invalidate in both icache and dcache and check dirty	
			if (report)	
				Indent(4), printf("Back invalidated from [%s] and [%s]\n", L[1]->Name.c_str(), L[0]->Name.c_str());
			dirty |= L[1]->Invalidate(L[2]->evicted->tag) == 1;	
			dirty |= L[0]->Invalidate(L[2]->evicted->tag) == 1;
			TimeTemp += L[1]->LatInvalidate;

			if (dirty)
				L[3]->Writeback(L[2]->evicted);  // no actual writeback will happen since L3 is not simulated
		}
	}	

	if (!L1Hit)
	{	
		bool Eviction = L[1]->Refill(L[1]->missed);	// refill L1		
		if (Eviction && L[1]->evicted->dirty)
			L[2]->Writeback(L[1]->evicted); // this refill will not cause an eviction due to inclusiveness		
	}
	TimeTemp += SimpleCacheExt::TimeTemp;
	if (report)	
		Indent(2), printf("Elapsed time = %d cycles\n", TimeTemp);
	return TimeTemp;
}

int SimpleSimulator::MemoryAccess(uint64_t virAddr, Operation op, IDType IDtype)
{
	MagicPageTableLookup(virAddr);
	int TimePT, TimeCache;

	if (report)	
		Indent(), printf("Address translation for 0x%lx\n", VirAddr);
	TimePT = CacheOrTLBHierAccess(IDtype == Instr ? iTLBHier : dTLBHier, VirPageIdx, Read);

	if (report)						
		Indent(), printf("Access physical address 0x%lx\n", PhyAddr);
	TimeCache = CacheOrTLBHierAccess(IDtype == Instr ? iCacheHier : dCacheHier, PhyAddr / CachelineSize, op);
	Time += max(TimePT, TimeCache);	// TODO: change this model		
}

int SimpleSimulator::Run()
{
	printf("Start simulation for %d instructions ...\n", SimulateLength);
	uint64_t PC = 65536;		// initial PC, just a random choice
	int SimulatedInstr = 0;
	uint64_t CurrentTime = 0;	
	SimpleInstruction CurrentInstr;
	
	while (SimulatedInstr < SimulateLength)
	{
		// fetch two instructions
		if (report) printf("\nInstruction fetch at 0x%lx\n", PC);	
		MemoryAccess(PC, Read, Instr);
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
						if (report) printf("\nOperand %s at 0x%lx\n", CurrentInstr.opcode == Load ? "fetch" : "store", CurrentInstr.opraddr[j]);
						MemoryAccess(CurrentInstr.opraddr[j], CurrentInstr.opcode == Load ? Read: Write, Data);
					}				
					break;
					
				case Branch:
					// randomize the branch outcome
					BranchTaken = Randomize2(1-BranchTakenPercent);
					PC = PC - 16 + CurrentInstr.opraddr[0] * InstrSize;		// jump a multiple of instructions
					if (report) printf("\nBranch to 0x%lx \n\t%s\n", PC, BranchTaken ? "Taken" : "Not taken");
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
