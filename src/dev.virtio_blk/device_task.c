#include "exec_funcs.h"
#include "virtio_blk_internal.h"


UINT32 VirtioBlk_WorkerTaskFunction(struct VirtioBlkTaskData *VirtioBlk_WorkerTaskData, APTR *SysBase)
{
	VirtioBlkBase* VirtioBlkBase;
	UINT32 unit_num = VirtioBlk_WorkerTaskData->unitNum;
	VirtioBlkBase = VirtioBlk_WorkerTaskData->VirtioBlkBase;

	DPrintF("VirtioBlk_WorkerTaskFunction: unit_num= %d\n", unit_num);
	VirtioBlk_InitMsgPort(&(VirtioBlkBase->VirtioBlkUnit[unit_num].vb_unit.unit_MsgPort), SysBase);

	INT8 signalNum = -1;
	signalNum = AllocSignal(-1);
	if (signalNum != -1)
    {
		VirtioBlkBase->VirtioBlkUnit[unit_num].taskWakeupSignal = signalNum;
	}
	else
	{
		DPrintF("VirtioBlk_WorkerTaskFunction: failed to allocate signal\n");
	}

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
		Forbid();

		curr_req = (struct IOStdReq *)GetHead(&(unit->unit_MsgPort.mp_MsgList));
		DPrintF("VirtioBlk_CheckPort curr_req = %x\n", curr_req);
		if (curr_req != NULL)
		{
			if(TEST_BITS(curr_req->io_Flags, IOF_QUEUED))
			{
				CLEAR_BITS(curr_req->io_Flags, IOF_QUEUED);
				SET_BITS(curr_req->io_Flags, IOF_CURRENT);
				Permit();

				//start processing another request
				VirtioBlk_process_request(VirtioBlkBase, unit_num);

				Forbid();
				Remove((struct Node *)curr_req);
				CLEAR_BITS(curr_req->io_Flags, IOF_CURRENT);
				SET_BITS(curr_req->io_Flags, IOF_DONE);
				curr_req->io_Error = 0;
				Permit();

				ReplyMsg((struct Message *)curr_req);
			}
			else if (TEST_BITS(curr_req->io_Flags, IOF_DONE))
			{
				//probably already aborted!
				Remove((struct Node *)curr_req);
				Permit();

				ReplyMsg((struct Message *)curr_req);
			}
			else
			{
				Permit();
			}
		}
		else
		{
			Permit();
			DPrintF("VirtioBlk_CheckPort waiting on unit_num= %d, unit= %x, port= %x\n", unit_num, unit, mport);
			WaitPort(mport);
			DPrintF("VirtioBlk_CheckPort waiting over\n");
		}
	}
}




