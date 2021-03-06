#include "types.h"
#include "sysbase.h"
#include "resident.h"
#include "timer.h"
#include "exec_funcs.h"
#include "mouseport.h"
#include "keyboard.h"
#include "inputevent.h"
#include "memory.h"

#include "virtio_blk.h"

// This is a Testsuite for implementations on the OS.
// Since we boot out of the ROM, we need an Resident Structure

static struct TestBase *test_Init(struct TestBase *TestBase, UINT32 *segList, struct SysBase *SysBase);
static void test_TestTask(APTR data, struct SysBase *SysBase);
static void test_TestTask0(APTR data, struct SysBase *SysBase);
static void test_TestTask1(APTR data, struct SysBase *SysBase);

static const char name[] = "test.device";
static const char version[] = "test 0.1\n";
static const char EndResident;
#define DEVICE_VERSION 0
#define DEVICE_REVISION 1

struct TestBase
{
	struct Library TestLib;
	SysBase	*SysBase;
	Task *WorkerTask;
};

static const struct TestBase TestLibData =
{
  .TestLib.lib_Node.ln_Name = (APTR)&name[0],
  .TestLib.lib_Node.ln_Type = NT_DEVICE,
  .TestLib.lib_Node.ln_Pri = -100,

  .TestLib.lib_OpenCnt = 0,
  .TestLib.lib_Flags = LIBF_SUMUSED|LIBF_CHANGED,
  .TestLib.lib_NegSize = 0,
  .TestLib.lib_PosSize = 0,
  .TestLib.lib_Version = DEVICE_VERSION,
  .TestLib.lib_Revision = DEVICE_REVISION,
  .TestLib.lib_Sum = 0,
  .TestLib.lib_IDString = (APTR)&version[0]
};

static volatile APTR FuncTab[] =
{
	(APTR) ((UINT32)-1)
};

static volatile const APTR InitTab[4]=
{
	(APTR)sizeof(struct TestBase),
	(APTR)FuncTab,  // Array of function (Library/Device)
	(APTR)&TestLibData,
	(APTR)test_Init
};


static volatile const struct Resident ROMTag =
{
	RTC_MATCHWORD,
	(struct Resident *)&ROMTag,
	(APTR)&EndResident,
	RTF_AUTOINIT | RTF_TESTCASE,
	DEVICE_VERSION,
	NT_DEVICE,
	-100,
	(char *)name,
	(char*)&version[0],
	0,
	&InitTab
};

void Launch_TestSuite0(void *SysBase)
{
	DPrintF("===== Start TestSuite0 =====\n");
}

void Switch_TestSuite0(void *SysBase)
{
	DPrintF("===== End TestSuite0 =====\n");
}

void Launch_TestSuite1(void *SysBase)
{
	DPrintF("===== Start TestSuite1 =====\n");
}

void Switch_TestSuite1(void *SysBase)
{
	DPrintF("===== End TestSuite1 =====\n");
}

static struct TestBase *test_Init(struct TestBase *TestBase, UINT32 *segList, struct SysBase *SysBase)
{
	TestBase->SysBase = SysBase;
	// Only initialise here, dont do long stuff, Multitasking is enabled. But we are running here with prio 100
	// For this we initialise a worker Task with Prio 0
	TestBase->WorkerTask = TaskCreate("TestSuite", test_TestTask, SysBase, 4096*2, 0); //8kb Stack should be enough
	Task *WorkerTask0 = TaskCreate("TestSuite0", test_TestTask0, SysBase, 4096*2, 0);
	WorkerTask0->Launch = Launch_TestSuite0;
	WorkerTask0->Switch = Switch_TestSuite0;
	Task *WorkerTask1 = TaskCreate("TestSuite1", test_TestTask1, SysBase, 4096*2, 0);
	WorkerTask1->Launch = Launch_TestSuite1;
	WorkerTask1->Switch = Switch_TestSuite1;
//	DPrintF("[INIT] Testinitialisation finished\n");
	return TestBase;
}

#define IsMsgPortEmpty(x) \
	( ((x)->mp_MsgList.lh_TailPred) == (struct Node *)(&(x)->mp_MsgList) )

#if 1
// WE NEED TO DO THIS BECAUSE OF WALL
struct InputEvent g_ie;

struct TestStruct {
	struct MsgPort *mp[10];
	struct IOStdReq *io[10];
	struct InputEvent ie[10];
};


static void hexdump(APTR SysBase, unsigned char *buf,int len)
{
	int cnt3,cnt4;
	int cnt=0;
	int cnt2=0;
	UINT8 *temp = buf;
	do
	{
		DPrintF("%08X | ", temp); //cnt);
		for (cnt3=0;cnt3<16;cnt3++)
		{
			if (cnt<len)
			{
				DPrintF("%02X ",buf[cnt++]);
				temp++;
			}
			else
				DPrintF("   ");
		}
		DPrintF("| ");
		for (cnt4=0;cnt4<cnt3;cnt4++)
		{
			if (cnt2==len)
				break;
			if (buf[cnt2]<0x20)
				DPrintF(".");
			else
				if (buf[cnt2]>0x7F && buf[cnt2]<0xC0)
					DPrintF(".");
				else
					DPrintF("%c",buf[cnt2]);
			cnt2++;
		}
		DPrintF("\n");
	}
	while (cnt!=len);
}

#include "alerts.h"

