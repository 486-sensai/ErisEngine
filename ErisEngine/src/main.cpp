#include "ErisEngine.h"


int main()
{
	ErisEngine engine;

	try
	{
		engine.Eris_init();
		engine.Eris_run();
		engine.Eris_cleanup();
	}
	catch (const std::exception&e)
	{
		std::cerr << "Engine Error: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}