#include "virtio_blk_internal.h"
#include "list.h"
#include "exec_funcs.h"

__attribute__((no_instrument_function)) BOOL VirtioBlkIRQServer(UINT32 number, VirtioBlkBase *VirtioBlkBase, APTR SysBase)
{
	DPrintF("VirtioBlkIRQServer\n");
	struct LibVirtioBase *LibVirtioBase = VirtioBlkBase->LibVirtioBase;

	VirtioBlk *vb;
	VirtioDevice* vd;
	struct  Unit *unit;
	int unit_num;

	//find out which unit generated this irq
	for (unit_num=0; unit_num < VirtioBlkBase->NumAvailUnits; unit_num++)
	{
		vb = &((VirtioBlkBase->VirtioBlkUnit[unit_num]).vb);
		vd = &(vb->vd);

		//See if virtio device generated an interrupt(1) or not(0)
		UINT8 isr;
		isr=VirtioRead8(vd->io_addr, VIRTIO_ISR_STATUS_OFFSET);
		DPrintF("VirtioBlkIRQServer: isr= %d\n", isr);

		if (isr == 1)
		{
			//now use the found unit_num to continue processing
			unit = (struct  Unit *)&((VirtioBlkBase->VirtioBlkUnit[unit_num]).vb_unit);
			struct VirtioBlkRequest *head_req = (struct VirtioBlkRequest *)GetHead(&unit->unit_MsgPort.mp_MsgList);

			DPrintF("One request complete\n");
			Remove((struct Node *)head_req);
			head_req->node.io_Error = 0;
			ReplyMsg((struct Message *)head_req);

			struct IOStdReq* next_req = (struct IOStdReq *)GetHead(&unit->unit_MsgPort.mp_MsgList);
			if(next_req != NULL)
			{
				//start processing another request
				VirtioBlk_process_request(VirtioBlkBase, next_req);
			}
			return 1; // was for us, dont proceed daisy chain
		}
	}
	return 0; // Not for us, proceed in daisy chain
}

