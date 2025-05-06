#include <vk_engine.h>
#include <Windows.h>
#include <tchar.h>
int main(int argc, char* argv[])
{
	VulkanEngine engine;
	DWORD dwError, dwPriClass;
	dwPriClass = GetPriorityClass(GetCurrentProcess());

	_tprintf(TEXT("Current priority class is 0x%x\n"), dwPriClass);


	if (!SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS))
	{
		dwError = GetLastError();
		if (ERROR_PROCESS_MODE_ALREADY_BACKGROUND == dwError)
			_tprintf(TEXT("Already in background mode\n"));
		else _tprintf(TEXT("Failed to enter background mode (%d)\n"), dwError);
	
	}

	dwPriClass = GetPriorityClass(GetCurrentProcess());

	_tprintf(TEXT("Current priority class is 0x%x\n"), dwPriClass);
	engine.init();	
	
	engine.run();	

	engine.cleanup();	

	return 0;
}
