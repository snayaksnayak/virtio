#include "lib_cache_internal.h"
#include "sysbase.h"
#include "resident.h"
#include "exec_funcs.h"

char LibCacheLibName[] = "lib_cache.library";
char LibCacheLibVer[] = "\0$VER: lib_cache.library 0.1 ("__DATE__")\r\n";

APTR lib_cache_FuncTab[] =
{
	(void(*)) lib_cache_OpenLib,
	(void(*)) lib_cache_CloseLib,
	(void(*)) lib_cache_ExpungeLib,
	(void(*)) lib_cache_ExtFuncLib,

	(void(*)) lib_cache_Configure,
	(void(*)) lib_cache_Read,
	(void(*)) lib_cache_Write,
	(void(*)) lib_cache_Discard,
	(void(*)) lib_cache_Sync,

	(APTR) ((UINT32)-1)
};

struct LibCacheBase *lib_cache_InitLib(struct LibCacheBase *LibCacheBase, UINT32 *segList, struct SysBase *SysBase)
{
	LibCacheBase->SysBase = SysBase;

	return LibCacheBase;
}

static const struct LibCacheBase LibCacheLibData =
{
	.Library.lib_Node.ln_Name = (APTR)&LibCacheLibName[0],
	.Library.lib_Node.ln_Type = NT_LIBRARY,
	.Library.lib_Node.ln_Pri = -50,

	.Library.lib_OpenCnt = 0,
	.Library.lib_Flags = LIBF_SUMUSED|LIBF_CHANGED,
	.Library.lib_NegSize = 0,
	.Library.lib_PosSize = 0,
	.Library.lib_Version = LIB_CACHE_VERSION,
	.Library.lib_Revision = LIB_CACHE_REVISION,
	.Library.lib_Sum = 0,
	.Library.lib_IDString = (APTR)&LibCacheLibVer[7],

	//more (specific to library)

};

//Init table
struct InitTable
{
	UINT32	LibBaseSize;
	APTR	FunctionTable;
	APTR	DataTable;
	APTR	InitFunction;
} lib_cache_InitTab =
{
	sizeof(struct LibCacheBase),
	lib_cache_FuncTab,
	(APTR)&LibCacheLibData,
	lib_cache_InitLib
};

static APTR LibCacheEndResident;

// Resident ROMTAG
struct Resident LibCacheRomTag =
{
	RTC_MATCHWORD,
	&LibCacheRomTag,
	&LibCacheEndResident,
	RTF_AUTOINIT | RTF_SINGLETASK,
	LIB_CACHE_VERSION,
	NT_LIBRARY,
	-50,
	LibCacheLibName,
	LibCacheLibVer,
	0,
	&lib_cache_InitTab
};

