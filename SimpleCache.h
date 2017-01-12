#ifndef SIMPLECACHE_H
#define SIMPLECACHE_H

#include <stdint.h>
#include <cstdio>
//#define NDEBUG	// stop checking asserts by uncommenting this line
#include <assert.h>

class SimpleCacheline
{
public:
	SimpleCacheline(int valid = 0, uint64_t tag = 0, uint64_t last_time = 0, int dirty = 0)
	{
		this->valid = valid;
		this->tag = tag;
		this->last_time = last_time;
		this->dirty = dirty; 
	}
	
	bool valid;
	uint64_t tag;
	uint64_t last_time;
	bool dirty;
};

enum Operation { Read, Write };
enum InvaliType { Notfound, Dirty, Notdirty};

class SimpleCache
{
public:
	SimpleCache(int capacity, int blockSize, int ways);
	~SimpleCache();
	void DebugOn() { debug = 1;}
	
	int Access(uint64_t addr, Operation op);
	int Refill(SimpleCacheline * refilled_line);
	InvaliType Invalidate(uint64_t addr);
	
	SimpleCacheline *missed, *evicted;
private:
	int Capacity;	// in Bytes, power of 2
	int BlockSize;  // in Bytes, power of 2
	int Ways;
	int Blocks;
	int Sets;	
	
	SimpleCacheline * cache;

	uint64_t numAccess; // logic time stamp for LRU
	bool debug;
	
	int Find(uint64_t addr);
	int ReplacementLogic(SimpleCacheline * refilled_line);
};

#endif
