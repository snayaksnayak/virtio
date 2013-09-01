#include "virtio_blk_internal.h"
#include "sysbase.h"
#include "resident.h"
#include "exec_funcs.h"
#include "expansion_funcs.h"
#include "lib_virtio.h"
#include "arch_config.h"



char DevName[] = "virtio_blk.device";
char Version[] = "\0$VER: virtio_blk.device 0.1 ("__DATE__")\r\n";

char* TaskName[VB_UNIT_MAX]={
	"virtio_blk.task0", //For unit 0
	"virtio_blk.task1", //For unit 1
	"virtio_blk.task2", //For unit 2
	"virtio_blk.task3"  //For unit 3
};

#define MAX_VIRTIO_BLK_FEATURES 10

static virtio_feature blkf[MAX_VIRTIO_BLK_FEATURES] = {
	{ "barrier",	VIRTIO_BLK_F_BARRIER,	0,	0 	},
	{ "sizemax",	VIRTIO_BLK_F_SIZE_MAX,	0,	0	},
	{ "segmax",		VIRTIO_BLK_F_SEG_MAX,	0,	0	},
	{ "geometry",	VIRTIO_BLK_F_GEOMETRY,	0,	0	},
	{ "read-only",	VIRTIO_BLK_F_RO,		0,	0	},
	{ "blocksize",	VIRTIO_BLK_F_BLK_SIZE,	0,	0	},
	{ "scsi",		VIRTIO_BLK_F_SCSI,		0,	0	},
	{ "flush",		VIRTIO_BLK_F_FLUSH,		0,	0	},
	{ "topology",	VIRTIO_BLK_F_TOPOLOGY,	0,	0	},
	{ "idbytes",	VIRTIO_BLK_ID_BYTES,	0,	0	}
};

static virtio_feature blk_feature[VB_UNIT_MAX][MAX_VIRTIO_BLK_FEATURES];

APTR virtio_blk_FuncTab[] =
{
 (void(*)) virtio_blk_OpenDev,
 (void(*)) virtio_blk_CloseDev,
 (void(*)) virtio_blk_ExpungeDev,
 (void(*)) virtio_blk_ExtFuncDev,

 (void(*)) virtio_blk_BeginIO,
 (void(*)) virtio_blk_AbortIO,

 (APTR) ((UINT32)-1)
};

struct VirtioBlkBase *virtio_blk_InitDev(struct VirtioBlkBase *VirtioBlkBase, UINT32 *segList, struct SysBase *SysBase)
{
	VirtioBlkBase->VirtioBlk_SysBase = SysBase;
	VirtioBlkBase->NumAvailUnits = 0;
	VirtioBlkBase->VirtioBlk_BootTask = (struct Task *)FindTask(NULL);

	struct ExpansionBase *ExpansionBase = (struct ExpansionBase *)OpenLibrary("expansion.library", 0);
	VirtioBlkBase->ExpansionBase = ExpansionBase;
	if (VirtioBlkBase->ExpansionBase == NULL) {
		DPrintF("virtio_blk_InitDev: Cant open expansion.library\n");
		return NULL;
	}


	struct LibVirtioBase *LibVirtioBase = (struct LibVirtioBase *)OpenLibrary("lib_virtio.library", 0);
	VirtioBlkBase->LibVirtioBase = LibVirtioBase;
	if (VirtioBlkBase->LibVirtioBase == NULL) {
		DPrintF("virtio_blk_InitDev: Cant open lib_virtio.library\n");
		return NULL;
	}


