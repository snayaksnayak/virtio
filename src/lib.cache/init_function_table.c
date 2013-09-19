#include "cache_internal.h"
#include "sysbase.h"
#include "resident.h"
#include "exec_funcs.h"

char CacheName[] = "cache.library";
char CacheVer[] = "\0$VER: cache.library 0.1 ("__DATE__")\r\n";

APTR cache_FuncTab[] =
{
	(void(*)) cache_OpenLib,
	(void(*)) cache_CloseLib,
	(void(*)) cache_ExpungeLib,
	(void(*)) cache_ExtFuncLib,

	(void(*)) cache_Configure,
	(void(*)) cache_Read,
	(void(*)) cache_Write,
	(void(*)) cache_Discard,
	(void(*)) cache_Sync,
	(void(*)) cache_Hit,
	(void(*)) cache_Dirty,

	(APTR) ((UINT32)-1)
};

struct CacheBase *cache_InitLib(CacheBase *CacheBase, UINT32 *segList, SysBase *SysBase)
{
	CacheBase->SysBase = SysBase;

	CacheBase->CacheBuffer = 0;
	CacheBase->CacheFlag = CF_INVALID;
	CacheBase->CacheNum = 0;

	return CacheBase;
}

static const struct CacheBase CacheData =
{
	.Library.lib_Node.ln_Name = (APTR)&CacheName[0],
	.Library.lib_Node.ln_Type = NT_LIBRARY,
	.Library.lib_Node.ln_Pri = -50,

	.Library.lib_OpenCnt = 0,
	.Library.lib_Flags = LIBF_SUMUSED|LIBF_CHANGED,
	.Library.lib_NegSize = 0,
	.Library.lib_PosSize = 0,
	.Library.lib_Version = CACHE_VERSION,
	.Library.lib_Revision = CACHE_REVISION,
	.Library.lib_Sum = 0,
	.Library.lib_IDString = (APTR)&CacheVer[7],

	//more (specific to library)

};

//Init table
struct InitTable
{
	UINT32	LibBaseSize;
	APTR	FunctionTable;
	APTR	DataTable;
	APTR	InitFunction;
} cache_InitTab =
{
	sizeof(CacheBase),
	cache_FuncTab,
	(APTR)&CacheData,
	cache_InitLib
};

static APTR CacheEndResident;

// Resident ROMTAG
struct Resident CacheRomTag =
{
	RTC_MATCHWORD,
	&CacheRomTag,
	&CacheEndResident,
	RTF_AUTOINIT | RTF_SINGLETASK,
	CACHE_VERSION,
	NT_LIBRARY,
	-50,
	CacheName,
	CacheVer,
	0,
	&cache_InitTab
};

