#include "virtio_blk_internal.h"
#include "list.h"
#include "exec_funcs.h"

__attribute__((no_instrument_function)) BOOL VirtioBlkIRQServer(UINT32 number, VirtioBlkBase *VirtioBlkBase, APTR SysBase)
{
	//DPrintF("VirtioBlkIRQServer\n");
	struct LibVirtioBase *LibVirtioBase = VirtioBlkBase->LibVirtioBase;

	VirtioBlk *vb;
	VirtioDevice* vd;
	struct  Unit *unit;
	struct IOStdReq *head_req;
	int unit_num;

	//find out which unit generated this irq
	for (unit_num=0; unit_num < VirtioBlkBase->NumAvailUnits; unit_num++)
	{
		vb = &((VirtioBlkBase->VirtioBlkUnit[unit_num]).vb);
		vd = &(vb->vd);

		//See if virtio device generated an interrupt(1) or not(0)
		UINT8 isr;
		isr=VirtioRead8(vd->io_addr, VIRTIO_ISR_STATUS_OFFSET);
		//DPrintF("VirtioBlkIRQServer: isr= %d\n", isr);

		if (isr == 1)
		{
			//now use the found unit_num to continue processing
			unit = (struct  Unit *)&((VirtioBlkBase->VirtioBlkUnit[unit_num]).vb_unit);
			head_req = (struct IOStdReq *)GetHead(&(unit->unit_MsgPort.mp_MsgList));

			if(TEST_BITS(head_req->io_Flags, IOF_SERVICING))
			{
				SET_BITS(head_req->io_Flags, IOF_DONE);
				Signal(VirtioBlkBase->VirtioBlkUnit[unit_num].VirtioBlk_WorkerTask, SIGF_SINGLE);
			}

			return 1; // was for us, dont proceed daisy chain
		}
	}
	return 0; // Not for us, proceed in daisy chain
}

