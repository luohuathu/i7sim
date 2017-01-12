#ifndef SIMPLEISA_H
#define SIMPLEISA_H

#include <stdint.h>
#include <cstdlib>

const int MaxOpcode = 4;
const int MaxOprCount = 3;

enum Opcode { Load, Store, Branch, Arith };

struct SimpleInstruction
{
	Opcode opcode;
	int oprcount;
	uint64_t opraddr[MaxOprCount];
};

static int Randomize2(float percentage1, float percentage2 = 0)
{
	int temp = rand() % RAND_MAX;
	return (temp > RAND_MAX * (1 - percentage1 - percentage2)) + (temp > RAND_MAX * (1 - percentage2));
}

// return a random instruction
const int locality = 32;
static SimpleInstruction RandInstruction()
{
	static uint64_t LastOprAddr = 0; // keep track of the last oprand addr to create locality
	
	SimpleInstruction instr;
	
	instr.opcode = (Opcode) (rand() % MaxOpcode);
	instr.oprcount = 1 + ( instr.opcode == Load) * Randomize2(0.3,0.4);	// a random number from 1 to 3
	for (int j = 0; j < instr.oprcount; j++)
		instr.opraddr[j] = LastOprAddr + rand() % locality; 
	
	LastOprAddr = instr.opraddr[instr.oprcount-1];	
	return instr;
}

#endif

