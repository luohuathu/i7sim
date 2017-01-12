#include "SimpleSimulator.h"
#include "SimpleISA.h"

void Indent(int n=1)
{
	for (int i = 0; i < n; i++)
		printf("\t");
}

SimpleSimulator::SimpleSimulator()
{
	dTLB = new SimpleCache (64, 1, 4);	// 64 entries, 4 way
	iTLB = new SimpleCache (128, 1, 4);	// 128 entries, 4 way
	TLB = new SimpleCache (512, 1, 4);	// 512 entries, 4 way

	iL1C = new SimpleCache (32 * 1024, 64, 4);	// 32KB, 64B block, 4 way
	dL1C = new SimpleCache (32 * 1024, 64, 8);	// 32KB, 64B block, 8 way
	L2C = new SimpleCache (256 * 1024, 64, 8);	// 256KB, 64B block, 8 way

	LatencyC = new int [3] {4, 8, 16};
	LateCRefill =new int [3] {4,8,16};
	LatencyCInvalidate = 2;
	LatencyPT = new int [3] {4, 8, 100};
	LatePTRefill = new int [3] {4, 8, 100};
	LatencyPInvalidate = 2;
	LatencyMem = 100;
	LatencyDisk = 100000;

	PTNameI = new string [4] {"iTLB", "TLB", "PT", "dTLB"};
	PTNameD = new string [4] {"dTLB", "TLB", "PT", "iTLB"};
	CacheNameI = new string [4] {"iL1C", "L2C", "L3C", "dL1C"};
	CacheNameD = new string [4] {"dL1C", "L2C", "L3C", "iL1C"};

	SimulateLength = 0;
	Time = 0;
	report = true;
}

