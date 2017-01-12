#include "SimpleCacheHier.h"

void Indent(int n)
{
	for (int i = 0; i < n; i++)
		printf("\t");
}

SimpleCacheHier::SimpleCacheHier(int hierarchy, SimpleCache * cachePtrs [])
{
	Hierarchy = hierarchy;
	CachePtrs = new SimpleCache * [Hierarchy];
	for (int i = 0; i < Hierarchy; i++)
		CachePtrs[i] = cachePtrs[i];

	RefilledLvls = new bool [Hierarchy];
	EvictedLvls = new bool [Hierarchy];
	
	report = 0;
	Names = new string [Hierarchy+1];
	HackyFunctionPtr = NULL;	// by default, no hacky callback
}

SimpleCacheHier::~SimpleCacheHier()
{
	delete []CachePtrs;
}

void SimpleCacheHier::SetNames(const string * names)
{	
	for (int i = 0; i < Hierarchy+1; i++)
		Names[i] = names[i];
}

int SimpleCacheHier::Access(uint64_t addr, Operation op)
{
	int Latency = 0;	// TODO: calculate latency
	bool hit = false;
	for (HitLevel = 0; HitLevel < Hierarchy; HitLevel++)
	{
		hit = CachePtrs[HitLevel]->Access(addr, op) > -1;
		if (report)
			Indent(2), printf("%s in [%s]\n", hit? "Hit" : "Miss", Names[HitLevel].c_str());		
		if (hit)
			break;
	}
	
	if (HitLevel == Hierarchy && HackyFunctionPtr) // hacky callback for the required fake simulate
		(*HackyFunctionPtr) ();
	
	Refill();

	return Latency;		
}

void SimpleCacheHier::Refill()
{
	if (Inclusive)
		RefillInclusive();
	else
		RefillExclusive();
}

void SimpleCacheHier::RefillInclusive()
{
	bool eviction, dirty;
	dirty = 0;
	for (int i = HitLevel-1; i >= 0; i--)
	{
		RefilledLvls[i] = 1;
		if (report)
			Indent(2), printf("Refill [%s]\n", Names[i].c_str());		
		eviction = CachePtrs[i]->Refill(CachePtrs[0]->missed);	// Refill the missed cache

		if (!eviction)
			continue;
				
		SimpleCacheline * evicted = CachePtrs[i]->evicted;
		if (report)	
			Indent(3), printf("Evict 0x%lx from [%s]\n", evicted->tag, Names[i].c_str());
			
		// back invalidation; do we count cycles here??
		// TODO: back invalidate in both icache and dcache?
		for (int j = 0; j < i; j++)
		{
			int outcome = CachePtrs[j]->Invalidate(evicted->tag);  //-1 = not present; 1 = dirty; 0 = not dirty
			if (outcome == 1)
				dirty = 1;
			if (report)	
				Indent(4), printf("Back invalidated from [%s]\n", Names[i].c_str());			
		}			

		EvictedLvls[i] = eviction && dirty;		
		if (EvictedLvls[i])			// need to write back to evicted dirty line
		{
			if (report)	
				Indent(4), printf("Written back to [%s]\n", Names[i+1].c_str());							
			if (i+1 < Hierarchy) 	// do the actual write only if it is in the hierarchy (being modeled)			
				CachePtrs[i+1]->Refill(evicted);	// this refill will not cause an eviction due to inclusiveness			
		}
	}
}

void SimpleCacheHier::RefillExclusive()
{
	memset(RefilledLvls, 0, sizeof(bool) * Hierarchy);
	memset(EvictedLvls, 0, sizeof(bool) * Hierarchy);	
	RefilledLvls[0] = 1;		// only refill the smallest level
	if (report)
		Indent(2), printf("Refill [%s]\n", Names[0].c_str());	
	EvictedLvls[0] = CachePtrs[0]->Refill( CachePtrs[0]->missed );
	
	for (int i = 0; i < Hierarchy; i++)
	{			
		if (EvictedLvls[i])		// no notion of dirty; need to write back any evicted line for exclusive cache					
		{
			if (report)	
				Indent(3), printf("Write back %lx to [%s]\n", CachePtrs[i]->evicted->tag, Names[i+1].c_str());				
			if (i+1 < Hierarchy) 	// do the actual write only if it is in the hierarchy (being modeled)			
				EvictedLvls[i+1] = CachePtrs[i+1]->Refill( CachePtrs[i]->evicted );
		}
		else break;
	}
}
