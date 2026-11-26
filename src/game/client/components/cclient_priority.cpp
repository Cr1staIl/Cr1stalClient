/* Cr1stal Client: process priority of the game. Based on DDNet, see licence.txt. */
#include "cclient_gameplay.h"

#if defined(CONF_FAMILY_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

void CcSetHighProcessPriority(bool High)
{
#if defined(CONF_FAMILY_WINDOWS)
	SetPriorityClass(GetCurrentProcess(), High ? HIGH_PRIORITY_CLASS : NORMAL_PRIORITY_CLASS);
#else
	(void)High;
#endif
}
