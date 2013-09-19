#ifndef virtio_internal_h
#define virtio_internal_h

#include "types.h"
#include "library.h"
#include "arch_config.h"
#include "virtio_ring.h"
#include "expansionbase.h"

#define VIRTIO_VERSION  0
#define VIRTIO_REVISION 1

//vendor id for all virtio devices
#define VIRTIO_VENDOR_ID 0x1af4

//pci config offsets
#define VIRTIO_HOST_F_OFFSET			0x0000
#define VIRTIO_GUEST_F_OFFSET			0x0004
#define VIRTIO_QADDR_OFFSET				0x0008
#define VIRTIO_QSIZE_OFFSET				0x000C
#define VIRTIO_QSEL_OFFSET				0x000E
#define VIRTIO_QNOTFIY_OFFSET			0x0010
#define VIRTIO_DEV_STATUS_OFFSET		0x0012
#define VIRTIO_ISR_STATUS_OFFSET		0x0013
#define VIRTIO_DEV_SPECIFIC_OFFSET		0x0014

//device status
#define VIRTIO_STATUS_RESET			0x00
#define VIRTIO_STATUS_ACK			0x01
#define VIRTIO_STATUS_DRV			0x02
#define VIRTIO_STATUS_DRV_OK		0x04
#define VIRTIO_STATUS_FAIL			0x80

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

// Feature description
typedef struct virtio_feature
{
	char *name;
	UINT8 bit;
	UINT8 host_support;
	UINT8 guest_support;
} virtio_feature;


typedef struct VirtioDevice
{
	PCIAddress		pciAddr;
	volatile UINT16			io_addr;

	UINT8 intLine;
	UINT8 intPin;

	int num_features;
	virtio_feature*   features;

	UINT16 num_queues;
	struct virtio_queue *queues;

} VirtioDevice;


typedef struct VirtioBase
{
	struct Library		Library;
	APTR				SysBase;

} VirtioBase;


extern char VirtioName[];
extern char VirtioVer[];


//functions
struct VirtioBase *virtio_OpenLib(struct VirtioBase *VirtioBase);
APTR virtio_CloseLib(struct VirtioBase *VirtioBase);
APTR virtio_ExpungeLib(struct VirtioBase *VirtioBase);
APTR virtio_ExtFuncLib(void);

void virtio_Write8(struct VirtioBase *VirtioBase, UINT16 base, UINT16 offset, UINT8 val);
void virtio_Write16(struct VirtioBase *VirtioBase, UINT16 base, UINT16 offset, UINT16 val);
void virtio_Write32(struct VirtioBase *VirtioBase, UINT16 base, UINT16 offset, UINT32 val);
UINT8 virtio_Read8(struct VirtioBase *VirtioBase, UINT16 base, UINT16 offset);
UINT16 virtio_Read16(struct VirtioBase *VirtioBase, UINT16 base, UINT16 offset);
UINT32 virtio_Read32(struct VirtioBase *VirtioBase, UINT16 base, UINT16 offset);

void virtio_ExchangeFeatures(struct VirtioBase *VirtioBase, VirtioDevice *vd);
int virtio_AllocateQueues(struct VirtioBase *VirtioBase, VirtioDevice *vd, INT32 num_queues);
int virtio_InitQueues(struct VirtioBase *VirtioBase, VirtioDevice *vd);
void virtio_FreeQueues(struct VirtioBase *VirtioBase, VirtioDevice *vd);
int virtio_HostSupports(VirtioBase *VirtioBase, VirtioDevice *vd, int bit);
int virtio_GuestSupports(VirtioBase *VirtioBase, VirtioDevice *vd, int bit);


//internals
int Virtio_supports(VirtioBase *VirtioBase, VirtioDevice *vd, int bit, int host);

int Virtio_alloc_phys_queue(VirtioBase *VirtioBase, struct virtio_queue *q);
void Virtio_init_phys_queue(VirtioBase *VirtioBase, struct virtio_queue *q);
void Virtio_free_phys_queue(VirtioBase *VirtioBase, struct virtio_queue *q);


//outer name (vectors) of our own library functions,
//we can use them inside our library too.
#define VirtioWrite8(a,b,c) (((void(*)(APTR, UINT16, UINT16, UINT8)) 	_GETVECADDR(VirtioBase, 5))(VirtioBase, a, b, c))
#define VirtioWrite16(a,b,c) (((void(*)(APTR, UINT16, UINT16, UINT16)) 	_GETVECADDR(VirtioBase, 6))(VirtioBase, a, b, c))
#define VirtioWrite32(a,b,c) (((void(*)(APTR, UINT16, UINT16, UINT32)) 	_GETVECADDR(VirtioBase, 7))(VirtioBase, a, b, c))
#define VirtioRead8(a,b) (((UINT8(*)(APTR, UINT16, UINT16)) 	_GETVECADDR(VirtioBase, 8))(VirtioBase, a, b))
#define VirtioRead16(a,b) (((UINT16(*)(APTR, UINT16, UINT16)) 	_GETVECADDR(VirtioBase, 9))(VirtioBase, a, b))
#define VirtioRead32(a,b) (((UINT32(*)(APTR, UINT16, UINT16)) 	_GETVECADDR(VirtioBase, 10))(VirtioBase, a, b))

#define VirtioExchangeFeatures(a) (((void(*)(APTR, VirtioDevice*)) 	_GETVECADDR(VirtioBase, 11))(VirtioBase, a))
#define VirtioAllocateQueues(a,b) (((int(*)(APTR, VirtioDevice*, INT32)) 	_GETVECADDR(VirtioBase, 12))(VirtioBase, a, b))
#define VirtioInitQueues(a) (((int(*)(APTR, VirtioDevice*)) 	_GETVECADDR(VirtioBase, 13))(VirtioBase, a))
#define VirtioFreeQueues(a) (((void(*)(APTR, VirtioDevice*)) 	_GETVECADDR(VirtioBase, 14))(VirtioBase, a))
#define VirtioHostSupports(a,b) (((int(*)(APTR, VirtioDevice*, int)) 	_GETVECADDR(VirtioBase, 15))(VirtioBase, a, b))
#define VirtioGuestSupports(a,b) (((int(*)(APTR, VirtioDevice*, int)) 	_GETVECADDR(VirtioBase, 16))(VirtioBase, a, b))


#endif //virtio_internal_h