static void test_mouse(SysBase *SysBase)
{
	struct TestStruct *ts = AllocVec(sizeof(struct TestStruct), MEMF_CLEAR);

	if (ts != NULL)
	{
		INT32 ret = 0;
		struct MsgPort *mp= CreateMsgPort();
		for (int i=0 ; i<10; i++)
		{
			ts->mp[i] = mp;
			// No need for 10 ports ts->mp[i] = CreateMsgPort();
			ts->io[i] = CreateIORequest(mp, sizeof(struct IOStdReq));
			if ((ts->mp[i] == NULL)||(ts->io[i] == NULL))
			{
				DPrintF("Error allocating Port/Message at : %d\n", i);
				break;
			}
			ret = OpenDevice("mouseport.device", 0, (struct IORequest *)ts->io[i], 0);
			if (ret != 0)
			{
				DPrintF("OpenDevice mouseport.device failed!\n");
				break;
			}
			ts->io[i]->io_Command = MD_READEVENT; /* add a new request */
			ts->io[i]->io_Error = 0;
			ts->io[i]->io_Actual = 0;
			ts->io[i]->io_Data = &ts->ie[i];
			ts->io[i]->io_Flags = 0;
			ts->io[i]->io_Length = sizeof(struct InputEvent);
			ts->io[i]->io_Message.mn_Node.ln_Name = (STRPTR)i;
			SendIO((struct IORequest *) ts->io[i] );
			//DPrintF("io->flags = %b\n", ts->io[i]->io_Flags);
			if (ts->io[i]->io_Error > 0) DPrintF("Io Error [%d]\n", i);
		}

		int x = 0, y = 0;

		for(;;)
		{
			Wait(1<<mp->mp_SigBit);
			while (!IsMsgPortEmpty(mp))
			{
				//DPrintF("[ID:%x]", SysBase->IDNestCnt);
				struct IOStdReq *rcvd_io = (struct IOStdReq *)GetMsg(mp);
				struct InputEvent *rcvd_ie = (struct InputEvent *)rcvd_io->io_Data;
				DPrintF("Event Class %x (i= %x)[%d, %d]", rcvd_ie->ie_Class, rcvd_io->io_Message.mn_Node.ln_Name, rcvd_ie->ie_X, rcvd_ie->ie_Y);

				rcvd_io->io_Command = MD_READEVENT; /* add a new request */
				rcvd_io->io_Error = 0;
				rcvd_io->io_Actual = 0;
				//rcvd_io->io_Data = &ts->ie[i];
				rcvd_io->io_Flags = 0;
				rcvd_io->io_Length = sizeof(struct InputEvent);
		x+=rcvd_ie->ie_X;
		y-=rcvd_ie->ie_Y;
		if (x>640) x=640;
		if (x<0) x= 0;
		if (y>480) y=480;
		if (y<0) y= 0;
		DPrintF("x: %d, y: %d\n", x, y);
				SendIO((struct IORequest *) rcvd_io );
			}
		}

		//DPrintF("EventClass [%x]\n", ts->io[i].ie_Class);
	}
}

static void test_keyboard(SysBase *SysBase)
{
	struct TestStruct *ts = AllocVec(sizeof(struct TestStruct), MEMF_CLEAR);

	if (ts != NULL)
	{
		INT32 ret = 0;
		struct MsgPort *mp= CreateMsgPort();
		for (int i=0 ; i<10; i++)
		{
			ts->mp[i] = mp;
			ts->io[i] = CreateIORequest(mp, sizeof(struct IOStdReq));
			if ((ts->mp[i] == NULL)||(ts->io[i] == NULL))
			{
				DPrintF("Error allocating Port/Message at : %d\n", i);
				for(;;);
				break;
			}
			ret = OpenDevice("keyboard.device", 0, (struct IORequest *)ts->io[i], 0);
			if (ret != 0)
			{
				DPrintF("OpenDevice keyboard.device failed!\n");
				for(;;);
				break;
			}
			ts->io[i]->io_Command = KBD_READEVENT; /* add a new request */
			ts->io[i]->io_Error = 0;
			ts->io[i]->io_Actual = 0;
			ts->io[i]->io_Data = &ts->ie[i];
			ts->io[i]->io_Flags = 0;
			ts->io[i]->io_Length = sizeof(struct InputEvent);
			ts->io[i]->io_Message.mn_Node.ln_Name = (STRPTR)i;
			SendIO((struct IORequest *) ts->io[i] );
			//DPrintF("io->flags = %b\n", ts->io[i]->io_Flags);
			if (ts->io[i]->io_Error > 0)
			{
				DPrintF("Io Error [%d]\n", i);
			}
		}

		for(;;)
		{
			Wait(1<<mp->mp_SigBit);
			while (!IsMsgPortEmpty(mp))
			{
				//DPrintF("[ID:%x]", SysBase->IDNestCnt);
				struct IOStdReq *rcvd_io = (struct IOStdReq *)GetMsg(mp);
				struct InputEvent *rcvd_ie = (struct InputEvent *)rcvd_io->io_Data;
				DPrintF("Event Class %x (i= %x)\n", rcvd_ie->ie_Class, rcvd_io->io_Message.mn_Node.ln_Name);

				rcvd_io->io_Command = KBD_READEVENT; /* add a new request */
				rcvd_io->io_Error = 0;
				rcvd_io->io_Actual = 0;
				//rcvd_io->io_Data = &ts->ie[i];
				rcvd_io->io_Flags = 0;
				rcvd_io->io_Length = sizeof(struct InputEvent);

				SendIO((struct IORequest *) rcvd_io );
			}
		}

		//DPrintF("EventClass [%x]\n", ts->io[i].ie_Class);
	}
}

static void test_AlertTest(SysBase *SysBase)
{
	Alert(AN_MemCorrupt, "Memory defect -> Just kidding\n");
}



