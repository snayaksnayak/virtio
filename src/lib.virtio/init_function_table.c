#include "virtio_internal.h"
#include "sysbase.h"
#include "resident.h"
#include "exec_funcs.h"

char VirtioName[] = "virtio.library";
char VirtioVer[] = "\0$VER: virtio.library 0.1 ("__DATE__")\r\n";

APTR virtio_FuncTab[] =
{
	(void(*)) virtio_OpenLib,
	(void(*)) virtio_CloseLib,
	(void(*)) virtio_ExpungeLib,
	(void(*)) virtio_ExtFuncLib,

	(void(*)) virtio_Write8,
	(void(*)) virtio_Write16,
	(void(*)) virtio_Write32,
	(void(*)) virtio_Read8,
	(void(*)) virtio_Read16,
	(void(*)) virtio_Read32,

	(void(*)) virtio_ExchangeFeatures,
	(void(*)) virtio_AllocateQueues,
	(void(*)) virtio_InitQueues,
	(void(*)) virtio_FreeQueues,
	(void(*)) virtio_HostSupports,
	(void(*)) virtio_GuestSupports,

	(APTR) ((UINT32)-1)
};

struct VirtioBase *virtio_InitLib(struct VirtioBase *VirtioBase, UINT32 *segList, struct SysBase *SysBase)
{
	VirtioBase->SysBase = SysBase;


	return VirtioBase;
}

static const struct VirtioBase VirtioData =
{
	.Library.lib_Node.ln_Name = (APTR)&VirtioName[0],
	.Library.lib_Node.ln_Type = NT_LIBRARY,
	.Library.lib_Node.ln_Pri = -50,

	.Library.lib_OpenCnt = 0,
	.Library.lib_Flags = LIBF_SUMUSED|LIBF_CHANGED,
	.Library.lib_NegSize = 0,
	.Library.lib_PosSize = 0,
	.Library.lib_Version = VIRTIO_VERSION,
	.Library.lib_Revision = VIRTIO_REVISION,
	.Library.lib_Sum = 0,
	.Library.lib_IDString = (APTR)&VirtioVer[7],

	//more (specific to library)

};

//Init table
struct InitTable
{
	UINT32	LibBaseSize;
	APTR	FunctionTable;
	APTR	DataTable;
	APTR	InitFunction;
} virtio_InitTab =
{
	sizeof(struct VirtioBase),
	virtio_FuncTab,
	(APTR)&VirtioData,
	virtio_InitLib
};

static APTR VirtioEndResident;

// Resident ROMTAG
struct Resident VirtioRomTag =
{
	RTC_MATCHWORD,
	&VirtioRomTag,
	&VirtioEndResident,
	RTF_AUTOINIT | RTF_SINGLETASK,
	VIRTIO_VERSION,
	NT_LIBRARY,
	-50,
	VirtioName,
	VirtioVer,
	0,
	&virtio_InitTab
};

