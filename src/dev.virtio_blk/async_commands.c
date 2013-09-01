#include "exec_funcs.h"
#include "virtio_blk_internal.h"
#include "device_error.h"
#include "arch_config.h"

#define SysBase VirtioBlkBase->VirtioBlk_SysBase

void VirtioBlkInvalid(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq)
{
	DPrintF("Inside VirtioBlkInvalid!\n");
	VirtioBlk_end_command(VirtioBlkBase, ioreq, IOERR_NOCMD);
	return;
}


void VirtioBlkStart(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq)
{
	DPrintF("Inside VirtioBlkStart!\n");
	VirtioBlk_end_command(VirtioBlkBase, ioreq, 0);
	return;
}


void VirtioBlkStop(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq)
{
	DPrintF("Inside VirtioBlkStop!\n");
	VirtioBlk_end_command(VirtioBlkBase, ioreq, 0);
	return;
}


void VirtioBlkRead(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq)
{
	//DPrintF("Inside VirtioBlkRead!\n");
	VirtioBlk *vb = &(((struct VirtioBlkUnit*)ioreq->io_Unit)->vb);
	struct VirtioBlkUnit *vbu = (struct VirtioBlkUnit*)ioreq->io_Unit;

	UINT32 track_offset = ioreq->io_Offset / (vb->Info.geometry.sectors + 1); // divide by 64 (1 track = 64 sectors)
	UINT32 sector_offset = ioreq->io_Offset % (vb->Info.geometry.sectors + 1);
	UINT32 sectors_to_copy = ioreq->io_Length / (vb->Info.blk_size); // divide by 512

	DPrintF("VirtioBlkRead: track_offset = %d\n", track_offset);
	DPrintF("VirtioBlkRead: sector_offset = %d\n", sector_offset);
	DPrintF("VirtioBlkRead: sectors_to_copy = %d\n", sectors_to_copy);
	DPrintF("VirtioBlkRead: (vb->Info.geometry.sectors + 1) = %d\n", (vb->Info.geometry.sectors + 1));
	DPrintF("VirtioBlkRead: (vb->Info.blk_size) = %d\n", (vb->Info.blk_size));

	UINT32 ipl;
	ipl = Disable();
	if((ioreq->io_Offset +
	sectors_to_copy - 1)
	>=
	(vb->Info.geometry.cylinders *
	vb->Info.geometry.heads *
	(vb->Info.geometry.sectors + 1)))
	{
		VirtioBlk_end_command(VirtioBlkBase, ioreq, BLK_ERR_NotEnoughSectors );
	}
	else if(vbu->DiskPresence == VBF_NO_DISK)
	{
		VirtioBlk_end_command(VirtioBlkBase, ioreq, BLK_ERR_DiskNotFound );
	}
	else if((vbu->CacheFlag == VBF_CLEAN)
	&& (track_offset == vbu->TrackNum)
	&& (sectors_to_copy <= ((vb->Info.geometry.sectors + 1)-(sector_offset))))
	{
		memcpy(ioreq->io_Data + ioreq->io_Actual, vbu->TrackCache , sectors_to_copy * (vb->Info.blk_size));
		ioreq->io_Actual = sectors_to_copy * (vb->Info.blk_size);
		DPrintF("VirtioBlkRead: quick service, ioreq->io_Actual = %d\n", ioreq->io_Actual);
		VirtioBlk_end_command(VirtioBlkBase, ioreq, 0 );
	}
	else
	{
		ioreq->io_Actual = 0;
		// Ok, we add this to the list
		VirtioBlk_queue_command(VirtioBlkBase, ioreq);
		CLEAR_BITS(ioreq->io_Flags, IOF_QUICK);
		DPrintF("VirtioBlkRead: late service\n");
	}

	Enable(ipl);
	return;
}


void VirtioBlkWrite(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq)
{
	DPrintF("Inside VirtioBlkWrite!\n");
	VirtioBlk *vb = &(((struct VirtioBlkUnit*)ioreq->io_Unit)->vb);
	UINT32 ipl;
	ipl = Disable();
	if((ioreq->io_Offset +
	ioreq->io_Length - 1)
	>=
	(vb->Info.geometry.cylinders *
	vb->Info.geometry.heads *
	(vb->Info.geometry.sectors + 1)))
	{
		VirtioBlk_end_command(VirtioBlkBase, ioreq, BLK_ERR_NotEnoughSectors);
	}
	else
	{
		// Ok, we add this to the list
		VirtioBlk_queue_command(VirtioBlkBase, ioreq);
		CLEAR_BITS(ioreq->io_Flags, IOF_QUICK);
	}

	Enable(ipl);
	return;
}