void test_virtio_blk(APTR SysBase);
static void test_virtio_blk_performance(SysBase *SysBase)
{
	struct MsgPort *mp=NULL;
	struct TimeRequest *io=NULL;
	struct TimeVal systime;
	struct Library *TimerBase;

	// We need a proper MsgPort to get Messages
	mp = CreateMsgPort();
	if (mp == NULL)
	{
		DPrintF("Couldnt create Message port (no Memory?)\n");
		goto end;
	}

	// Now we need an TimeRequest Structure
	io = CreateIORequest(mp, sizeof(struct TimeRequest));
	if (io == NULL)
	{
		DPrintF("Couldnt create TimeRequest (no Memory?)\n");
		goto end;
	}

	//UNIT_VBLANK works at 1/100sec and used for delays
	INT32 ret = OpenDevice("timer.device", UNIT_VBLANK, (struct IORequest *)io, 0);
	if (ret != 0)
	{
		DPrintF("OpenDevice Timer failed for UNIT_VBLANK!\n");
		goto end;
	}

	// set TimerBase
	TimerBase = (struct Library *)io->tr_node.io_Device;

	systime.tv_secs = 0;
	systime.tv_micro = 0;

	// Get current system time
	GetSysTime(&systime);
	DPrintF("now wall clock time is sec: %d\n", systime.tv_secs);
	DPrintF("now wall clock time is micro: %d\n", systime.tv_micro);

	// set wall clock time (seconds till today since 1 January 1970)
	io->tr_node.io_Command = TR_SETSYSTIME;
	io->tr_time.tv_secs = 5000;
	io->tr_time.tv_micro = 123;

	// call timer device to set wall clock time
	DPrintF("Setting wall clock time: 5000 sec 123 micro\n");
	DoIO((struct IORequest *) io );
	DPrintF("Setting wall clock time: done!\n");

	systime.tv_secs = 0;
	systime.tv_micro = 0;

	// Get current system time
	GetSysTime(&systime);
	DPrintF("now wall clock time is sec: %d\n", systime.tv_secs);
	DPrintF("now wall clock time is micro: %d\n", systime.tv_micro);

	test_virtio_blk(SysBase);

	// Get current system time
	GetSysTime(&systime);
	DPrintF("now wall clock time is sec: %d\n", systime.tv_secs);
	DPrintF("now wall clock time is micro: %d\n", systime.tv_micro);

	// Close Timer device
	DPrintF("Closing Timer Device UNIT_VBLANK\n");
	CloseDevice((struct IORequest *)io);
	DeleteIORequest((struct IORequest *)io);
	DeleteMsgPort(mp);

	mp = NULL;
	io = NULL;

end:

	if (io != NULL)
	{
		DeleteIORequest((struct IORequest *)io);
	}

	if (mp != NULL)
	{
		DeleteMsgPort(mp);
	}
}



static void test_Timer(SysBase *SysBase)
{
	struct MsgPort *mp=NULL;
	struct TimeRequest *io=NULL;
	struct TimeVal systime;
	struct Library *TimerBase;

	// We need a proper MsgPort to get Messages
	mp = CreateMsgPort();
	if (mp == NULL)
	{
		DPrintF("Couldnt create Message port (no Memory?)\n");
		goto end;
	}

	// Now we need an TimeRequest Structure
	io = CreateIORequest(mp, sizeof(struct TimeRequest));
	if (io == NULL)
	{
		DPrintF("Couldnt create TimeRequest (no Memory?)\n");
		goto end;
	}

	//UNIT_VBLANK works at 1/100sec and used for delays
	INT32 ret = OpenDevice("timer.device", UNIT_VBLANK, (struct IORequest *)io, 0);
	if (ret != 0)
	{
		DPrintF("OpenDevice Timer failed for UNIT_VBLANK!\n");
		goto end;
	}

	// set TimerBase
	TimerBase = (struct Library *)io->tr_node.io_Device;

	systime.tv_secs = 0;
	systime.tv_micro = 0;

	// Get current system time
	GetSysTime(&systime);
	DPrintF("now wall clock time is sec: %d\n", systime.tv_secs);
	DPrintF("now wall clock time is micro: %d\n", systime.tv_micro);

	// set wall clock time (seconds till today since 1 January 1970)
	io->tr_node.io_Command = TR_SETSYSTIME;
	io->tr_time.tv_secs = 5000;
	io->tr_time.tv_micro = 123;

	// call timer device to set wall clock time
	DPrintF("Setting wall clock time: 5000 sec 123 micro\n");
	DoIO((struct IORequest *) io );
	DPrintF("Setting wall clock time: done!\n");

	systime.tv_secs = 0;
	systime.tv_micro = 0;

	// Get current system time
	GetSysTime(&systime);
	DPrintF("now wall clock time is sec: %d\n", systime.tv_secs);
	DPrintF("now wall clock time is micro: %d\n", systime.tv_micro);

	// lets try a delay
	io->tr_node.io_Command = TR_ADDREQUEST;
	io->tr_time.tv_secs = 15;
	io->tr_time.tv_micro = 0;

	// post request to the timer device in sync way
	DPrintF("We will go 15 Seconds to sleep\n");
	DoIO((struct IORequest *) io );
	DPrintF("Return after 15 Seconds\n");

	// Close Timer device
	DPrintF("Closing Timer Device UNIT_VBLANK\n");
	CloseDevice((struct IORequest *)io);
	DeleteIORequest((struct IORequest *)io);
	DeleteMsgPort(mp);

	mp = NULL;
	io = NULL;

	// We need a proper MsgPort to get Messages
	mp = CreateMsgPort();
	if (mp == NULL)
	{
		DPrintF("Couldnt create Message port 2nd time (no Memory?)\n");
		goto end;
	}

	// Now we need an TimeRequest Structure
	io = CreateIORequest(mp, sizeof(struct TimeRequest));
	if (io == NULL)
	{
		DPrintF("Couldnt create TimeRequest 2nd time (no Memory?)\n");
		goto end;
	}

	//UNIT_WAITUNTIL is used for alarm
	ret = OpenDevice("timer.device", UNIT_WAITUNTIL, (struct IORequest *)io, 0);
	if (ret != 0)
	{
		DPrintF("OpenDevice Timer failed for UNIT_WAITUNTIL!\n");
		goto end;
	}

	// set TimerBase
	TimerBase = (struct Library *)io->tr_node.io_Device;

	// Get current system time
	GetSysTime(&systime);
	DPrintF("now wall clock time is sec: %d\n", systime.tv_secs);
	DPrintF("now wall clock time is micro: %d\n", systime.tv_micro);

	// lets set an alarm adding 2 minute to current wall clock time
	io->tr_node.io_Command = TR_ADDREQUEST;
	io->tr_time.tv_secs  = systime.tv_secs+120; // 2 minute = 120 sec
	io->tr_time.tv_micro = systime.tv_micro;
	DPrintF("We will go for a 2 minute alarm\n");
	DoIO((struct IORequest *) io);
	DPrintF("Return after 2 minute alarm\n");

	// Get current system time
	GetSysTime(&systime);
	DPrintF("after alarm, now wall clock time is sec: %d\n", systime.tv_secs);
	DPrintF("after alarm, now wall clock time is micro: %d\n", systime.tv_micro);

	// Close Timer device
	DPrintF("Closing Timer Device UNIT_WAITUNTIL\n");
	CloseDevice((struct IORequest *)io);

end:

	if (io != NULL)
	{
		DeleteIORequest((struct IORequest *)io);
	}

	if (mp != NULL)
	{
		DeleteMsgPort(mp);
	}
}

