#include "exec_funcs.h"
#include "virtio_blk_internal.h"
#include "expansion_funcs.h"
#include "virtio.h"
#include "arch_config.h"

#define SysBase VirtioBlkBase->VirtioBlk_SysBase

//Call this function atomically
void VirtioBlk_queue_command(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq)
{
	struct Unit	*unit = ioreq->io_Unit;

	if (ioreq->io_Error == 0)
	{
		SET_BITS(ioreq->io_Flags, IOF_QUEUED);
		DPrintF("VirtioBlk_queue_command: PutMsg %x to port %x\n", &(ioreq->io_Message), &(unit->unit_MsgPort));
		PutMsg(&(unit->unit_MsgPort), &(ioreq->io_Message));
	}

	return;
}

//Call this function atomically
void VirtioBlk_end_command(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq, INT8 error)
{
	ioreq->io_Error = error;
	return;
}

//Call this function atomically
void VirtioBlk_process_request(VirtioBlkBase *VirtioBlkBase, UINT32 unit_num)
{
	struct  Unit *unit;
	struct IOStdReq *curr_req;
	struct VirtioBlkUnit *vbu;

	unit = (struct  Unit *)&((VirtioBlkBase->VirtioBlkUnit[unit_num]).vb_unit);
	vbu = &(VirtioBlkBase->VirtioBlkUnit[unit_num]);

	struct CacheBase *CacheBase = vbu->CacheBase;

	curr_req = (struct IOStdReq *)GetHead(&(unit->unit_MsgPort.mp_MsgList));
	DPrintF("VirtioBlk_process_request curr_req = %x\n", curr_req);
	if (curr_req != NULL)
	{
		if(TEST_BITS(curr_req->io_Flags, IOF_CURRENT))
		{
			if(curr_req->io_Command == CMD_READ)
			{
				CacheRead(curr_req->io_Offset, curr_req->io_Length, curr_req->io_Data);
				DPrintF("VirtioBlk_process_request: READ: One request complete\n");
			}
			else if (curr_req->io_Command == CMD_WRITE)
			{
				CacheWrite(curr_req->io_Offset, curr_req->io_Length, curr_req->io_Data);
				DPrintF("VirtioBlk_process_request: WRITE: One request complete\n");
			}
			else if (curr_req->io_Command == CMD_UPDATE)
			{
				CacheSync();
				DPrintF("VirtioBlk_process_request: UPDATE: One request complete\n");
			}
		}
	}

	return;
}


int VirtioBlk_setup(VirtioBlkBase *VirtioBlkBase, VirtioBlk *vb, INT32 unit_num)
{
	struct ExpansionBase *ExpansionBase = VirtioBlkBase->ExpansionBase;

	VirtioDevice* vd = &(vb->vd);

	if (!PCIFindDeviceByUnit(VIRTIO_VENDOR_ID, VIRTIO_BLK_DEVICE_ID, &(vd->pciAddr), unit_num)) {
		DPrintF("VirtioBlk_setup: No Virtio device found.\n");
		return 0;
	}
	else
	{
		DPrintF("VirtioBlk_setup: Virtio block device found.\n");
	}

	DPrintF("VirtioBlk_setup: (vd->pciAddr).bus %x\n", (vd->pciAddr).bus);
	DPrintF("VirtioBlk_setup: (vd->pciAddr).device %x\n", (vd->pciAddr).device);
	DPrintF("VirtioBlk_setup: (vd->pciAddr).function %x\n", (vd->pciAddr).function);

	PCISetMemEnable(&vd->pciAddr, TRUE);
	vd->io_addr = PCIGetBARAddr(&vd->pciAddr, 0);
	DPrintF("VirtioBlk_setup: ioAddress %x\n", vd->io_addr);

	vd->intLine = PCIGetIntrLine(&vd->pciAddr);
	DPrintF("VirtioBlk_setup: intLine %x\n", vd->intLine);

	vd->intPin = PCIGetIntrPin(&vd->pciAddr);
	DPrintF("VirtioBlk_setup: intPin %x\n", vd->intPin);

	return 1;
}

