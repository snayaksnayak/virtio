#include "exec_funcs.h"
#include "virtio_blk_internal.h"
#include "expansion_funcs.h"
#include "lib_virtio.h"
#include "arch_config.h"

#define SysBase VirtioBlkBase->VirtioBlk_SysBase


void VirtioBlk_queue_command(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq)
{
	UINT32 ipl = Disable();

	struct Unit	*unit = ioreq->io_Unit;

	if (ioreq->io_Error == 0)
	{
		PutMsg(&unit->unit_MsgPort, &ioreq->io_Message);
		//if it is the head request
		if (GetHead(&unit->unit_MsgPort.mp_MsgList) == &ioreq->io_Message.mn_Node)
		{
			//start processing
			VirtioBlk_process_request(VirtioBlkBase, ioreq);
		}
	}
	Enable(ipl);
	return;
}


void VirtioBlk_end_command(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq, UINT32 error)
{
	ioreq->io_Error = error;
	return;
}


void VirtioBlk_process_request(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq)
{
	UINT32 ipl = Disable();
	//collect unit from request structure
	struct Unit	*unit = ioreq->io_Unit;
	//collect head request from unit's request queue
	struct VirtioBlkRequest *head_req = (struct VirtioBlkRequest *)GetHead(&unit->unit_MsgPort.mp_MsgList);

	VirtioBlk *vb = &(((struct VirtioBlkUnit*)ioreq->io_Unit)->vb);
	UINT32 sector_start = head_req->sector_start;
	UINT32 num_sectors = head_req->num_sectors;
	UINT8 write = head_req->write;
	UINT8 *buf = head_req->buf;
	VirtioBlk_transfer(VirtioBlkBase, vb, sector_start, num_sectors, write, buf);
	Enable(ipl);
	return;
}


int VirtioBlk_setup(VirtioBlkBase *VirtioBlkBase, VirtioBlk *vb, INT32 unit_num)
{
	struct ExpansionBase *ExpansionBase = VirtioBlkBase->ExpansionBase;

	VirtioDevice* vd = &(vb->vd);

	if (!PCIFindDeviceByUnit(VIRTIO_VENDOR_ID, VIRTIO_BLK_DEVICE_ID, &(vd->pciAddr), unit_num)) {
		DPrintF("VirtioBlk_setup: No Virtio device found.");
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



int VirtioBlk_configuration(VirtioBlkBase *VirtioBlkBase, VirtioBlk *vb)
{
	struct LibVirtioBase* LibVirtioBase = VirtioBlkBase->LibVirtioBase;
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

void test_mhz_delay(APTR, int);
void VirtioBlk_transfer(VirtioBlkBase *VirtioBlkBase, VirtioBlk* vb, UINT32 sector_start, UINT32 num_sectors, UINT8 write, UINT8* buf)
{
	struct LibVirtioBase* LibVirtioBase = VirtioBlkBase->LibVirtioBase;
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

	DPrintF("\n\n\nsector = %d\n", sector_start);
	DPrintF("idx = %d\n", (vd->queues[0]).vring.avail->idx);



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

/*
	//give some delay
	test_mhz_delay(SysBase, 1);

	DPrintF("vb->status[0] %d\n", vb->status[0]);


	int j=0;

	DPrintF("(vd->queues[0]).vring.used->flags %d\n", (vd->queues[0]).vring.used->flags);
	DPrintF("(vd->queues[0]).vring.used->idx %d\n", (vd->queues[0]).vring.used->idx);
	for(j=0; j<(vd->queues[0]).num;j++)
	{
		DPrintF("(vd->queues[0]).vring.used->ring[%d].id %d\n", j, (vd->queues[0]).vring.used->ring[j].id);
		DPrintF("(vd->queues[0]).vring.used->ring[%d].len %d\n", j, (vd->queues[0]).vring.used->ring[j].len);
	}

	DPrintF("(vd->queues[0]).vring.avail->flags %d\n", (vd->queues[0]).vring.avail->flags);
	DPrintF("(vd->queues[0]).vring.avail->idx %d\n", (vd->queues[0]).vring.avail->idx);
	for(j=0; j<(vd->queues[0]).num;j++)
	{
		DPrintF("(vd->queues[0]).vring.avail->ring[%d] %d\n", j, (vd->queues[0]).vring.avail->ring[j]);
	}


	//See if virtio device generated an interrupt(1) or not(0)
	UINT8 isr;
	isr=VirtioRead8(vd->io_addr, VIRTIO_ISR_STATUS_OFFSET);
	DPrintF("virtio_blk_transfer: isr= %d\n", isr);

	DPrintF("virtio_blk_transfer: buf[0]= %x\n", buf[0]);
	DPrintF("virtio_blk_transfer: buf[1]= %x\n", buf[1]);
	DPrintF("virtio_blk_transfer: buf[2]= %x\n", buf[2]);
	DPrintF("virtio_blk_transfer: buf[3]= %x\n", buf[3]);
*/
}
