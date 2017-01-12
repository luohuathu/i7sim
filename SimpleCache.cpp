#include "SimpleCache.h"

SimpleCache::SimpleCache(int capacity, int blockSize, int ways)
{
	Capacity = capacity;
	BlockSize =  blockSize;
	Ways = ways;
	
	Blocks = Capacity / BlockSize;
	Sets = Blocks / Ways;
	
	cache = new SimpleCacheline [Blocks];
	missed = new SimpleCacheline;	
	evicted = new SimpleCacheline;
	
	numAccess = 0;	
	debug = 0;
}

SimpleCache::~SimpleCache() 
{ 
	delete []cache;
	delete missed, evicted;
}

int SimpleCache::Find(uint64_t addr)	// all addresses are cacheline indices without block offsets
{
	int set = addr % Sets;
	for (int j = 0; j < Ways; j++)
	{
		SimpleCacheline * current = cache + set * Ways + j;
		if (current->valid && current->tag == addr)
			return set * Ways + j;
	}
	return -1;		// not found
}

int SimpleCache::Access(uint64_t addr, Operation op)
{
	numAccess++;
	int hit_at = Find(addr);
	if (hit_at >= 0)
	{
		cache[hit_at].last_time = numAccess;
		if (op == Write) 
			cache[hit_at].dirty = 1; // set dirty	
	}
	*missed = SimpleCacheline(hit_at == -1, addr, numAccess);	
	
	if (debug)
		printf("Access %lx, hit at location %d\n", addr, hit_at);
	return hit_at;
}

InvaliType SimpleCache::Invalidate(uint64_t addr)
{	
	int hit_at = Find(addr);
	if (hit_at >= 0)
		cache[hit_at].valid = 0;
	if (debug)
		printf("Invalidate %lx, at location %d\n", addr, hit_at);	

	if (hit_at == -1)
		return Notfound;	// not present
	else
		return cache[hit_at].dirty? Dirty : Notdirty;	// 1 = dirty; 0 = not dirty
}

int SimpleCache::Refill(SimpleCacheline * refilled_line)
{	
	assert(refilled_line->valid);  // there is not necessarily a miss if it's an exclusive eviction
	
	int hit_at = Find(refilled_line->tag);
	if (hit_at >= 0)	
	{
		cache[hit_at] = *refilled_line;	// the line is present, do a write back
		evicted->valid = 0;				// no eviction
	}
	else
	{
		int replaced = ReplacementLogic(refilled_line);
		*evicted = cache[replaced];
		cache[replaced] = *refilled_line;
	}
		
	if (debug)
	{
		printf("Refill (%d, %lx, %ld), ", refilled_line->valid, refilled_line->tag, refilled_line->last_time);
		if (evicted->valid)
			printf("evicting %ld\n", evicted->tag);
		else
			printf("no eviction\n");
	}	
	return evicted->valid;		// 0 = no eviction; 1 = evicted some other line
}

int SimpleCache::ReplacementLogic(SimpleCacheline * refilled_line)
{
	int set = refilled_line->tag % Sets;
	int replaced = set * Ways;
	for (int j = 0; j < Ways; j++)
	{
		SimpleCacheline *current = cache + set * Ways + j;
		if (!current->valid)	// if there's an empty spot, no eviction
			return set * Ways + j;
		else if (current->last_time < cache[replaced].last_time) // else, look for the least recent used line
			replaced = set * Ways + j;
	}
	return replaced;
}
