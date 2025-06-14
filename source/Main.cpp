#include "engine/EngineCore.h"
#include "engine/application/Application.h"


int main()
{
	parus::Application application;
	
	try
	{
		application.init();
		application.loop();
		application.clean();
	}
	catch (const std::exception& exception)
	{
		LOG_FATAL(exception.what());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
