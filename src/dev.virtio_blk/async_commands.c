#include "exec_funcs.h"
#include "virtio_blk_internal.h"

#define SysBase VirtioBlkBase->VirtioBlk_SysBase

void VirtioBlkInvalid(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq)
{
	DPrintF("Inside VirtioBlkInvalid!\n");
	VirtioBlk_end_command(VirtioBlkBase, IOERR_NOCMD, ioreq);
	return;
}


void VirtioBlkStart(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq)
{
	DPrintF("Inside VirtioBlkStart!\n");
	VirtioBlk_end_command(VirtioBlkBase, 0, ioreq);
	return;
}


void VirtioBlkStop(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq)
{
	DPrintF("Inside VirtioBlkStop!\n");
	VirtioBlk_end_command(VirtioBlkBase, 0, ioreq);
	return;
}


void VirtioBlkRead(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq)
{
	DPrintF("Inside VirtioBlkRead!\n");
	VirtioBlk *vb = &(((struct VirtioBlkUnit*)ioreq->io_Unit)->vb);
	UINT32 ipl;
	ipl = Disable();
	if((((struct VirtioBlkRequest*)ioreq)->sector_start +
	((struct VirtioBlkRequest*)ioreq)->num_sectors - 1)
	>=
	(vb->Info.geometry.cylinders *
	vb->Info.geometry.heads *
	(vb->Info.geometry.sectors + 1)))
	{
		Enable(ipl);
		//should set a valid error status rather than 0
		VirtioBlk_end_command(VirtioBlkBase, 0, ioreq);
	}
	else
	{
		//set that this is a read request
		((struct VirtioBlkRequest*)ioreq)->write = 0;

		// Ok, we add this to the list
		VirtioBlk_queue_command(VirtioBlkBase, ioreq);
		Enable(ipl);
		CLEAR_BITS(((struct VirtioBlkRequest*)ioreq)->node.io_Flags, IOF_QUICK);
	}
	return;
}


void VirtioBlkWrite(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq)
{
	DPrintF("Inside VirtioBlkWrite!\n");
	VirtioBlk *vb = &(((struct VirtioBlkUnit*)ioreq->io_Unit)->vb);
	UINT32 ipl;
	ipl = Disable();
	if((((struct VirtioBlkRequest*)ioreq)->sector_start +
	((struct VirtioBlkRequest*)ioreq)->num_sectors - 1)
	>=
	(vb->Info.geometry.cylinders *
	vb->Info.geometry.heads *
	(vb->Info.geometry.sectors + 1)))
	{
		Enable(ipl);
		//should set a valid error status rather than 0
		VirtioBlk_end_command(VirtioBlkBase, 0, ioreq);
	}
	else
	{
		//set that this is a write request
		((struct VirtioBlkRequest*)ioreq)->write = 1;

		// Ok, we add this to the list
		VirtioBlk_queue_command(VirtioBlkBase, ioreq);
		Enable(ipl);
		CLEAR_BITS(((struct VirtioBlkRequest*)ioreq)->node.io_Flags, IOF_QUICK);
	}
	return;
}

void VirtioBlkGetDeviceInfo(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq)
{
	VirtioBlk *vb = &(((struct VirtioBlkUnit*)ioreq->io_Unit)->vb);
	DPrintF("Inside VirtioBlkGetDeviceInfo!\n");
	UINT32 ipl = Disable();
	((struct VirtioBlkRequest*)ioreq)->info = vb->Info;
	Enable(ipl);

	VirtioBlk_end_command(VirtioBlkBase, 0, (struct IOStdReq *)ioreq);
	return;
}


void (*VirtioBlkCmdVector[])(VirtioBlkBase *, struct IOStdReq * ) =
{
	//standard
	VirtioBlkInvalid, //VirtioBlkInvalid
	0, //VirtioBlkReset
	VirtioBlkRead, //VirtioBlkRead
	VirtioBlkWrite, //VirtioBlkWrite
	0, //VirtioBlkUpdate
	0, //VirtioBlkClear
	VirtioBlkStop, //VirtioBlkStop
	VirtioBlkStart, //VirtioBlkStart
	0, //VirtioBlkFlush

	//non standard
	VirtioBlkGetDeviceInfo //VirtioBlkGetDeviceInfo
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