SimpleSimulator::~SimpleSimulator()
{
	delete dTLB, iTLB, TLB, iL1C, dL1C, L2C;
	delete[] LatencyC, LateCRefill, LatencyPT, LatePTRefill;
	delete[] PTNameI, PTNameD, CacheNameI, CacheNameD;
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

int SimpleSimulator::PTorL3Sim()
{
	TimeTemp += Latency[2];
	if (PMtype == Page)
	{
		if (report)
			Indent(2), printf("Hit in [PT]  (%d cycles)\n", Latency[2]);
		bool rVPMiss = Randomize2(rVPMissPercent);		// randomize the outcome
		TimeTemp += rVPMiss * LatencyDisk;
		if ( report && rVPMiss )
			Indent(3), printf("Page fault!  (%d cycles)\n", LatencyDisk);	
	}
	else
	{
		bool rVL3CacheMiss = Randomize2(rVL3CMissPercent);	// randomize the outcome		
		if (report)
			Indent(2), printf("%s in [L3]  (%d cycles)\n", rVL3CacheMiss ? "Miss" : "Hit", Latency[2]);
		TimeTemp += rVL3CacheMiss * LatencyMem;	// calculate latency
		if (report && rVL3CacheMiss)
			Indent(2), printf("Hit in [Memeory]  (%d cycles)\n", LatencyMem);			
	}
	return 0;
}

int SimpleSimulator::CacheOrTLBHierAccess(uint64_t addr, Operation op)
{	
	TimeTemp = 0;
	bool L1Hit = L1->Access(addr, op) > -1; // look up L1
	TimeTemp += Latency[0];
	if (report)
		Indent(2), printf("%s in [%s]  (%d cycles)\n", L1Hit? "Hit" : "Miss", Names[0].c_str(), Latency[0]);
	if (L1Hit)
		return 0;

	// if miss, look up L2
	bool L2Hit = L2->Access(addr, op) > -1;
	TimeTemp += Latency[1];
	if (report)
		Indent(2), printf("%s in [%s]  (%d cycles)\n", L2Hit? "Hit" : "Miss", Names[1].c_str(), Latency[1]);
	
	if (!L2Hit)
	{
		PTorL3Sim();  // handle the fake simulation	
				
		// refill L2
		if (report)
			Indent(2), printf("Refill [%s]  (%d cycles)\n", Names[1].c_str(), LatencyRefill[1]);		
		bool Eviction = L2->Refill(L2->missed);
		TimeTemp += LatencyRefill[1];
		
		if (Eviction)
		{
			if (report)	
				Indent(3), printf("Evict 0x%lx from [%s]\n", L2->evicted->tag, Names[1].c_str());
			
			// back invalidate in both icache and dcache
			if (report)	
				Indent(3), printf("Back invalidated from [%s] and [%s]  (%d cycles)\n", Names[0].c_str(), Names[3].c_str(), LatencyInvalidate);			
			InvaliType outcome1 = L1->Invalidate(L2->evicted->tag);	
			InvaliType outcome2 = OtherL1->Invalidate(L2->evicted->tag);
			TimeTemp += LatencyInvalidate;

			bool dirty = (outcome1 == Dirty || outcome2 == Dirty || L2->evicted->dirty);	// check dirty in L2, L1I and L1D
			if (dirty)
			{
				TimeTemp += LatencyRefill[2];
				if (report)	
					Indent(3), printf("Written back to [%s]  (%d cycles)\n", Names[2].c_str(), LatencyRefill[2]);														
				// no need for the actual write back because L3 is not simulated		
			}
		}
	}	

	if (!L1Hit)
	{
		// refill L1
		if (report)
			Indent(2), printf("Refill [%s]  (%d cycles)\n", Names[0].c_str(), LatencyRefill[0]);		
		bool Eviction = L1->Refill(L1->missed);
		TimeTemp += LatencyRefill[0];		

		if (Eviction)
		{
			if (report)	
				Indent(3), printf("Evict 0x%lx from [%s]\n", L1->evicted->tag, Names[0].c_str());

			bool dirty = L1->evicted->dirty;	// check dirty,
			if (dirty)
			{
				if (report)	
					Indent(3), printf("Written back to [%s]  (%d cycles)\n", Names[1].c_str(), LatencyRefill[1]);								
				L2->Refill(L1->evicted);	// this refill will not cause an eviction due to inclusiveness	
				TimeTemp += LatencyRefill[1];	
			}
		}		
	}
}

int SimpleSimulator::MemoryAccess(uint64_t virAddr, Operation op)
{
	MagicPageTableLookup(virAddr);
	int TimeOneAccess = 0;

	// setup the temp variables for page table walk and then just call CacheOrTLBHierAccess()
	if (report)	
		Indent(), printf("Address translation for 0x%lx\n", VirAddr);
	L1 = IDtype == Instr ? iTLB : dTLB;
	OtherL1 = IDtype == Instr ? dTLB : iTLB;
	L2 = TLB;
	Names = IDtype == Instr? PTNameI : PTNameD;
	PMtype = Page;
	Latency = LatencyPT;
	LatencyRefill = LatePTRefill;
	LatencyInvalidate = LatencyPInvalidate;
	CacheOrTLBHierAccess(VirPageIdx, Read);	
	if(report)
		Indent(2), printf("Time elapsed: %d clock cycles\n", TimeTemp);
	TimeOneAccess = TimeTemp;

	// setup the temp variables for physical memory access and then just call CacheOrTLBHierAccess()
	if (report)						
		Indent(), printf("Access physical address 0x%lx\n", PhyAddr);
	L1 = IDtype == Instr ? iL1C : dL1C;
	OtherL1 = IDtype == Instr ? dL1C : iL1C;
	L2 = L2C;
	Names = IDtype == Instr? CacheNameI : CacheNameD;
	PMtype = PhyMem;
	Latency = LatencyC;
	LatencyRefill = LateCRefill;
	LatencyInvalidate = LatencyCInvalidate;
	CacheOrTLBHierAccess(PhyAddr / CachelineSize, op);	
	if(report)
		Indent(2), printf("Time elapsed: %d clock cycles\n", TimeTemp);
	TimeOneAccess += TimeTemp;
	TimeOneAccess -= LatencyPT[0];	// parallel TLB and cache access
	
	if(report)
		Indent(), printf("Time elapsed for this access: %d clock cycles\n", TimeOneAccess);
	Time += TimeOneAccess;		
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
		IDtype = Instr;		
		MemoryAccess(PC, Read);
		PC = PC + 2 * InstrSize;
		for (int k = 0; k < 2; k++)
		{
			CurrentInstr = RandInstruction();
			SimulatedInstr++;
			
			bool BranchTaken = false;
			uint64_t PCnew;		
			switch (CurrentInstr.opcode)
			{
				case Load:
				case Store:
					for (int j = 0; j < CurrentInstr.oprcount; j++)
					{						
						if (report) printf("\nOperand %s at 0x%lx\n", CurrentInstr.opcode == Load ? "fetch" : "store", CurrentInstr.opraddr[j]);
						IDtype = Data;
						MemoryAccess(CurrentInstr.opraddr[j], CurrentInstr.opcode == Load ? Read: Write);
					}				
					break;
					
				case Branch:
					// randomize the branch outcome
					BranchTaken = Randomize2(BranchTakenPercent);
					PCnew = PC + (CurrentInstr.opraddr[0] - 2 + k) * InstrSize;
					if (report) printf("\nBranch to 0x%lx\n\t%s\n", PCnew, BranchTaken ? "Taken" : "Not taken");
					if (BranchTaken)
						PC = PCnew;	// jump a multiple of instructions
					break;
					
				default:
					break;	
			}
			
			if (BranchTaken)
				break;		// do not execute the next instruction; go to branch		
		}
	}
	printf("Total time elapsed: %ld cycles.\n", Time);			
	return 0;
}