	for(int unit_num = 0; unit_num < VB_UNIT_MAX ; unit_num++)
	{
		VirtioBlk *vb = &((VirtioBlkBase->VirtioBlkUnit[unit_num]).vb);
		VirtioDevice* vd = &(vb->vd);

		//1. setup the device.

		//setup
		int res = VirtioBlk_setup(VirtioBlkBase, vb, unit_num);

		if(res != 1)
			break;

		// Reset the device
		VirtioWrite8(vd->io_addr, VIRTIO_DEV_STATUS_OFFSET, VIRTIO_STATUS_RESET);

		// Ack the device
		VirtioWrite8(vd->io_addr, VIRTIO_DEV_STATUS_OFFSET, VIRTIO_STATUS_ACK);


		//2. check if you can drive the device.

		//driver supports these features
		vd->features = &blk_feature[unit_num][0];
		vd->num_features = MAX_VIRTIO_BLK_FEATURES;
		memcpy(vd->features, blkf, sizeof(blkf));

		//exchange features
		VirtioExchangeFeatures(vd);

		// We know how to drive the device...
		VirtioWrite8(vd->io_addr, VIRTIO_DEV_STATUS_OFFSET, VIRTIO_STATUS_DRV);


		//3. be ready to go.

		// virtio blk has only 1 queue
		VirtioAllocateQueues(vd, VIRTIO_BLK_NUM_QUEUES);

		//init queues
		VirtioInitQueues(vd);

		//Allocate memory for headers and status
		VirtioBlk_alloc_phys_requests(VirtioBlkBase, vb);

		//collect configuration
		VirtioBlk_configuration(VirtioBlkBase, vb);

		//Driver is ready to go!
		VirtioWrite8(vd->io_addr, VIRTIO_DEV_STATUS_OFFSET, VIRTIO_STATUS_DRV_OK);

		// start irq server after driver is ok
		VirtioBlkBase->VirtioBlkIntServer = CreateIntServer(DevName, VIRTIO_BLK_INT_PRI, VirtioBlkIRQServer, VirtioBlkBase);
		AddIntServer(vd->intLine, VirtioBlkBase->VirtioBlkIntServer);

		//initialize unit structure
		VirtioBlkBase->VirtioBlkUnit[unit_num].DiskPresence = VirtioBlk_getDiskPresence(VirtioBlkBase, vb);
		VirtioBlkBase->VirtioBlkUnit[unit_num].DiskChangeCounter = 0;

		VirtioBlkBase->VirtioBlkUnit[unit_num].TrackCache = AllocVec((vb->Info.geometry.sectors + 1) * vb->Info.blk_size, MEMF_FAST|MEMF_CLEAR);
		if(VirtioBlkBase->VirtioBlkUnit[unit_num].TrackCache == NULL)
		{
			break;
		}
		VirtioBlkBase->VirtioBlkUnit[unit_num].CacheFlag = VBF_INVALID;
		VirtioBlkBase->VirtioBlkUnit[unit_num].TrackNum = 0;

		//start worker task
		DPrintF("virtio_blk_InitDev: create a worker task and wait\n");
		SetSignal(0L, SIGF_SINGLE);
		VirtioBlkBase->VirtioBlkUnit[unit_num].VirtioBlk_WorkerTaskData = AllocVec(sizeof(struct VirtioBlkTaskData), MEMF_FAST|MEMF_CLEAR);
		if(VirtioBlkBase->VirtioBlkUnit[unit_num].VirtioBlk_WorkerTaskData == NULL)
		{
			FreeVec(VirtioBlkBase->VirtioBlkUnit[unit_num].TrackCache);
			break;
		}
		VirtioBlkBase->VirtioBlkUnit[unit_num].VirtioBlk_WorkerTaskData->VirtioBlkBase = VirtioBlkBase;
		VirtioBlkBase->VirtioBlkUnit[unit_num].VirtioBlk_WorkerTaskData->unitNum = unit_num;
		VirtioBlkBase->VirtioBlkUnit[unit_num].VirtioBlk_WorkerTask = TaskCreate(TaskName[unit_num], VirtioBlk_WorkerTaskFunction, VirtioBlkBase->VirtioBlkUnit[unit_num].VirtioBlk_WorkerTaskData, 8192, 20);
		Wait(SIGF_SINGLE);
		DPrintF("virtio_blk_InitDev: a worker task created, waiting finished\n");

		//save number of available units
		VirtioBlkBase->NumAvailUnits++;
	}

	return VirtioBlkBase;
}

static const struct VirtioBlkBase VirtioBlkDevData =
{
	.Device.dd_Library.lib_Node.ln_Name = (APTR)&DevName[0],
	.Device.dd_Library.lib_Node.ln_Type = NT_DEVICE,
	.Device.dd_Library.lib_Node.ln_Pri = 50,
	.Device.dd_Library.lib_OpenCnt = 0,
	.Device.dd_Library.lib_Flags = LIBF_SUMUSED|LIBF_CHANGED,
	.Device.dd_Library.lib_NegSize = 0,
	.Device.dd_Library.lib_PosSize = 0,
	.Device.dd_Library.lib_Version = VERSION,
	.Device.dd_Library.lib_Revision = REVISION,
	.Device.dd_Library.lib_Sum = 0,
	.Device.dd_Library.lib_IDString = (APTR)&Version[7],

};

//Init table
struct InitTable
{
	UINT32	LibBaseSize;
	APTR	FunctionTable;
	APTR	DataTable;
	APTR	InitFunction;
} virtio_blk_InitTab =
{
	sizeof(struct VirtioBlkBase),
	virtio_blk_FuncTab,
	(APTR)&VirtioBlkDevData,
	virtio_blk_InitDev
};

static APTR VirtioBlkEndResident;

// Resident ROMTAG
struct Resident VirtioBlkRomTag =
{
	RTC_MATCHWORD,
	&VirtioBlkRomTag,
	&VirtioBlkEndResident,
	RTF_AUTOINIT | RTF_COLDSTART,
	VERSION,
	NT_DEVICE,
	50,
	DevName,
	Version,
	0,
	&virtio_blk_InitTab
};

