#include "types.h"
#include "sysbase.h"
#include "expansionbase.h"
#include "expansion_funcs.h"
#include "exec_funcs.h"
#include "arch_config.h"

#include "virtio_ring.h"

static void virtio_write8(UINT16 base, UINT16 offset, UINT8 val)
{
	IO_Out8(base+offset, val);
}
static void virtio_write16(UINT16 base, UINT16 offset, UINT16 val)
{
	IO_Out16(base+offset, val);
}
static void virtio_write32(UINT16 base, UINT16 offset, UINT32 val)
{
	IO_Out32(base+offset, val);
}

static UINT8 virtio_read8(UINT16 base, UINT16 offset)
{
	return IO_In8(base+offset);
}
static UINT16 virtio_read16(UINT16 base, UINT16 offset)
{
	return IO_In16(base+offset);
}
static UINT32 virtio_read32(UINT16 base, UINT16 offset)
{
	return IO_In32(base+offset);
}

#define VIRTIO_VENDOR_ID 0x1af4
#define VIRTIO_BLK_DEVICE_ID 0x1001

#define VIRTIO_HOST_F_OFFSET			0x0000
#define VIRTIO_GUEST_F_OFFSET			0x0004
#define VIRTIO_QADDR_OFFSET				0x0008
#define VIRTIO_QSIZE_OFFSET				0x000C
#define VIRTIO_QSEL_OFFSET				0x000E
#define VIRTIO_QNOTFIY_OFFSET			0x0010
#define VIRTIO_DEV_STATUS_OFFSET		0x0012
#define VIRTIO_ISR_STATUS_OFFSET		0x0013

#define VIRTIO_DEV_SPECIFIC_OFFSET		0x0014

#define VIRTIO_STATUS_RESET			0x00
#define VIRTIO_STATUS_ACK			0x01
#define VIRTIO_STATUS_DRV			0x02
#define VIRTIO_STATUS_DRV_OK		0x04
#define VIRTIO_STATUS_FAIL			0x80

/* These two define direction. */
#define VIRTIO_BLK_T_IN		0
#define VIRTIO_BLK_T_OUT	1


/* This is the first element of the read scatter-gather list. */
struct virtio_blk_outhdr {
	/* VIRTIO_BLK_T* */
	UINT32 type;
	/* io priority. */
	UINT32 ioprio;
	/* Sector (ie. 512 byte offset) */
	UINT64 sector;
};


// Feature description
typedef struct virtio_feature
{
	char *name;
	UINT8 bit;
	UINT8 host_support;
	UINT8 guest_support;
} virtio_feature;


struct virtio_queue
{
	void* unaligned_addr;
	void* paddr;			/* physical addr of ring */
	UINT32 page;				/* physical guest page  = paddr/4096*/

	UINT16 num;				/* number of descriptors collected from device config offset*/
	UINT32 ring_size;			/* size of ring in bytes */
	struct vring vring;

	UINT16 free_num;				/* free descriptors */
	UINT16 free_head;			/* next free descriptor */
	UINT16 free_tail;			/* last free descriptor */
	UINT16 last_used;			/* we checked in used */

	void **data;				/* pointer to array of pointers */
};

struct virtio_blk_config {
	/* The capacity (in 512-byte sectors). */
	UINT64 capacity;
	/* The maximum segment size (if VIRTIO_BLK_F_SIZE_MAX) */
	UINT32 size_max;
	/* The maximum number of segments (if VIRTIO_BLK_F_SEG_MAX) */
	UINT32 seg_max;
	/* geometry the device (if VIRTIO_BLK_F_GEOMETRY) */
	struct virtio_blk_geometry {
		UINT16 cylinders;
		UINT8 heads;
		UINT8 sectors;
	} geometry;

	/* block size of device (if VIRTIO_BLK_F_BLK_SIZE) */
	UINT32 blk_size;

	/* the next 4 entries are guarded by VIRTIO_BLK_F_TOPOLOGY  */
	/* exponent for physical block per logical block. */
	UINT8 physical_block_exp;
	/* alignment offset in logical blocks. */
	UINT8 alignment_offset;
	/* minimum I/O size without performance penalty in logical blocks. */
	UINT16 min_io_size;
	/* optimal sustained I/O size in logical blocks. */
	UINT32 opt_io_size;

} __attribute__((packed));


typedef struct virtio_test {
	SysBase			*SysBase;
	ExpansionBase	*ExpansionBase;
	PCIAddress		pciAddr;

	/* For registers access */
	volatile UINT16			io_addr;
	int num_features;
	virtio_feature*   features;

	struct virtio_queue *queues;		/* our queues */
	UINT16 num_queues;

} virtio_test;