int VirtioBlk_alloc_phys_requests(VirtioBlkBase *VirtioBlkBase, VirtioBlk *vb)
{
	/* Allocate memory for request headers and status field */

	vb->hdrs = AllocVec(VIRTIO_BLK_NUM_QUEUES * sizeof(vb->hdrs[0]),
				MEMF_FAST|MEMF_CLEAR);

	if (vb->hdrs == NULL)
	{
		DPrintF("Couldn't allocate memory for vb->hdrs\n");
		return 0;
	}

	vb->status = AllocVec(VIRTIO_BLK_NUM_QUEUES * sizeof(vb->status[0]),
				  MEMF_FAST|MEMF_CLEAR);

	if (vb->status == NULL)
	{
		FreeVec(vb->hdrs);
		DPrintF("Couldn't allocate memory for vb->status\n");
		return 0;
	}

	return 1;
}

int VirtioBlk_getDiskPresence(VirtioBlkBase *VirtioBlkBase, VirtioBlk *vb)
{
	DPrintF("Disk present\n");
	return VBF_DISK_IN;
}

int VirtioBlk_getWriteProtection(VirtioBlkBase *VirtioBlkBase, VirtioBlk *vb)
{
	DPrintF("Disk not write protected\n");
	return VBF_NOT_WRITE_PROTECTED;
}

int VirtioBlk_configuration(VirtioBlkBase *VirtioBlkBase, VirtioBlk *vb)
{
	struct VirtioBase* VirtioBase = VirtioBlkBase->VirtioBase;
	VirtioDevice* vd = &(vb->vd);

	UINT32 sectors_low, sectors_high, size_mbs;

	/* capacity is always there */
	sectors_low = VirtioRead32(vd->io_addr, VIRTIO_DEV_SPECIFIC_OFFSET+0);
	sectors_high = VirtioRead32(vd->io_addr, VIRTIO_DEV_SPECIFIC_OFFSET+4);
	vb->Info.capacity = ((UINT64)sectors_high << 32) | sectors_low;

	/* If this gets truncated, you have a big disk... */
	size_mbs = (UINT32)(vb->Info.capacity * 512 / 1024 / 1024);
	DPrintF("Capacity: %d MB\n", size_mbs);

	if (VirtioHostSupports(vd, VIRTIO_BLK_F_GEOMETRY))
	{
		vb->Info.geometry.cylinders = VirtioRead16(vd->io_addr, VIRTIO_DEV_SPECIFIC_OFFSET+16);
		vb->Info.geometry.heads = VirtioRead8(vd->io_addr, VIRTIO_DEV_SPECIFIC_OFFSET+18);
		vb->Info.geometry.sectors = VirtioRead8(vd->io_addr, VIRTIO_DEV_SPECIFIC_OFFSET+19);

		DPrintF("Geometry: cyl=%d heads=%d sectors=%d\n",
					vb->Info.geometry.cylinders,
					vb->Info.geometry.heads,
					vb->Info.geometry.sectors);
	}

	if (VirtioHostSupports(vd, VIRTIO_BLK_F_BLK_SIZE))
	{
		vb->Info.blk_size = VirtioRead32(vd->io_addr, VIRTIO_DEV_SPECIFIC_OFFSET+20);
		DPrintF("Block Size: %d\n", vb->Info.blk_size);
	}

	return 1;
}

