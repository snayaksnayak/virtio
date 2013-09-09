#include "exec_funcs.h"
#include "lib_cache_internal.h"

#define SysBase LibCacheBase->SysBase

struct LibCacheBase* lib_cache_OpenLib(struct LibCacheBase *LibCacheBase)
{
    LibCacheBase->Library.lib_OpenCnt++;

	return(LibCacheBase);
}

APTR lib_cache_CloseLib(struct LibCacheBase *LibCacheBase)
{
	LibCacheBase->Library.lib_OpenCnt--;

	return (LibCacheBase);
}

APTR lib_cache_ExpungeLib(struct LibCacheBase *LibCacheBase)
{
	return (NULL);
}

APTR lib_cache_ExtFuncLib(void)
{
	return (NULL);
}


//**************


void lib_cache_Configure()
{
}
void lib_cache_Read()
{
}
void lib_cache_Write()
{
}
void lib_cache_Discard()
{
}
void lib_cache_Sync()
{
}
