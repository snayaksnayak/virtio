#include "exec_funcs.h"
#include "virtio_blk_internal.h"
#include "device_error.h"

#define SysBase VirtioBlkBase->VirtioBlk_SysBase

struct VirtioBlkBase* virtio_blk_OpenDev(struct VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq, UINT32 unitNum, UINT32 flags)
{
	ioreq->io_Error = IOERR_OPENFAIL;
	if(unitNum < VirtioBlkBase->NumAvailUnits)
	{
		VirtioBlkBase->Device.dd_Library.lib_OpenCnt++;
		VirtioBlkBase->Device.dd_Library.lib_Flags &= ~LIBF_DELEXP;

		ioreq->io_Error = 0;
		ioreq->io_Unit = (struct  Unit *)&VirtioBlkBase->VirtioBlkUnit[unitNum];
		ioreq->io_Device = (struct Device *)VirtioBlkBase;
	}
	else
	{
		ioreq->io_Error = BLK_ERR_BadUnitNum;
	}
	return(VirtioBlkBase);
}

APTR virtio_blk_CloseDev(struct VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq)
{
	VirtioBlkBase->Device.dd_Library.lib_OpenCnt--;
	if(!VirtioBlkBase->Device.dd_Library.lib_OpenCnt)
	{
		// Should we "expunge" the device?
	}
	ioreq->io_Unit = NULL;
	ioreq->io_Device = NULL;
	return (VirtioBlkBase);
}

APTR virtio_blk_ExpungeDev(struct VirtioBlkBase *VirtioBlkBase)
{
	return (NULL);
}

APTR virtio_blk_ExtFuncDev(void)
{
	return (NULL);
}


void virtio_blk_BeginIO(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq)
{
	UINT8 cmd = ioreq->io_Command;
	ioreq->io_Error = 0;
	ioreq->io_Flags &= (~(IOF_QUEUED|IOF_CURRENT|IOF_SERVICING|IOF_DONE))&0x0ff;

	if (cmd == CMD_START
	|| cmd == CMD_STOP
	|| cmd == CMD_READ
	|| cmd == CMD_WRITE
	|| cmd == CMD_CLEAR
	|| cmd == CMD_UPDATE
	|| cmd == VB_GETDEVICEINFO
	|| cmd == VB_GETDISKCHANGECOUNT
	|| cmd == VB_GETDISKPRESENCESTATUS
	|| cmd == VB_EJECT
	|| cmd == VB_FORMAT)
	{
		//valid command
	}
	else
	{
		cmd = CMD_INVALID; // Invalidate the command.
	}

	VirtioBlkCmdVector[cmd](VirtioBlkBase, ioreq);
}

void virtio_blk_AbortIO(VirtioBlkBase *VirtioBlkBase, struct IOStdReq *ioreq)
{
	Forbid();
	if(ioreq->io_Message.mn_Node.ln_Type != NT_REPLYMSG)
    {
		if(TEST_BITS(ioreq->io_Flags, IOF_QUEUED))
		{
			//these operations operate on single bits only.
			//if, flag values are totally exclusive of each other,
			//we can directly use assignment rather than
			//using these two operations simultaneously.
			CLEAR_BITS(ioreq->io_Flags, IOF_QUEUED);
			SET_BITS(ioreq->io_Flags, IOF_DONE);
			VirtioBlk_end_command(VirtioBlkBase, ioreq, IOERR_ABORTED );
			Permit();
			return;
		}
		else
		{
			Permit();
			return;
		}
	}

	Permit();
	return;
}



