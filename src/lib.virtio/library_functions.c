#include "exec_funcs.h"
#include "virtio_internal.h"


#define SysBase VirtioBase->SysBase

struct VirtioBase* virtio_OpenLib(struct VirtioBase *VirtioBase)
{
    VirtioBase->Library.lib_OpenCnt++;

	return(VirtioBase);
}

APTR virtio_CloseLib(struct VirtioBase *VirtioBase)
{
	VirtioBase->Library.lib_OpenCnt--;

	return (VirtioBase);
}

APTR virtio_ExpungeLib(struct VirtioBase *VirtioBase)
{
	return (NULL);
}

APTR virtio_ExtFuncLib(void)
{
	return (NULL);
}


//**************


void virtio_Write8(struct VirtioBase *VirtioBase, UINT16 base, UINT16 offset, UINT8 val)
{
	IO_Out8(base+offset, val);
}
void virtio_Write16(struct VirtioBase *VirtioBase, UINT16 base, UINT16 offset, UINT16 val)
{
	IO_Out16(base+offset, val);
}
void virtio_Write32(struct VirtioBase *VirtioBase, UINT16 base, UINT16 offset, UINT32 val)
{
	IO_Out32(base+offset, val);
}

UINT8 virtio_Read8(struct VirtioBase *VirtioBase, UINT16 base, UINT16 offset)
{
	return IO_In8(base+offset);
}
UINT16 virtio_Read16(struct VirtioBase *VirtioBase, UINT16 base, UINT16 offset)
{
	return IO_In16(base+offset);
}
UINT32 virtio_Read32(struct VirtioBase *VirtioBase, UINT16 base, UINT16 offset)
{
	return IO_In32(base+offset);
}


//******************


void virtio_ExchangeFeatures(VirtioBase *VirtioBase, VirtioDevice *vd)
{
	UINT32 guest_features = 0, host_features = 0;
	virtio_feature *f;

	//collect host features
	host_features = VirtioRead32(vd->io_addr, VIRTIO_HOST_F_OFFSET);

	for (int i = 0; i < vd->num_features; i++)
	{
		f = &vd->features[i];

		// prepare the features the guest/driver supports
		guest_features |= (f->guest_support << f->bit);
		DPrintF("guest feature %d\n", (f->guest_support << f->bit));

		// just load the host/device feature int the struct
		f->host_support |=  ((host_features >> f->bit) & 1);
		DPrintF("host feature %d\n\n", ((host_features >> f->bit) & 1));
	}

	// let the device know about our features
	VirtioWrite32(vd->io_addr, VIRTIO_GUEST_F_OFFSET, guest_features);
}


//*************

int virtio_AllocateQueues(VirtioBase *VirtioBase, VirtioDevice *vd, INT32 num_queues)
{
	int r = 1;

	// Assume there's no device with more than 256 queues
	if (num_queues < 0 || num_queues > 256)
	{
		DPrintF("Invalid num_queues!\n");
		return 0;
	}

	vd->num_queues = num_queues;

	// allocate queue memory
	vd->queues = AllocVec(num_queues * sizeof(vd->queues[0]), MEMF_FAST|MEMF_CLEAR);

	if (vd->queues == NULL)
	{
		DPrintF("Couldn't allocate memory for %d queues\n", vd->num_queues);
		return 0;
	}

	//not needed because of MEMF_CLEAR
	memset(vd->queues, 0, num_queues * sizeof(vd->queues[0]));

	return r;
}


int virtio_InitQueues(VirtioBase *VirtioBase, VirtioDevice *vd)
{
	//Initialize all queues
	int i, j, r;
	struct virtio_queue *q;

	for (i = 0; i < vd->num_queues; i++)
	{
		q = &vd->queues[i];

		//select the queue
		VirtioWrite16(vd->io_addr, VIRTIO_QSEL_OFFSET, i);
		q->num = VirtioRead16(vd->io_addr, VIRTIO_QSIZE_OFFSET);
		DPrintF("Queue %d, q->num (%d)\n", i, q->num);
		if (q->num & (q->num - 1)) {
			DPrintF("Queue %d num=%d not ^2\n", i, q->num);
			r = 0;
			goto free;
		}

		r = Virtio_alloc_phys_queue(VirtioBase,q);

		if (r != 1)
		{
			DPrintF("Couldn't allocate queue number %d\n", i);
			goto free;
		}

		Virtio_init_phys_queue(VirtioBase, q);

		//Let the host know about the guest physical page
		VirtioWrite32(vd->io_addr, VIRTIO_QADDR_OFFSET, q->page);
	}

	return 1;

free:
	for (j = 0; j < i; j++)
	{
		Virtio_free_phys_queue(VirtioBase, &vd->queues[i]);
	}

	return r;
}



void virtio_FreeQueues(VirtioBase *VirtioBase, VirtioDevice *vd)
{
	int i;
	for (i = 0; i < vd->num_queues; i++)
	{
		Virtio_free_phys_queue(VirtioBase, &vd->queues[i]);
	}

	FreeVec(vd->queues);
	vd->queues = 0;
}


int virtio_HostSupports(VirtioBase *VirtioBase, VirtioDevice *vd, int bit)
{
	return Virtio_supports(VirtioBase, vd, bit, 1);
}

int virtio_GuestSupports(VirtioBase *VirtioBase, VirtioDevice *vd, int bit)
{
	return Virtio_supports(VirtioBase, vd, bit, 0);
}


