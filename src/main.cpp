#include <vk_engine.h>
#include <Windows.h>
#include <tchar.h>
#include <string>
int main(int argc, char* argv[])
{
	VulkanEngine engine;
	//DWORD dwError, dwPriClass;
	//dwPriClass = GetPriorityClass(GetCurrentProcess());

	//_tprintf(TEXT("Current priority class is 0x%x\n"), dwPriClass);

	// Read the first argument as a string if provided
	std::string firstArg;
	if (argc > 1) {
		firstArg = argv[1];
		_tprintf(TEXT("First argument: %hs\n"), firstArg.c_str());
	}


	//if (!SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS))
	//{
	//	dwError = GetLastError();
	//	if (ERROR_PROCESS_MODE_ALREADY_BACKGROUND == dwError)
	//		_tprintf(TEXT("Already in background mode\n"));
	//	else _tprintf(TEXT("Failed to enter background mode (%d)\n"), dwError);
	//
	//}

	//dwPriClass = GetPriorityClass(GetCurrentProcess());

	//_tprintf(TEXT("Current priority class is 0x%x\n"), dwPriClass);
	engine.init(firstArg);
	
	engine.run();	

	engine.cleanup();	

	return 0;
}
