#ifdef USEEXTVERSION
	#include "SimpleSimulatorExt.h"
#else
	#include "SimpleSimulator.h"
#endif

using namespace std;

int main()
{
	SimpleSimulator sim;
	sim.SimulateLength = 10000;
	sim.Run();

	return 0;
}