void test_mhz_delay(SysBase *SysBase, int sec)
{
	struct MsgPort *mp=NULL;
	struct TimeRequest *io=NULL;
	//struct TimeVal systime;
	//struct Library *TimerBase;

	// We need a proper MsgPort to get Messages
	mp = CreateMsgPort();
	if (mp == NULL)
	{
		DPrintF("Couldnt create Message port (no Memory?)\n");
		goto end;
	}

	// Now we need an TimeRequest Structure
	io = CreateIORequest(mp, sizeof(struct TimeRequest));
	if (io == NULL)
	{
		DPrintF("Couldnt create TimeRequest (no Memory?)\n");
		goto end;
	}

	//UNIT_MICROHZ works used for msec delays
	INT32 ret = OpenDevice("timer.device", UNIT_MICROHZ, (struct IORequest *)io, 0);
	if (ret != 0)
	{
		DPrintF("OpenDevice Timer failed for UNIT_MICROHZ!\n");
		goto end;
	}

	// lets try a delay
	io->tr_node.io_Command = TR_ADDREQUEST;
	io->tr_time.tv_secs = sec;
	io->tr_time.tv_micro = 0;

	// post request to the timer device in sync way
	DPrintF("We will go %d Seconds to sleep\n", sec);
	DoIO((struct IORequest *) io );
	DPrintF("Return after %d Seconds\n", sec);

	// Close Timer device
	DPrintF("Closing Timer Device UNIT_VBLANK\n");
	CloseDevice((struct IORequest *)io);
	DeleteIORequest((struct IORequest *)io);
	DeleteMsgPort(mp);

	mp = NULL;
	io = NULL;

end:

	if (io != NULL)
	{
		DeleteIORequest((struct IORequest *)io);
	}

	if (mp != NULL)
	{
		DeleteMsgPort(mp);
	}
}

static void test_printit(INT32 chr, APTR SysBase)
{
	RawPutChar(chr);
}

static void test_RawIO(SysBase *SysBase)
{
	RawIOInit();

	// Simple printf("hello...
	RawDoFmt("HELLO WORLD\n", NULL, test_printit, SysBase);

	UINT32 var[2] = {0x32, 0x10};

	// we emulate a command sequence printf("...", var1, var2);
	RawDoFmt("[%b] [%x]\n", (va_list) var, test_printit, SysBase);

	// single char output
	RawPutChar('H');
	RawPutChar('E');
	RawPutChar('L');
	RawPutChar('O');
	RawPutChar('\n');

	// Now test some input ->
	UINT8 chr = RawMayGetChar();
	while(chr != 'q') {
		// we only print chars received a-z and return
		if ((chr > 'a' && chr <'z') || chr=='\r') RawPutChar(chr);
		chr = RawMayGetChar();
	}
	Alert(AN_MemCorrupt, "End of Test RawIO\n");
}

static inline void memset32(void *dest, UINT32 value, UINT32 size) { asm volatile ("cld; rep stosl" : "+c" (size), "+D" (dest) : "a" (value) : "memory"); }

BOOL VmwSetVideoMode(UINT32 Width, UINT32 Height, UINT32 Bpp, SysBase *SysBase);

#include "vgagfx.h"
#include "vmware.h"

void test_vgagfx(APTR SysBase)
{
	VgaGfxBase *VgaGfxBase = OpenLibrary("vgagfx.library", 0);
	if (!VgaGfxBase) DPrintF("Failed to open library\n");

	//SVGA_SetDisplayMode(VgaGfxBase, 640, 480, 32);

//	SVGA_CopyRect(VgaGfxBase, 0, 0, 100, 100, 50, 50);
//	SVGA_FillRect(VgaGfxBase, 0xff00ff00, 5, 5, 30, 240);

	//SVGA_FifoUpdateFullscreen(VgaGfxBase);

//	SVGA_DrawHorzLine32(VgaGfxBase, 0, 640, 0, 0xffff0000, 0);
//	SVGA_DrawHorzLine32(VgaGfxBase, 0, 640, 479, 0xffff0000, 0);

//	for(int i= 0xFF000000;i<0xFFFFFFFF; i++) {
//		memset32(VgaGfxBase->fbDma, i, 640*20);
		//SVGA_FifoUpdateFullscreen(VgaGfxBase);
//	}

//	SVGA_FillRect(VgaGfxBase, i, 5, 5, 630, 240);

//	SVGA_FillRect(VgaGfxBase, 0xFFFF0000, 0, 0, 640, 10);
//	SVGA_FillRect(VgaGfxBase, 0xFFFF0000, 0, 470, 640, 10);
//	SVGA_FillRect(VgaGfxBase, 0xFFFF0000, 0, 0, 10, 480);
//	SVGA_FillRect(VgaGfxBase, 0xFFFF0000, 630, 0, 10, 480);

//	for(int i= 0xFF000000;i<0xFFFFFFFF; i++) SVGA_FillRect(VgaGfxBase, i, 5, 5, 630, 240);
}

#include "coregfx.h"
#include "pixmap.h"

void test_cgfx(APTR SysBase)
{
	APTR *CoreGfxBase = OpenLibrary("coregfx.library", 0);
	if (!CoreGfxBase) DPrintF("Failed to open library\n");

//	PixMap *pix = cgfx_AllocPixMap(CoreGfxBase, 200, 200, 32, NULL, NULL);
//	if (!pix) DPrintF("Failed to alloc pixmap\n");

}

