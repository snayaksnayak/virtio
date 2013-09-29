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

	struct CacheBase *CacheBase = vbu->CacheBase;

	//set till-now-number-of-bytes-read is zero.
	ioreq->io_Actual = 0;

	UINT32 track_offset = ioreq->io_Offset / (vb->Info.geometry.sectors + 1); // divide by 64 (1 track = 64 sectors)
	UINT32 sector_offset = ioreq->io_Offset % (vb->Info.geometry.sectors + 1);
	UINT32 sectors_to_read = ioreq->io_Length / (vb->Info.blk_size); // divide by 512

	DPrintF("VirtioBlkRead: track_offset = %d\n", track_offset);
	DPrintF("VirtioBlkRead: sector_offset = %d\n", sector_offset);
	DPrintF("VirtioBlkRead: sectors_to_read = %d\n", sectors_to_read);

	Forbid();
	if((ioreq->io_Offset +
	sectors_to_read - 1)
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
	else if(CacheHit(ioreq->io_Offset, ioreq->io_Length))
	{
		ioreq->io_Actual = CacheRead(ioreq->io_Offset, ioreq->io_Length, ioreq->io_Data);
		DPrintF("VirtioBlkRead: quick service, ioreq->io_Actual = %d\n", ioreq->io_Actual);
		VirtioBlk_end_command(VirtioBlkBase, ioreq, 0 );
	}
	else
	{
		// Ok, we add this to the list
		VirtioBlk_queue_command(VirtioBlkBase, ioreq);
		CLEAR_BITS(ioreq->io_Flags, IOF_QUICK);
		DPrintF("VirtioBlkRead: late service\n");
	}
	Permit();

	return;
}


void VirtioBlkWrite(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq)
{
	DPrintF("Inside VirtioBlkWrite!\n");
	VirtioBlk *vb = &(((struct VirtioBlkUnit*)ioreq->io_Unit)->vb);
	struct VirtioBlkUnit *vbu = (struct VirtioBlkUnit*)ioreq->io_Unit;

	struct CacheBase *CacheBase = vbu->CacheBase;

	//set till-now-number-of-bytes-write is zero.
	ioreq->io_Actual = 0;

	UINT32 track_offset = ioreq->io_Offset / (vb->Info.geometry.sectors + 1); // divide by 64 (1 track = 64 sectors)
	UINT32 sector_offset = ioreq->io_Offset % (vb->Info.geometry.sectors + 1);
	UINT32 sectors_to_write = ioreq->io_Length / (vb->Info.blk_size); // divide by 512

	DPrintF("VirtioBlkWrite: track_offset = %d\n", track_offset);
	DPrintF("VirtioBlkWrite: sector_offset = %d\n", sector_offset);
	DPrintF("VirtioBlkWrite: sectors_to_write = %d\n", sectors_to_write);

	Forbid();
	if((ioreq->io_Offset +
	sectors_to_write - 1)
	>=
	(vb->Info.geometry.cylinders *
	vb->Info.geometry.heads *
	(vb->Info.geometry.sectors + 1)))
	{
		VirtioBlk_end_command(VirtioBlkBase, ioreq, BLK_ERR_NotEnoughSectors);
	}
	else if(vbu->DiskPresence == VBF_NO_DISK)
	{
		VirtioBlk_end_command(VirtioBlkBase, ioreq, BLK_ERR_DiskNotFound );
	}
	else if(CacheHit(ioreq->io_Offset, ioreq->io_Length))
	{
		ioreq->io_Actual = CacheWrite(ioreq->io_Offset, ioreq->io_Length, ioreq->io_Data);
		DPrintF("VirtioBlkWrite: quick service, ioreq->io_Actual = %d\n", ioreq->io_Actual);
		VirtioBlk_end_command(VirtioBlkBase, ioreq, 0 );
	}
	else
	{
		// Ok, we add this to the list
		VirtioBlk_queue_command(VirtioBlkBase, ioreq);
		CLEAR_BITS(ioreq->io_Flags, IOF_QUICK);
		DPrintF("VirtioBlkWrite: late service\n");
	}
	Permit();

	return;
}