static virtio_test vt_str;
static virtio_test* vt = &vt_str;

/* Headers for requests */
static struct virtio_blk_outhdr *hdrs;

/* Status bytes for requests.
 *
 * Usually a status is only one byte in length, but we need the lowest bit
 * to propagate writable. For this reason we take u16_t and use a mask for
 * the lower byte later.
 */
static UINT16 *status;

static struct virtio_blk_config blk_config;



// Feature bits
#define VIRTIO_BLK_F_BARRIER	0	// Does host support barriers?
#define VIRTIO_BLK_F_SIZE_MAX	1	// Indicates maximum segment size
#define VIRTIO_BLK_F_SEG_MAX	2	// Indicates maximum # of segments
#define VIRTIO_BLK_F_GEOMETRY	4	// Legacy geometry available
#define VIRTIO_BLK_F_RO			5	// Disk is read-only
#define VIRTIO_BLK_F_BLK_SIZE	6	// Block size of disk is available
#define VIRTIO_BLK_F_SCSI		7	// Supports scsi command passthru
#define VIRTIO_BLK_F_FLUSH		9	// Cache flush command support
#define VIRTIO_BLK_F_TOPOLOGY	10	// Topology information is available
#define VIRTIO_BLK_ID_BYTES		20	// ID string length

static virtio_feature blkf[] = {
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



void exchange_features(APTR SysBase, virtio_test *vt)
{
	UINT32 guest_features = 0, host_features = 0;
	virtio_feature *f;

	//collect host features
	host_features = virtio_read32(vt->io_addr, VIRTIO_HOST_F_OFFSET);

	for (int i = 0; i < vt->num_features; i++)
	{
		f = &vt->features[i];

		// prepare the features the guest/driver supports
		guest_features |= (f->guest_support << f->bit);
		DPrintF("guest feature %d\n", (f->guest_support << f->bit));

		// just load the host/device feature int the struct
		f->host_support |=  ((host_features >> f->bit) & 1);
		DPrintF("host feature %d\n\n", ((host_features >> f->bit) & 1));
	}

	// let the device know about our features
	virtio_write32(vt->io_addr, VIRTIO_GUEST_F_OFFSET, guest_features);
}


void free_phys_queue(APTR SysBase, struct virtio_queue *q)
{
	FreeVec(q->unaligned_addr);
	q->paddr = 0;
	q->num = 0;
	FreeVec(q->data);
	q->data = NULL;
}

int alloc_phys_queue(APTR SysBase, struct virtio_queue *q)
{
	/* How much memory do we need? */
	q->ring_size = vring_size(q->num, 4096);
	DPrintF("q->ring_size (%d)\n", q->ring_size);

	UINT32 addr;
	q->unaligned_addr = AllocVec(q->ring_size, MEMF_FAST|MEMF_CLEAR);
	DPrintF("q->unaligned_addr (%x)\n", q->unaligned_addr);
	DPrintF("q->unaligned_addr + q->ring_size = (%x)\n", q->unaligned_addr + q->ring_size);

	addr = (UINT32)q->unaligned_addr & 4095;
	DPrintF("addr (%x)\n", addr);
	addr = (UINT32)q->unaligned_addr - addr;
	DPrintF("addr (%x)\n", addr);
	addr = addr + 4096;
	DPrintF("addr (%x)\n", addr);

	q->paddr = (void*)addr;

	if (q->unaligned_addr == NULL)
		return 0;

	q->data = AllocVec(sizeof(q->data[0]) * q->num, MEMF_FAST|MEMF_CLEAR);

	if (q->data == NULL) {
		FreeVec(q->unaligned_addr);
		q->unaligned_addr = NULL;
		q->paddr = 0;
		return 0;
	}

	return 1;
}

void init_phys_queue(APTR SysBase, struct virtio_queue *q)
{
	//not needed
	//memset(q->vaddr, 0, q->ring_size);
	//memset(q->data, 0, sizeof(q->data[0]) * q->num);

	/* physical page in guest */
	q->page = (UINT32)q->paddr / 4096;

	/* Set pointers in q->vring according to size */
	vring_init(&q->vring, q->num, q->paddr, 4096);

	DPrintF("vring desc %x\n", q->vring.desc);
	DPrintF("vring avail %x\n", q->vring.avail);
	DPrintF("vring used %x\n", q->vring.used);

/*
	// Everything's free at this point
	for (int i = 0; i < q->num; i++) {
		q->vring.desc[i].flags = VRING_DESC_F_NEXT;
		q->vring.desc[i].next = (i + 1) & (q->num - 1);
	}
*/

	q->free_num = q->num;
	q->free_head = 0;
	q->free_tail = q->num - 1;
	q->last_used = 0;

	return;
}


int init_phys_queues(APTR SysBase, virtio_test *vt)
{
	/* Initialize all queues */
	int i, j, r;
	struct virtio_queue *q;

	for (i = 0; i < vt->num_queues; i++)
	{
		q = &vt->queues[i];

		/* select the queue */
		virtio_write16(vt->io_addr, VIRTIO_QSEL_OFFSET, i);
		q->num = virtio_read16(vt->io_addr, VIRTIO_QSIZE_OFFSET);
		DPrintF("Queue %d, q->num (%d)\n", i, q->num);
		if (q->num & (q->num - 1)) {
			DPrintF("Queue %d num=%d not ^2", i, q->num);
			r = 0;
			goto free_phys_queues;
		}

		r = alloc_phys_queue(SysBase,q);

		if (r != 1)
			goto free_phys_queues;

		init_phys_queue(SysBase, q);

		/* Let the host know about the guest physical page */
		virtio_write32(vt->io_addr, VIRTIO_QADDR_OFFSET, q->page);
	}

	return 1;

/* Error path */
free_phys_queues:
	for (j = 0; j < i; j++)
	{
		free_phys_queue(SysBase, &vt->queues[i]);
	}

	return r;
}


int virtio_alloc_queues(APTR SysBase, virtio_test *vt, int num_queues)
{
	int r = 1;

	// Assume there's no device with more than 256 queues
	if (num_queues < 0 || num_queues > 256)
		return 0;

	vt->num_queues = num_queues;

	// allocate queue memory
	vt->queues = AllocVec(num_queues * sizeof(vt->queues[0]), MEMF_FAST|MEMF_CLEAR);

	if (vt->queues == NULL)
		return 0;

	//not needed in because of MEMF_CLEAR
	//memset(dev->queues, 0, num_queues * sizeof(dev->queues[0]));

	r = init_phys_queues(SysBase, vt);

	if ((r != 1)) {
		DPrintF("Could not initialize queues (%d)\n", r);
		FreeVec(vt->queues);
		vt->queues = NULL;
	}

	return r;
}


void virtio_free_queues(APTR SysBase, virtio_test *dev)
{
	int i;
	for (i = 0; i < dev->num_queues; i++)
	{
		free_phys_queue(SysBase, &dev->queues[i]);
	}

	dev->num_queues = 0;
	dev->queues = NULL;
}

#define VIRTIO_BLK_NUM_THREADS 1

int virtio_blk_alloc_requests(APTR SysBase)
{
	/* Allocate memory for request headers and status field */

	hdrs = AllocVec(VIRTIO_BLK_NUM_THREADS * sizeof(hdrs[0]),
				MEMF_FAST|MEMF_CLEAR);

	if (!hdrs)
		return 0;

	status = AllocVec(VIRTIO_BLK_NUM_THREADS * sizeof(status[0]),
				  MEMF_FAST|MEMF_CLEAR);

	if (!status) {
		FreeVec(hdrs);
		return 0;
	}

	return 1;
}

int supports(APTR SysBase, virtio_test *dev, int bit, int host)
{
	for (int i = 0; i < dev->num_features; i++)
	{
		struct virtio_feature *f = &dev->features[i];

		if (f->bit == bit)
			return host ? f->host_support : f->guest_support;
	}
	DPrintF("ERROR: Bit not found!!\n");
	return 0;
}



int virtio_host_supports(APTR SysBase, virtio_test *dev, int bit)
{
	return supports(SysBase, dev, bit, 1);
}

int virtio_guest_supports(APTR SysBase, virtio_test *dev, int bit)
{
	return supports(SysBase, dev, bit, 0);
}


int virtio_blk_configuration(APTR SysBase, virtio_test *vt)
{
	UINT32 sectors_low, sectors_high, size_mbs;

	/* capacity is always there */
	sectors_low = virtio_read32(vt->io_addr, VIRTIO_DEV_SPECIFIC_OFFSET+0);
	sectors_high = virtio_read32(vt->io_addr, VIRTIO_DEV_SPECIFIC_OFFSET+4);
	blk_config.capacity = ((UINT64)sectors_high << 32) | sectors_low;

	/* If this gets truncated, you have a big disk... */
	size_mbs = (UINT32)(blk_config.capacity * 512 / 1024 / 1024);
	DPrintF("Capacity: %d MB\n", size_mbs);


	if (virtio_host_supports(SysBase, vt, VIRTIO_BLK_F_SEG_MAX))
	{
		blk_config.seg_max = virtio_read32(vt->io_addr, VIRTIO_DEV_SPECIFIC_OFFSET+12);
		DPrintF("Seg Max: %d\n", blk_config.seg_max);
	}

	if (virtio_host_supports(SysBase, vt, VIRTIO_BLK_F_GEOMETRY))
	{
		blk_config.geometry.cylinders = virtio_read16(vt->io_addr, VIRTIO_DEV_SPECIFIC_OFFSET+16);
		blk_config.geometry.heads = virtio_read8(vt->io_addr, VIRTIO_DEV_SPECIFIC_OFFSET+18);
		blk_config.geometry.sectors = virtio_read8(vt->io_addr, VIRTIO_DEV_SPECIFIC_OFFSET+19);

		DPrintF("Geometry: cyl=%d heads=%d sectors=%d\n",
					blk_config.geometry.cylinders,
					blk_config.geometry.heads,
					blk_config.geometry.sectors);
	}

	if (virtio_host_supports(SysBase, vt, VIRTIO_BLK_F_BLK_SIZE))
	{
		blk_config.blk_size = virtio_read32(vt->io_addr, VIRTIO_DEV_SPECIFIC_OFFSET+20);
		DPrintF("Block Size: %d\n", blk_config.blk_size);
	}

	return 0;
}

void test_mhz_delay(SysBase *SysBase, int sec);
void virtio_blk_transfer(APTR SysBase, virtio_test* vt, UINT32 sector_num, UINT8 write, UINT8* buf)
{
	//prepare first out_hdr, since we have only one, replace 0 by a variable
	//memset(&hdrs[0], 0, sizeof(hdrs[0]));

	if(write == 1)
	{
		//for writing to disk
		hdrs[0].type = VIRTIO_BLK_T_OUT;
	}
	else
	{
		//for reading from disk
		hdrs[0].type = VIRTIO_BLK_T_IN;
	}

	//fill up sector
	hdrs[0].ioprio = 0;
	hdrs[0].sector = sector_num;

	//clear status
	status[0] = 1; //0 means success, 1 means error, 2 means unsupported

	DPrintF("\n\n\n sector = %d\n", sector_num);
	DPrintF("idx = %d\n", (vt->queues[0]).vring.avail->idx);


	//fill into descriptor table
	(vt->queues[0]).vring.desc[0].addr = (UINT32)&hdrs[0];
	(vt->queues[0]).vring.desc[0].len = sizeof(hdrs[0]);
	(vt->queues[0]).vring.desc[0].flags = VRING_DESC_F_NEXT;
	(vt->queues[0]).vring.desc[0].next = 1;

	(vt->queues[0]).vring.desc[1].addr = (UINT32)buf;
	(vt->queues[0]).vring.desc[1].len = 512;
	(vt->queues[0]).vring.desc[1].flags = VRING_DESC_F_NEXT | VRING_DESC_F_WRITE;
	(vt->queues[0]).vring.desc[1].next = 2;

	(vt->queues[0]).vring.desc[2].addr = (UINT32)&status[0];
	(vt->queues[0]).vring.desc[2].len = sizeof(status[0]);
	(vt->queues[0]).vring.desc[2].flags = VRING_DESC_F_WRITE;
	(vt->queues[0]).vring.desc[2].next = 0;

	//fill in available ring
	(vt->queues[0]).vring.avail->flags = 0; //1 mean no interrupt needed, 0 means interrupt needed
	(vt->queues[0]).vring.avail->ring[0] = 0; // 0 is the head of above request descriptor chain
	(vt->queues[0]).vring.avail->idx = (vt->queues[0]).vring.avail->idx + 3; //next available descriptor

	//notify
	virtio_write16(vt->io_addr, VIRTIO_QNOTFIY_OFFSET, 0); //notify that 1st queue (0) of this device has been updated

	//give some delay
	test_mhz_delay(SysBase, 1);

	DPrintF("status[0] %d\n", status[0]);

/*
	int j=0;

	DPrintF("(vt->queues[0]).vring.used->flags %d\n", (vt->queues[0]).vring.used->flags);
	DPrintF("(vt->queues[0]).vring.used->idx %d\n", (vt->queues[0]).vring.used->idx);
	for(j=0; j<(vt->queues[0]).num;j++)
	{
		DPrintF("(vt->queues[0]).vring.used->ring[%d].id %d\n", j, (vt->queues[0]).vring.used->ring[j].id);
		DPrintF("(vt->queues[0]).vring.used->ring[%d].len %d\n", j, (vt->queues[0]).vring.used->ring[j].len);
	}

	DPrintF("(vt->queues[0]).vring.avail->flags %d\n", (vt->queues[0]).vring.avail->flags);
	DPrintF("(vt->queues[0]).vring.avail->idx %d\n", (vt->queues[0]).vring.avail->idx);
	for(j=0; j<(vt->queues[0]).num;j++)
	{
		DPrintF("(vt->queues[0]).vring.avail->ring[%d] %d\n", j, (vt->queues[0]).vring.avail->ring[j]);
	}
*/

	//See if virtio device generated an interrupt(1) or not(0)
	UINT8 isr;
	isr=virtio_read8(vt->io_addr, VIRTIO_ISR_STATUS_OFFSET);
	DPrintF("virtio_blk_transfer: isr= %d\n", isr);

	DPrintF("virtio_blk_transfer: buf[0]= %x\n", buf[0]);
	DPrintF("virtio_blk_transfer: buf[1]= %x\n", buf[1]);
	DPrintF("virtio_blk_transfer: buf[2]= %x\n", buf[2]);
	DPrintF("virtio_blk_transfer: buf[3]= %x\n", buf[3]);

}

void DetectVirtio(APTR SysBase)
{
	DPrintF("DetectVirtio\n");
	struct ExpansionBase *ExpansionBase = (struct ExpansionBase *)OpenLibrary("expansion.library", 0);
	vt->ExpansionBase = ExpansionBase;

	if (vt->ExpansionBase == NULL) {
		DPrintF("DetectVirtio: Cant open expansion.library\n");
		return;
	}

	if (!PCIFindDevice(VIRTIO_VENDOR_ID, VIRTIO_BLK_DEVICE_ID, &vt->pciAddr)) {
		DPrintF("DetectVirtio: No Virtio device found.");
		return ;
	}
	else
	{
		DPrintF("DetectVirtio: Virtio block device found.\n");
	}

	DPrintF("DetectVirtio: (vt->pciAddr).bus %x\n", (vt->pciAddr).bus);
	DPrintF("DetectVirtio: (vt->pciAddr).device %x\n", (vt->pciAddr).device);
	DPrintF("DetectVirtio: (vt->pciAddr).function %x\n", (vt->pciAddr).function);

	PCISetMemEnable(&vt->pciAddr, TRUE);
	vt->io_addr = PCIGetBARAddr(&vt->pciAddr, 0);
	DPrintF("DetectVirtio: ioAddress %x\n", vt->io_addr);

	UINT8 intLine;
	intLine = PCIGetIntrLine(&vt->pciAddr);
	DPrintF("DetectVirtio: intLine %x\n", intLine);

	UINT8 intPin;
	intPin = PCIGetIntrPin(&vt->pciAddr);
	DPrintF("DetectVirtio: intPin %x\n", intPin);

	// Reset the device
	virtio_write8(vt->io_addr, VIRTIO_DEV_STATUS_OFFSET, VIRTIO_STATUS_RESET);

	// Ack the device
	virtio_write8(vt->io_addr, VIRTIO_DEV_STATUS_OFFSET, VIRTIO_STATUS_ACK);

	//driver supports these features
	vt->features = blkf;
	vt->num_features = sizeof(blkf) / sizeof(blkf[0]);

	//exchange features
	exchange_features(SysBase, vt);

	//initialize indirect desc tables
	//init_indirect_desc_tables();

	// We know how to drive the device...
	virtio_write8(vt->io_addr, VIRTIO_DEV_STATUS_OFFSET, VIRTIO_STATUS_DRV);

	// virtio blk has only 1 queue
	virtio_alloc_queues(SysBase, vt, 1);

	/* Allocate memory for headers and status */
	if (virtio_blk_alloc_requests(SysBase) != 1)
	{
		virtio_free_queues(SysBase, vt);
		return;
	}

	virtio_blk_configuration(SysBase, vt);

	/* Let the host know that we are ready */
	/* Driver is ready to go! */
	virtio_write8(vt->io_addr, VIRTIO_DEV_STATUS_OFFSET, VIRTIO_STATUS_DRV_OK);


	UINT32 sector_num;
	UINT8 write = 0; //0 means "READ" a sector, 1 means "WRITE"
	UINT8 buf[512]; //buffer to which data is read/write, fill this buffer to write into device

	int i;
	for (i=0; i < 8192; i++) //8192 max
	{
		//lets try to read the sectors
		sector_num = i;
		memset(buf, 0, 512);
		virtio_blk_transfer(SysBase, vt, sector_num, write, buf);
	}
}



