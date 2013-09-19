#ifndef virtio_blk_h
#define virtio_blk_h

// this file shall go to top most include folder in future

#include "types.h"
#include "device.h"
#include "io.h"
#include "virtio.h"

// Units
#define VB_UNIT_MAX    4

// non standard async commands
#define VB_GETDEVICEINFO (CMD_NONSTD+0)


//****************
#define VIRTIO_BLK_DEVICE_ID 0x1001

/* These two define direction. */
#define VIRTIO_BLK_T_IN		0
#define VIRTIO_BLK_T_OUT	1

//how many ques we have
#define VIRTIO_BLK_NUM_QUEUES 1

// Feature bits for virtio blk device
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



/* This is the first element of the read scatter-gather list. */
struct virtio_blk_outhdr {
	/* VIRTIO_BLK_T* */
	UINT32 type;
	/* io priority. */
	UINT32 ioprio;
	/* Sector (ie. 512 byte offset) */
	UINT64 sector;
};

//*****************

struct VirtioBlkGeometry
{
	UINT16 cylinders;
	UINT8 heads;
	UINT8 sectors;
};

struct VirtioBlkDeviceInfo
{
	UINT64 capacity; //number of 512 byte sectors
	UINT32 blk_size; //512 or 1024 etc.
	struct VirtioBlkGeometry geometry;
};


typedef struct VirtioBlk
{
	//dev structure
	struct VirtioDevice vd;

	//device capacity etc.
	struct VirtioBlkDeviceInfo Info;

	//Headers for requests
	struct virtio_blk_outhdr *hdrs;

	//Status bytes for requests.
	//Usually a status is only one byte in length, but we need the lowest bit
	//to propagate writable. For this reason we take u16_t and use a mask for
	//the lower byte later.
	UINT16 *status;


} VirtioBlk;

struct VirtioBlkBase;
struct VirtioBlkTaskData
{
	struct VirtioBlkBase* VirtioBlkBase;
	UINT32 unitNum;
};

struct VirtioBlkUnit
{
	struct Unit vb_unit;
	struct VirtioBlk vb;
	Task				*VirtioBlk_WorkerTask;
	struct VirtioBlkTaskData *VirtioBlk_WorkerTaskData;
	INT8 taskWakeupSignal;
};

typedef struct VirtioBlkBase
{
	struct Device		Device;
	APTR				VirtioBlk_SysBase;
	ExpansionBase* ExpansionBase;
	VirtioBase* VirtioBase;

	UINT32				VirtioBlkIRQ;
	struct Interrupt	*VirtioBlkIntServer;
	Task				*VirtioBlk_BootTask;

	struct VirtioBlkUnit VirtioBlkUnit[VB_UNIT_MAX];
	UINT32 NumAvailUnits;

} VirtioBlkBase;



// declaration of library style synchronous functions (non standard or device specific)
// we have nothing now

// vectors to library style synchronous functions (non standard or device specific)
// we have nothing now

#endif //virtio_blk_h