void VirtioBlkGetDeviceInfo(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq)
{
	VirtioBlk *vb = &(((struct VirtioBlkUnit*)ioreq->io_Unit)->vb);
	DPrintF("Inside VirtioBlkGetDeviceInfo!\n");

	//forbid-permit is not needed here
	Forbid();
	*((struct VirtioBlkDeviceInfo*)(ioreq->io_Data)) = vb->Info;
	VirtioBlk_end_command(VirtioBlkBase, (struct IOStdReq *)ioreq, 0);
	Permit();

	return;
}

void VirtioBlkClear(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq)
{
	struct VirtioBlkUnit *vbu = (struct VirtioBlkUnit*)ioreq->io_Unit;
	struct CacheBase *CacheBase = vbu->CacheBase;

	DPrintF("Inside VirtioBlkClear!\n");

	Forbid();
	CacheDiscard();
	VirtioBlk_end_command(VirtioBlkBase, (struct IOStdReq *)ioreq, 0);
	Permit();

	return;
}

void VirtioBlkUpdate(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq)
{
	DPrintF("Inside VirtioBlkUpdate!\n");
	struct VirtioBlkUnit *vbu = (struct VirtioBlkUnit*)ioreq->io_Unit;
	struct CacheBase *CacheBase = vbu->CacheBase;


	Forbid();
	if(vbu->DiskPresence == VBF_NO_DISK)
	{
		VirtioBlk_end_command(VirtioBlkBase, ioreq, BLK_ERR_DiskNotFound );
	}
	else if(!CacheDirty())
	{
		DPrintF("VirtioBlkUpdate: quick service\n");
		VirtioBlk_end_command(VirtioBlkBase, ioreq, 0 );
	}
	else
	{
		// Ok, we add this to the list
		VirtioBlk_queue_command(VirtioBlkBase, ioreq);
		CLEAR_BITS(ioreq->io_Flags, IOF_QUICK);
		DPrintF("VirtioBlkUpdate: late service\n");
	}
	Permit();

	return;
}

void VirtioBlkGetDiskChangeCount(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq)
{
	struct VirtioBlkUnit *vbu = (struct VirtioBlkUnit*)ioreq->io_Unit;
	DPrintF("Inside VirtioBlkGetDiskChangeCount!\n");

	Forbid();
	ioreq->io_Actual = vbu->DiskChangeCounter;
	VirtioBlk_end_command(VirtioBlkBase, (struct IOStdReq *)ioreq, 0);
	Permit();

	return;
}

void VirtioBlkGetDiskPresenceStatus(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq)
{
	struct VirtioBlkUnit *vbu = (struct VirtioBlkUnit*)ioreq->io_Unit;
	VirtioBlk *vb = &(((struct VirtioBlkUnit*)ioreq->io_Unit)->vb);
	DPrintF("Inside VirtioBlkGetDiskPresenceStatus!\n");

	Forbid();
	vbu->DiskPresence = VirtioBlk_getDiskPresence(VirtioBlkBase, vb);
	ioreq->io_Actual = vbu->DiskPresence;
	VirtioBlk_end_command(VirtioBlkBase, (struct IOStdReq *)ioreq, 0);
	Permit();

	return;
}

void VirtioBlkEject(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq)
{
	struct VirtioBlkUnit *vbu = (struct VirtioBlkUnit*)ioreq->io_Unit;
	DPrintF("Inside VirtioBlkEject!\n");

	Forbid();
	vbu->DiskChangeCounter++;
	VirtioBlk_end_command(VirtioBlkBase, (struct IOStdReq *)ioreq, 0);
	Permit();

	return;
}

void VirtioBlkFormat(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq)
{
	//TODO: Right now there is no difference between WRITE and FORMAT
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
	//TODO: Right now there is no difference between WRITE and FORMAT
	VirtioBlkWrite //VirtioBlkFormat
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