#include "cursor.h"
#include "coregfx_funcs.h"
/*
static Cursor_T arrow = {	// default arrow cursor
	16, 16,
	0,  0,
	RGB(255, 255, 255), RGB(0, 0, 0),
	{ 0xe000, 0x9800, 0x8600, 0x4180,
	  0x4060, 0x2018, 0x2004, 0x107c,
	  0x1020, 0x0910, 0x0988, 0x0544,
	  0x0522, 0x0211, 0x000a, 0x0004 },
	{ 0xe000, 0xf800, 0xfe00, 0x7f80,
	  0x7fe0, 0x3ff8, 0x3ffc, 0x1ffc,
	  0x1fe0, 0x0ff0, 0x0ff8, 0x077c,
	  0x073e, 0x021f, 0x000e, 0x0004 }
};
*/
#include "coregfx.h"
#include "view.h"

struct ViewPort *cgfx_CreateVPort(CoreGfxBase *CoreGfxBase, PixMap *pix, INT32 xOffset, INT32 yOffset);
struct View *cgfx_CreateView(CoreGfxBase *CoreGfxBase, UINT32 nWidth, UINT32 nHeight, UINT32 bpp);
BOOL cgfx_MakeVPort(CoreGfxBase *CoreGfxBase, struct View *view, struct ViewPort *vp);
void cgfx_LoadView(CoreGfxBase *CoreGfxBase, struct View *view);
PixMap *cgfx_AllocPixMap(CoreGfxBase *CoreGfxBase, UINT32 width, UINT32 height, UINT32 format, UINT32 flags, APTR pixels, int palsize);
struct CRastPort *cgfx_InitRastPort(CoreGfxBase *CoreGfxBase, struct PixMap *bm);

struct InputEvent ie;

//	SetForegroundColor(rp, RGB(255, 0, 0));

void nxDraw3dBox(APTR CoreGfxBase, struct CRastPort *rp, int x, int y, int w, int h, UINT32 crTop, UINT32 crBottom)
{
	UINT32 old = SetForegroundColor(rp, crTop);
	Line(rp, x, y+h-2, x, y+1,TRUE);		/* left*/
	Line(rp, x, y, x+w-2, y,TRUE);			/* top*/

	SetForegroundColor(rp, crBottom);
	Line(rp, x+w-1, y, x+w-1, y+h-2,TRUE);		/* right*/
	Line(rp, x+w-1, y+h-1, x, y+h-1,TRUE);		/* bottom*/
	SetForegroundColor(rp, old);
}
/*
static void test_Arc(SysBase *SysBase, CoreGfxBase *CoreGfxBase, CRastPort *rp)
{
	int x = 40;
	int y = 40;
	int rx = 30;
	int ry = 30;
	int xoff = (rx + 10) * 2;

#if 1
	//filled arc
	SetForegroundColor(rp, RGB(0,250,0));
	ArcAngle(rp, x, y, 3, 3, 0, 0, PIE);

	SetForegroundColor(rp, RGB(0,0,0));
	ArcAngle(rp, x, y, 3, 3, 0, 0, ARC);
	Point(rp, x, y);

#else
	GrSetGCForeground(gc, GREEN);
	GrArc(wid, gc, x, y, rx, ry, 0, -30, -30, 0, GR_PIE);
	GrArc(wid, gc, x+5, y, rx, ry, 30, 0, 0, -30, GR_PIE);
	GrArc(wid, gc, x, y+5, rx, ry, -30, 0, 0, 30, GR_PIE);
	GrArc(wid, gc, x+5, y+5, rx, ry, 0, 30, 30, 0, GR_PIE);
#endif
	//outlined arc
	DPrintF("outlined arc\n");
	SetForegroundColor(rp, RGB(0,250,0));
	x += xoff;
	Arc(rp, x, y, rx, ry, 0, -30, -30, 0, ARCOUTLINE);
	Arc(rp, x+5, y, rx, ry, 30, 0, 0, -30, ARCOUTLINE);
	Arc(rp, x, y+5, rx, ry, -30, 0, 0, 30, ARCOUTLINE);
	Arc(rp, x+5, y+5, rx, ry, 0, 30, 30, 0, ARCOUTLINE);

	//arc only
	DPrintF("arc only\n");
	x += xoff;
	Arc(rp, x, y, rx, ry, 0, -30, -30, 0, ARC);
	Arc(rp, x+5, y, rx, ry, 30, 0, 0, -30, ARC);
	Arc(rp, x, y+5, rx, ry, -30, 0, 0, 30, ARC);
	Arc(rp, x+5, y+5, rx, ry, 0, 30, 30, 0, ARC);
}
*/
static UINT32 next = 1;
#define	RAND_MAX	0x7fffffff

int rand()
{
	return ((next = next * 1103515245 + 12345) % ((UINT32)RAND_MAX + 1));
}

void srand(UINT32 seed)
{
	next = seed;
}

#if 0
static void test_Ellipse(SysBase *SysBase, CoreGfxBase *CoreGfxBase, CRastPort *rp)
{
	INT32	x;
	INT32	y;
	INT32	rx;
	INT32	ry;
	UINT32	pixelval;

	x = rand() % 640;
	y = rand() % 480;
	rx = (rand() % 100) + 5;
	ry = (rx * 100) / 100;	/* make it appear circular */

	pixelval = rand();

	SetForegroundColor(rp, RGB(rand(),rand(),rand()));
	Ellipse(rp, x, y, rx, ry, TRUE);
//	for(int i=0; i<1000000;i++);
}
#endif