void VirtioBlk_transfer(VirtioBlkBase *VirtioBlkBase, VirtioBlk* vb, UINT32 sector_start, UINT32 num_sectors, UINT8 write, UINT8* buf)
{
	DPrintF("VirtioBlk_transfer: sector_start = %d\n", sector_start);
	DPrintF("VirtioBlk_transfer: num_sectors = %d\n", num_sectors);
	DPrintF("VirtioBlk_transfer: write = %d\n", write);

	struct VirtioBase* VirtioBase = VirtioBlkBase->VirtioBase;
	VirtioDevice* vd = &(vb->vd);

	//prepare first out_hdr, since we have only one we are using 0,
	//otherwise replace 0 by a variable
	memset(&(vb->hdrs[0]), 0, sizeof(vb->hdrs[0]));

	if(write == 1)
	{
		//for writing to disk
		vb->hdrs[0].type = VIRTIO_BLK_T_OUT;
	}
	else
	{
		//for reading from disk
		vb->hdrs[0].type = VIRTIO_BLK_T_IN;
	}

	//fill up sector
	vb->hdrs[0].ioprio = 0;
	vb->hdrs[0].sector = sector_start;

	//clear status
	vb->status[0] = 1; //0 means success, 1 means error, 2 means unsupported

	//DPrintF("\n\n\nsector = %d\n", sector_start);
	//DPrintF("idx = %d\n", (vd->queues[0]).vring.avail->idx);



	//fill into descriptor table
	(vd->queues[0]).vring.desc[0].addr = (UINT32)&(vb->hdrs[0]);
	(vd->queues[0]).vring.desc[0].len = sizeof(vb->hdrs[0]);
	(vd->queues[0]).vring.desc[0].flags = VRING_DESC_F_NEXT;
	(vd->queues[0]).vring.desc[0].next = 1;



	//fillup buffer info
	(vd->queues[0]).vring.desc[1].addr = (UINT32)&buf[0];
	(vd->queues[0]).vring.desc[1].len = 512*num_sectors;
	(vd->queues[0]).vring.desc[1].flags = VRING_DESC_F_NEXT;
	//for reading sector, say that, this buffer is empty and writable
	if(write == 0)
	{
		(vd->queues[0]).vring.desc[1].flags |= VRING_DESC_F_WRITE;
	}
	(vd->queues[0]).vring.desc[1].next = 2;



	//fill up status header
	(vd->queues[0]).vring.desc[2].addr = (UINT32)&(vb->status[0]);
	(vd->queues[0]).vring.desc[2].len = sizeof(vb->status[0]);
	(vd->queues[0]).vring.desc[2].flags = VRING_DESC_F_WRITE;
	(vd->queues[0]).vring.desc[2].next = 0;



	//fill in available ring
	(vd->queues[0]).vring.avail->flags = 0; //1 mean no interrupt needed, 0 means interrupt needed
	(vd->queues[0]).vring.avail->ring[0] = 0; // 0 is the head of above request descriptor chain
	(vd->queues[0]).vring.avail->idx = (vd->queues[0]).vring.avail->idx + 3; //next available descriptor

	//notify
	VirtioWrite16(vd->io_addr, VIRTIO_QNOTFIY_OFFSET, 0); //notify that 1st queue (0) of this device has been updated

}

void Read(struct VirtioBlkTaskData *UserData, UINT32 sector_start, UINT32 num_sectors, UINT8* buf)
{
	VirtioBlkBase* VirtioBlkBase = UserData->VirtioBlkBase;
	UINT32 unit_num = UserData->unitNum;
	DPrintF("Read callback: unit_num = %d\n", unit_num);

	struct VirtioBlkUnit *vbu;
	vbu = &(VirtioBlkBase->VirtioBlkUnit[unit_num]);

	VirtioBlk_transfer(VirtioBlkBase, &(vbu->vb), sector_start, num_sectors, VB_READ, (UINT8 *)buf);

	DPrintF("Read callback: wait for track read irq\n");
	Wait(1 << (VirtioBlkBase->VirtioBlkUnit[unit_num].taskWakeupSignal));
}

void Write(struct VirtioBlkTaskData *UserData, UINT32 sector_start, UINT32 num_sectors, UINT8* buf)
{
	VirtioBlkBase* VirtioBlkBase = UserData->VirtioBlkBase;
	UINT32 unit_num = UserData->unitNum;

	struct VirtioBlkUnit *vbu;
	vbu = &(VirtioBlkBase->VirtioBlkUnit[unit_num]);

	VirtioBlk_transfer(VirtioBlkBase, &(vbu->vb), sector_start, num_sectors, VB_WRITE, (UINT8 *)buf);

	DPrintF("Write callback: wait for track write irq\n");
	Wait(1 << (VirtioBlkBase->VirtioBlkUnit[unit_num].taskWakeupSignal));
}
