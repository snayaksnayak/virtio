#ifndef virtio_blk_internal_h
#define virtio_blk_internal_h

#include "types.h"
#include "device.h"
#include "io.h"
#include "expansionbase.h"
#include "lib_virtio.h"


#define VERSION  0
#define REVISION 1

#define DEVNAME "virtio_blk"
#define DEVVER  " 0.1 __DATE__"

#define VIRTIO_BLK_INT_PRI 0

#define IOF_QUEUED (1<<4)
#define IOF_CURRENT (1<<5)
#define IOF_SERVICING (1<<6)
#define IOF_DONE (1<<7)

// Units
#define VB_UNIT_MAX		4

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
	LibVirtioBase* LibVirtioBase;

	UINT32				VirtioBlkIRQ;
	struct Interrupt	*VirtioBlkIntServer;
	Task				*VirtioBlk_BootTask;

	struct VirtioBlkUnit VirtioBlkUnit[VB_UNIT_MAX];
	UINT32 NumAvailUnits;

} VirtioBlkBase;


extern char DevName[];
extern char Version[];


extern void (*VirtioBlkCmdVector[])(VirtioBlkBase *, struct IOStdReq*);
//extern INT8 VirtioBlkCmdQuick[];

//functions
struct VirtioBlkBase *virtio_blk_OpenDev(struct VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq, UINT32 unitNum, UINT32 flags);
APTR virtio_blk_CloseDev(struct VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq);
APTR virtio_blk_ExpungeDev(struct VirtioBlkBase *VirtioBlkBase);
APTR virtio_blk_ExtFuncDev(void);
void virtio_blk_BeginIO(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq);
void virtio_blk_AbortIO(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq);


//commands
void VirtioBlkInvalid(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq);
void VirtioBlkStart(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq);
void VirtioBlkStop(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq);
void VirtioBlkRead(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq);
void VirtioBlkWrite(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq);
void VirtioBlkGetDeviceInfo(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq);

//internals
void VirtioBlk_end_command(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq, INT8 error);
void VirtioBlk_queue_command(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq);
void VirtioBlk_process_request(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq);

int VirtioBlk_setup(VirtioBlkBase *VirtioBlkBase,VirtioBlk *vb, INT32 unit_num);
int VirtioBlk_alloc_phys_requests(VirtioBlkBase *VirtioBlkBase,VirtioBlk *vb);
int VirtioBlk_configuration(VirtioBlkBase *VirtioBlkBase, VirtioBlk *vb);
void VirtioBlk_transfer(VirtioBlkBase *VirtioBlkBase, VirtioBlk* vb, UINT32 sector_start, UINT32 num_sectors, UINT8 write, UINT8* buf);

UINT32 VirtioBlk_WorkerTaskFunction(struct VirtioBlkTaskData *VirtioBlk_WorkerTaskData, APTR *SysBase);
INT8 VirtioBlk_InitMsgPort(struct MsgPort *mport, APTR *SysBase);
void VirtioBlk_CheckPort(UINT32 unit_num, VirtioBlkBase *VirtioBlkBase);




//irq handler
__attribute__((no_instrument_function)) BOOL VirtioBlkIRQServer(UINT32 number, VirtioBlkBase *VirtioBlkBase, APTR SysBase);

#endif //virtio_blk_internal_h