static void test_MousePointer(SysBase *SysBase)
{
/*
	INT32 xres = 1024;
	INT32 yres = 768;
//	UINT32 *temp = AllocVec(1280*2*1024, MEMF_FAST);
//	DPrintF ("Temp allocated\n");
	APTR CoreGfxBase = OpenLibrary("coregfx.library", 0);
	if (!CoreGfxBase) { DPrintF("Failed to open library\n"); return; }
	struct PixMap *pix	= cgfx_AllocPixMap(CoreGfxBase, xres, yres, IF_BGRA8888, FPM_Displayable, NULL,0 ); //IF_BGR888
DPrintF("AllocPixmap.......ok\n");
	struct CRastPort *rp = cgfx_InitRastPort(CoreGfxBase, pix);
	DPrintF("cgfx_AllocPixMap() = %x\n", pix->addr);
	SetForegroundColor(rp, RGB(150,150,150));

	pMemCHead node;
    struct MemHeader *mh=(struct MemHeader *)SysBase->MemList.lh_Head;

	ForeachNode(&mh->mh_ListUsed, node)
	{
		Task *task = node->mch_Task;
		DPrintF("Used Memory at %x, size %x, task [%s]\n", node, node->mch_Size, task->Node.ln_Name);
	}

	ForeachNode(&mh->mh_List, node)
	{
		DPrintF("Free Memory at %x, size %x\n", node, node->mch_Size);
	}

	FillRect(rp, 0, 0, xres, yres);
//	if (pix) memset32(pix->addr, 0x0, pix->size/4);

	DPrintF("cgfx_CreateView()\n");
	struct View *view	= cgfx_CreateView(CoreGfxBase, xres, yres, 32);//24);
	struct ViewPort *vp = cgfx_CreateVPort(CoreGfxBase, pix, 0, 0);
	cgfx_MakeVPort(CoreGfxBase, view, vp);
	DPrintF("LoadView()\n");
	cgfx_LoadView(CoreGfxBase, view);

	//UINT32 size = view->width * view->height;
	//UINT32 *fb = (UINT32*)view->fbAddr;
	//for (UINT32 i = 0; i< size; i++) fb[i] = 0xFFFF0000;

//	SetForegroundColor(rp, RGB(255,0,0));
//	FillRect(rp, 0, 0, xres, yres);
//	hexdump(SysBase, view->fbAddr + 0x10000, 0x200);
//		memset32(view->fbAddr, 0xffffffff, view->width * view->height);


//DPrintF("Fill Screen\n");
//for(;;);

nxDraw3dBox(CoreGfxBase, rp, 50, 50, 200, 200, RGB(162, 141, 104), RGB(234, 230, 221));
nxDraw3dBox(CoreGfxBase, rp, 51, 51, 198, 198, RGB(  0,   0,   0), RGB(213, 204, 187));

test_Arc(SysBase, CoreGfxBase, rp);
//for(int i =0 ; i<1000; i++) test_Ellipse(SysBase, CoreGfxBase, rp);

	DPrintF("coregfx: %x\n", CoreGfxBase);
	INT32 x=0, y=0;
	DPrintF("Movecursor\n");
	MoveCursor(x,y);
	DPrintF("Setcursor\n");
	SetCursor(&arrow);
	DPrintF("Showcursor\n");
	ShowCursor();
	DPrintF("ShowcursorEnd\n");
	INT32 ret = 0;
	struct MsgPort *mp	= CreateMsgPort();
	struct IOStdReq *io	= CreateIORequest(mp, sizeof(struct IOStdReq));;

	ret = OpenDevice("mouseport.device", 0, (struct IORequest *)io, 0);
	if (ret != 0)
	{
		DPrintF("OpenDevice mouseport.device failed!\n");
		return;
	}

	io->io_Command = MD_READEVENT; //add a new request
	io->io_Error = 0;
	io->io_Actual = 0;
	io->io_Data = &ie;
	io->io_Flags = 0;
	io->io_Length = sizeof(struct InputEvent);
	io->io_Message.mn_Node.ln_Name = (STRPTR)0;
	DPrintF("Doio\n");
	DoIO((struct IORequest *) io );
	if (io->io_Error > 0) DPrintF("Io Error [%d]\n", 0);
//	DPrintF("GotIO\n");


	for(;;)
	{
		struct IOStdReq *rcvd_io = io;
//		struct InputEvent *rcvd_ie = (struct InputEvent *)rcvd_io->io_Data;
		//DPrintF("Event Class %x (i= %x)[%d, %d](%x/%x)  --- ", rcvd_ie->ie_Class, rcvd_io->io_Message.mn_Node.ln_Name, rcvd_ie->ie_X, rcvd_ie->ie_Y, rcvd_ie, &ie);

		rcvd_io->io_Command = MD_READEVENT; //add a new request
		rcvd_io->io_Error = 0;
		rcvd_io->io_Actual = 0;
		rcvd_io->io_Data = &ie;
		rcvd_io->io_Flags = 0;
		rcvd_io->io_Length = sizeof(struct InputEvent);

		x+=ie.ie_X;
		y-=ie.ie_Y;
		if (x>xres) x=xres;
		if (x<0) x= 0;
		if (y>yres) y=yres;
		if (y<0) y= 0;
		MoveCursor(x,y);
		//DPrintF("x:%d, y:%d \n",x, y);
//for(;;);
		DoIO((struct IORequest *) io );
	}
*/
}

struct InputEvent *inputcode(struct InputEvent *ie, struct SysBase *SysBase)
{
	if (ie->ie_Class != IECLASS_TIMER)	DPrintF("rcvd ieclass: %d", ie->ie_Class);
	return ie;
}

#include "inputdev.h"

struct Interrupt input;

static void test_InputDev(struct SysBase *SysBase)
{
	INT32 ret = 0;
	struct MsgPort *mp	= CreateMsgPort();
	struct IOStdReq *io	= CreateIORequest(mp, sizeof(struct IOStdReq));;

	ret = OpenDevice("input.device", 0, (struct IORequest *)io, 0);
	if (ret != 0)
	{
		DPrintF("OpenDevice input.device failed!\n");
		return;
	}
	input.is_Data = SysBase;
	input.is_Code = (void *(*)())inputcode;
	input.is_Node.ln_Name = "Testinput";
	input.is_Node.ln_Pri = 50;
	io->io_Command = IND_ADDHANDLER; /* add a new request */
	io->io_Error = 0;
	io->io_Actual = 0;
	io->io_Data = &input;
	io->io_Flags = 0;
	io->io_Length = 0;//sizeof(struct InputEvent);
	DoIO((struct IORequest *)io);
}

void d_showtask(struct SysBase *SysBase)
{
	struct Task *dev;
	DPrintF("PowerOS Registered Tasks :\n\n");
	dev = FindTask(NULL);
	DPrintF("Run ----------------------------\n");
	DPrintF("Name : %s\n",dev->Node.ln_Name);
	DPrintF("Prio : %d\n",dev->Node.ln_Pri);
	DPrintF("Type : %X\n",dev->Node.ln_Type);

	DPrintF("Ready --------------------------\n");
	ForeachNode(&SysBase->TaskReady,dev)
	{
		DPrintF("Name : %s\n",dev->Node.ln_Name);
		DPrintF("Prio : %d\n",dev->Node.ln_Pri);
		DPrintF("Type : %X\n",dev->Node.ln_Type);
	}
	DPrintF("Wait  --------------------------\n");
	ForeachNode(&SysBase->TaskWait,dev)
	{
		DPrintF("Name : %s\n",dev->Node.ln_Name);
		DPrintF("Prio : %d\n",dev->Node.ln_Pri);
		DPrintF("Type : %X\n",dev->Node.ln_Type);
	}
}

