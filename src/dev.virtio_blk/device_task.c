#include "exec_funcs.h"
#include "virtio_blk_internal.h"


UINT32 VirtioBlk_WorkerTaskFunction(VirtioBlkBase *VirtioBlkBase, APTR *SysBase)
{
	//For every unit, this function is called once
	//to create a worker task.
	//When this function gets called, NumAvailUnits
	//represents the unit number for which this worker task is created.
	volatile UINT32 unit_num = VirtioBlkBase->NumAvailUnits;
	DPrintF("VirtioBlk_WorkerTaskFunction: unit_num= %d\n", unit_num);
	VirtioBlk_InitMsgPort(&(VirtioBlkBase->VirtioBlkUnit[unit_num].vb_unit.unit_MsgPort), SysBase);

	DPrintF("VirtioBlk_WorkerTaskFunction: Signal sent\n");
	Signal(VirtioBlkBase->VirtioBlk_BootTask, SIGF_SINGLE);

	VirtioBlk_CheckPort(unit_num, VirtioBlkBase);

	for(;;)
	{
		DPrintF("VirtioBlk_WorkerTaskFunction...caught in a loop\n");
	}
	return 0;
}


INT8 VirtioBlk_InitMsgPort(struct MsgPort *mport, APTR *SysBase)
{
	INT8 signalNum = -1;
	signalNum = AllocSignal(-1);
	if (signalNum != -1)
    {
		mport->mp_SigBit = signalNum;
		mport->mp_Flags  = PA_SIGNAL;
		mport->mp_Node.ln_Type = NT_MSGPORT;
		mport->mp_SigTask = (struct Task *)FindTask(NULL);

		NewListType(&mport->mp_MsgList, NT_MSGPORT);
	}
	else
	{
		DPrintF("VirtioBlk_InitMsgPort: failed to allocate signal\n");
	}
	return signalNum;
}

#define SysBase VirtioBlkBase->VirtioBlk_SysBase

void VirtioBlk_CheckPort(UINT32 unit_num, VirtioBlkBase *VirtioBlkBase)
{
	struct  Unit *unit;
	struct MsgPort *mport;
	struct IOStdReq *curr_req;

	unit = (struct  Unit *)&((VirtioBlkBase->VirtioBlkUnit[unit_num]).vb_unit);
	mport = &(unit->unit_MsgPort);
	while(1)
	{
		DPrintF("VirtioBlk_CheckPort loop\n");

		curr_req = (struct IOStdReq *)GetHead(&(unit->unit_MsgPort.mp_MsgList));
		DPrintF("VirtioBlk_CheckPort curr_req = %x\n", curr_req);
		if (curr_req != NULL)
		{
			if(TEST_BITS(curr_req->io_Flags, IOF_QUEUED))
			{
				SetSignal(0L, SIGF_SINGLE);
				SET_BITS(curr_req->io_Flags, IOF_SERVICING);
				//start processing another request
				VirtioBlk_process_request(VirtioBlkBase, curr_req);
				DPrintF("VirtioBlk_CheckPort: wait after VirtioBlk_process_request\n");
				Wait(SIGF_SINGLE);
			}

			if(TEST_BITS(curr_req->io_Flags, IOF_DONE))
			{
				DPrintF("One request complete\n");
				Remove((struct Node *)curr_req);
				curr_req->io_Error = 0;
				ReplyMsg((struct Message *)curr_req);
			}
		}
		else
		{
			DPrintF("VirtioBlk_CheckPort before wait\n");
			DPrintF("VirtioBlk_CheckPort waiting on unit_num= %d, unit= %x, port= %x\n", unit_num, unit, mport);
			WaitPort(mport);
			DPrintF("VirtioBlk_CheckPort after wait\n");
		}
	}
}