void VirtioBlkGetDeviceInfo(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq)
{
	VirtioBlk *vb = &(((struct VirtioBlkUnit*)ioreq->io_Unit)->vb);
	DPrintF("Inside VirtioBlkGetDeviceInfo!\n");
	UINT32 ipl = Disable();
	*((struct VirtioBlkDeviceInfo*)(ioreq->io_Data)) = vb->Info;
	VirtioBlk_end_command(VirtioBlkBase, (struct IOStdReq *)ioreq, 0);

	Enable(ipl);
	return;
}

void VirtioBlkClear(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq)
{
	struct VirtioBlkUnit *vbu = (struct VirtioBlkUnit*)ioreq->io_Unit;
	DPrintF("Inside VirtioBlkClear!\n");
	UINT32 ipl = Disable();
	vbu->CacheFlag = VBF_INVALID;
	VirtioBlk_end_command(VirtioBlkBase, (struct IOStdReq *)ioreq, 0);

	Enable(ipl);
	return;
}

void VirtioBlkUpdate(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq)
{
	/*
	VirtioBlk *vb = &(((struct VirtioBlkUnit*)ioreq->io_Unit)->vb);
	DPrintF("Inside VirtioBlkGetDeviceInfo!\n");
	UINT32 ipl = Disable();
	*((struct VirtioBlkDeviceInfo*)(ioreq->io_Data)) = vb->Info;
	VirtioBlk_end_command(VirtioBlkBase, (struct IOStdReq *)ioreq, 0);

	Enable(ipl);
	return;
	*/
}

void VirtioBlkGetDiskChangeCount(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq)
{
	struct VirtioBlkUnit *vbu = (struct VirtioBlkUnit*)ioreq->io_Unit;
	DPrintF("Inside VirtioBlkGetDiskChangeCount!\n");
	UINT32 ipl = Disable();
	ioreq->io_Actual = vbu->DiskChangeCounter;
	VirtioBlk_end_command(VirtioBlkBase, (struct IOStdReq *)ioreq, 0);

	Enable(ipl);
	return;
}

void VirtioBlkGetDiskPresenceStatus(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq)
{
	struct VirtioBlkUnit *vbu = (struct VirtioBlkUnit*)ioreq->io_Unit;
	VirtioBlk *vb = &(((struct VirtioBlkUnit*)ioreq->io_Unit)->vb);
	DPrintF("Inside VirtioBlkGetDiskPresenceStatus!\n");
	UINT32 ipl = Disable();
	vbu->DiskPresence = VirtioBlk_getDiskPresence(VirtioBlkBase, vb);
	ioreq->io_Actual = vbu->DiskPresence;
	VirtioBlk_end_command(VirtioBlkBase, (struct IOStdReq *)ioreq, 0);

	Enable(ipl);
	return;
}

void VirtioBlkEject(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq)
{
	struct VirtioBlkUnit *vbu = (struct VirtioBlkUnit*)ioreq->io_Unit;
	DPrintF("Inside VirtioBlkEject!\n");
	UINT32 ipl = Disable();
	vbu->DiskChangeCounter++;
	VirtioBlk_end_command(VirtioBlkBase, (struct IOStdReq *)ioreq, 0);

	Enable(ipl);
	return;
}

void VirtioBlkFormat(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq)
{
	/*
	VirtioBlk *vb = &(((struct VirtioBlkUnit*)ioreq->io_Unit)->vb);
	DPrintF("Inside VirtioBlkGetDeviceInfo!\n");
	UINT32 ipl = Disable();
	*((struct VirtioBlkDeviceInfo*)(ioreq->io_Data)) = vb->Info;
	VirtioBlk_end_command(VirtioBlkBase, (struct IOStdReq *)ioreq, 0);

	Enable(ipl);
	return;
	*/
}

void (*VirtioBlkCmdVector[])(VirtioBlkBase *, struct IOStdReq * ) =
{
	//standard
	VirtioBlkInvalid, //VirtioBlkInvalid
	VirtioBlkInvalid, //VirtioBlkReset
	VirtioBlkRead, //VirtioBlkRead
	VirtioBlkWrite, //VirtioBlkWrite
	VirtioBlkUpdate, //VirtioBlkUpdate
	VirtioBlkClear, //VirtioBlkClear
	VirtioBlkStop, //VirtioBlkStop
	VirtioBlkStart, //VirtioBlkStart
	VirtioBlkInvalid, //VirtioBlkFlush

	//non standard
	VirtioBlkGetDeviceInfo,
	VirtioBlkGetDiskChangeCount,
	VirtioBlkGetDiskPresenceStatus,
	VirtioBlkEject,
	VirtioBlkFormat
};


/*
INT8 VirtioBlkCmdQuick[] =
// -1 : not queued
// 0  : queued
{
	-1, //VirtioBlkInvalid
	-1, //VirtioBlkReset
	0, //VirtioBlkRead
	0, //VirtioBlkWrite
	-1, //VirtioBlkUpdate
	-1, //VirtioBlkClear
	-1, //VirtioBlkStop
	-1, //VirtioBlkStart
	-1, //VirtioBlkFlush

	-1, //VirtioBlkGetDeviceInfo
};
*/