void d_showint(int addr, struct SysBase *SysBase)
{
	struct Interrupt *irq;
	if (addr >= 16) return;
	DPrintF("Interrupt [%X] :\n",addr);
	ForeachNode(&SysBase->IntVectorList[addr],irq)
	{
		DPrintF("-------------------------------\n");
		DPrintF("Addr : %x\n",&irq->is_Node);
		DPrintF("Name : %s\n",irq->is_Node.ln_Name);
		DPrintF("Prio : %d\n",irq->is_Node.ln_Pri);
		DPrintF("Type : %X\n",irq->is_Node.ln_Type);
		DPrintF("Funct: %x\n",irq->is_Code);
  }
}
#endif
void test_new_memory();
void test_library();
void DetectVirtio(APTR SysBase);

void test_virtio_blk(APTR SysBase)
{
	struct MsgPort *mp=NULL;
	struct IOStdReq *io=NULL;

	// We need a proper MsgPort to get Messages
	mp = CreateMsgPort();
	if (mp == NULL)
	{
		DPrintF("Couldnt create Message port (no Memory?)\n");
		goto end;
	}

	// Now we need a VirtioBlkRequest Structure
	io = CreateIORequest(mp, sizeof(struct IOStdReq));
	if (io == NULL)
	{
		DPrintF("Couldnt create IOStdReq (no Memory?)\n");
		goto end;
	}

	//lets open the device
	INT32 ret = OpenDevice("virtio_blk.device", 0, (struct IORequest *)io, 0);
	if (ret != 0)
	{
		DPrintF("OpenDevice virtio_blk failed for virtio_blk unit 0\n");
		goto end;
	}

	// lets try to read the device

	//number of sectors to read/write at once
	#define NUM_SECTORS 69

	UINT8* buf = AllocVec(512*NUM_SECTORS, MEMF_PUBLIC);
	if (buf == NULL)
	{
		DPrintF("ERROR: No Buffer\n");
	}

	int j = NUM_SECTORS;
	for(int i = 0; i < 64*15; i+=j) //8192 max
	{
		// lets try a to read or write some sectors from the device

		io->io_Offset = i;
		io->io_Length = NUM_SECTORS*512;

		//to READ sectors, enable below two lines
		io->io_Command = CMD_READ;
		memset(buf, 0, 512*NUM_SECTORS);

		//to WRITE sectors, enable below two lines
		//io->io_Command = CMD_WRITE;
		//for(int c=0; c<j; c++){memset(buf+(512*c), (UINT8)(i+c), 512);}

		io->io_Data = buf;

		// post request to the virtio device in sync way
		DPrintF("We will read/write a sector from the device\n");
		DoIO((struct IORequest *) io );
		DPrintF("Return after reading/writing a sector from the device\n");

		for(int k=0; k<j; k++)
		{
			DPrintF("buf[512*%d+0]= %x\n", k, buf[512*k+0]);
		}
	}

	FreeVec(buf);

	// Close device
	DPrintF("Closing virtio_blk Device 0\n");
	CloseDevice((struct IORequest *)io);
	DeleteIORequest((struct IORequest *)io);
	DeleteMsgPort(mp);

	mp = NULL;
	io = NULL;

end:

	if (io != NULL)
	{
		DeleteIORequest((struct IORequest *)io);
	}

	if (mp != NULL)
	{
		DeleteMsgPort(mp);
	}
}

static void test_TestTask(APTR data, struct SysBase *SysBase)
{
	DPrintF("TestTask_________________________________________________\n");

	DPrintF("Binary  Output: %b\n", 0x79);
	DPrintF("Hex     Output: %x\n", 0x79);
	DPrintF("Decimal Output: %d\n", 0x79);

	DPrintF("---------------------------------------------\n");
	pMemCHead node;
    struct MemHeader *mh=(struct MemHeader *)SysBase->MemList.lh_Head;

	ForeachNode(&mh->mh_ListUsed, node)
	{
		Task *task = node->mch_Task;
		DPrintF("Used Memory at %x, size %x, task [%s]\n", node, node->mch_Size, task->Node.ln_Name);
	}
	DPrintF("---------------------------------------------\n");

	ForeachNode(&mh->mh_List, node)
	{
		DPrintF("Free Memory at %x, size %x\n", node, node->mch_Size);
	}

	DPrintF("---------------------------------------------\n");
	DPrintF("Largest Chunk Memory Available : %x (%d)\n", AvailMem(MEMF_FAST|MEMF_LARGEST), AvailMem(MEMF_FAST|MEMF_LARGEST));
	DPrintF("Free Memory Available          : %x (%d)\n", AvailMem(MEMF_FAST|MEMF_FREE), AvailMem(MEMF_FAST|MEMF_FREE));
	DPrintF("Total Memory Available         : %x (%d)\n", AvailMem(MEMF_FAST|MEMF_TOTAL), AvailMem(MEMF_FAST|MEMF_TOTAL));

	DPrintF("SysBase %x\n", SysBase);
	DPrintF("SysBase->IDNestcnt %x\n", SysBase->IDNestCnt);

	goto out;

	test_virtio_blk_performance(SysBase);
	test_virtio_blk(SysBase);
	DetectVirtio(SysBase);
	test_mhz_delay(SysBase, 5);
	test_Timer(SysBase);
	test_library(SysBase);
	test_MousePointer(SysBase);
	//test_cgfx(SysBase);
	test_mouse(SysBase);
	test_keyboard(SysBase);
	test_AlertTest(SysBase);
	test_RawIO(SysBase);
	//VmwSetVideoMode(800, 600, 32, SysBase);
	//d_showtask(SysBase);
	//memset32((APTR)0x230000, 0x00, 0x200000);
	hexdump(SysBase, 0x0, 100);
	test_InputDev(SysBase);
	//test_new_memory();
out:
	DPrintF("[TESTTASK] Finished, we are leaving... bye bye... till next reboot\n");
}


