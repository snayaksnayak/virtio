#include "exec_funcs.h"
#include "lib_cache_internal.h"

#define SysBase LibCacheBase->SysBase

struct LibCacheBase* lib_cache_OpenLib(LibCacheBase *LibCacheBase)
{
    LibCacheBase->Library.lib_OpenCnt++;

	return(LibCacheBase);
}

APTR lib_cache_CloseLib(LibCacheBase *LibCacheBase)
{
	LibCacheBase->Library.lib_OpenCnt--;

	return (LibCacheBase);
}

APTR lib_cache_ExpungeLib(LibCacheBase *LibCacheBase)
{
	return (NULL);
}

APTR lib_cache_ExtFuncLib(void)
{
	return (NULL);
}


//**************


void lib_cache_Configure(LibCacheBase *LibCacheBase)
{
}
void lib_cache_Read(LibCacheBase *LibCacheBase)
{
}
void lib_cache_Write(LibCacheBase *LibCacheBase)
{
}
void lib_cache_Discard(LibCacheBase *LibCacheBase)
{
}
void lib_cache_Sync(LibCacheBase *LibCacheBase)
{
}