//==========concurrency test===========

void test_virtio_blk0(APTR SysBase);
void test_virtio_blk1(APTR SysBase);

static void test_TestTask0(APTR data, struct SysBase *SysBase)
{
	test_virtio_blk0(SysBase);
	DPrintF("[TESTTASK0] Finished, we are leaving... bye bye... till next reboot\n");
}
static void test_TestTask1(APTR data, struct SysBase *SysBase)
{
	test_virtio_blk1(SysBase);
	DPrintF("[TESTTASK1] Finished, we are leaving... bye bye... till next reboot\n");
}

void test_virtio_blk0(APTR SysBase)
{
	struct MsgPort *mp=NULL;
	struct IOStdReq *io=NULL;

	// We need a proper MsgPort to get Messages
	mp = CreateMsgPort();
	if (mp == NULL)
	{
		DPrintF("Couldnt create Message port (no Memory?)\n");
		goto end;
	}

	// Now we need a VirtioBlkRequest Structure
	io = CreateIORequest(mp, sizeof(struct IOStdReq));
	if (io == NULL)
	{
		DPrintF("Couldnt create IOStdReq (no Memory?)\n");
		goto end;
	}

	//lets open the device
	INT32 ret = OpenDevice("virtio_blk.device", 0, (struct IORequest *)io, 0);
	if (ret != 0)
	{
		DPrintF("OpenDevice virtio_blk failed for virtio_blk unit 0\n");
		goto end;
	}

	// lets try to read the device

	//number of sectors to read/write at once
	#define NUM_SECTORS0 1

	UINT8* buf = AllocVec(512*NUM_SECTORS0, MEMF_PUBLIC);
	if (buf == NULL)
	{
		DPrintF("ERROR: No Buffer\n");
	}

	int j = NUM_SECTORS0;
	for(int i = 0; i < 64*15; i+=j) //8192 max
	{
		// lets try a to read or write some sectors from the device

		io->io_Offset = i;
		io->io_Length = NUM_SECTORS0*512;

		//to READ sectors, enable below two lines
		io->io_Command = CMD_READ;
		memset(buf, 0, 512*NUM_SECTORS0);

		//to WRITE sectors, enable below two lines
		//io->io_Command = CMD_WRITE;
		//for(int c=0; c<j; c++){memset(buf+(512*c), (UINT8)(i+c), 512);}

		io->io_Data = buf;

		// post request to the virtio device in sync way
		DPrintF("We will read/write a sector from the device\n");
		DoIO((struct IORequest *) io );
		DPrintF("Return after reading/writing a sector from the device\n");

		for(int k=0; k<j; k++)
		{
			DPrintF("buf[512*%d+0]= %x\n", k, buf[512*k+0]);
		}
		test_mhz_delay(SysBase, 1);
	}

	FreeVec(buf);

	// Close device
	DPrintF("Closing virtio_blk Device 0\n");
	CloseDevice((struct IORequest *)io);
	DeleteIORequest((struct IORequest *)io);
	DeleteMsgPort(mp);

	mp = NULL;
	io = NULL;

end:

	if (io != NULL)
	{
		DeleteIORequest((struct IORequest *)io);
	}

	if (mp != NULL)
	{
		DeleteMsgPort(mp);
	}
}

void test_virtio_blk1(APTR SysBase)
{
	struct MsgPort *mp=NULL;
	struct IOStdReq *io=NULL;

	// We need a proper MsgPort to get Messages
	mp = CreateMsgPort();
	if (mp == NULL)
	{
		DPrintF("Couldnt create Message port (no Memory?)\n");
		goto end;
	}

	// Now we need a VirtioBlkRequest Structure
	io = CreateIORequest(mp, sizeof(struct IOStdReq));
	if (io == NULL)
	{
		DPrintF("Couldnt create IOStdReq (no Memory?)\n");
		goto end;
	}

	//lets open the device
	INT32 ret = OpenDevice("virtio_blk.device", 0, (struct IORequest *)io, 0);
	if (ret != 0)
	{
		DPrintF("OpenDevice virtio_blk failed for virtio_blk unit 0\n");
		goto end;
	}

	// lets try to read the device

	//number of sectors to read/write at once
	#define NUM_SECTORS1 1

	UINT8* buf = AllocVec(512*NUM_SECTORS1, MEMF_PUBLIC);
	if (buf == NULL)
	{
		DPrintF("ERROR: No Buffer\n");
	}

	int j = NUM_SECTORS1;
	for(int i = 0; i < 64*15; i+=j) //8192 max
	{
		// lets try a to read or write some sectors from the device

		io->io_Offset = i;
		io->io_Length = NUM_SECTORS1*512;

		//to READ sectors, enable below two lines
		io->io_Command = CMD_READ;
		memset(buf, 0, 512*NUM_SECTORS1);

		//to WRITE sectors, enable below two lines
		//io->io_Command = CMD_WRITE;
		//for(int c=0; c<j; c++){memset(buf+(512*c), (UINT8)(i+c), 512);}

		io->io_Data = buf;

		// post request to the virtio device in sync way
		DPrintF("We will read/write a sector from the device\n");
		DoIO((struct IORequest *) io );
		DPrintF("Return after reading/writing a sector from the device\n");

		for(int k=0; k<j; k++)
		{
			DPrintF("buf[512*%d+0]= %x\n", k, buf[512*k+0]);
		}
		test_mhz_delay(SysBase, 2);
	}

	FreeVec(buf);

	// Close device
	DPrintF("Closing virtio_blk Device 0\n");
	CloseDevice((struct IORequest *)io);
	DeleteIORequest((struct IORequest *)io);
	DeleteMsgPort(mp);

	mp = NULL;
	io = NULL;

end:

	if (io != NULL)
	{
		DeleteIORequest((struct IORequest *)io);
	}

	if (mp != NULL)
	{
		DeleteMsgPort(mp);
	}
}
