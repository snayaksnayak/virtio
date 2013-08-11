#include "types.h"
#include "sysbase.h"
#include "resident.h"
#include "exec_funcs.h"

static const char name[] = "mylib.library";
static const char version[] = "mylib 0.1\n";
static const char EndResident;
#define LIBRARY_VERSION 0
#define LIBRARY_REVISION 1

#define add(a,b) (((INT32(*)(APTR, INT32, INT32)) _GETVECADDR(MyLibBase, 5))(MyLibBase, a, b))
#define sub(a,b) (((INT32(*)(APTR, INT32, INT32)) _GETVECADDR(MyLibBase, 6))(MyLibBase, a, b))

struct mylib
{
	struct Library lib;
	SysBase* SysBase;
	int ver;
	int rev;
	int add;
	int sub;
};

int mylib_add(struct mylib* mylib_p, int a, int b);
int mylib_sub(struct mylib* mylib_p, int a, int b);

static const struct mylib mylib_data =
{
  .lib.lib_Node.ln_Name = (APTR)&name[0],
  .lib.lib_Node.ln_Type = NT_LIBRARY,
  .lib.lib_Node.ln_Pri = -97,

  .lib.lib_OpenCnt = 0,
  .lib.lib_Flags = LIBF_SUMUSED|LIBF_CHANGED,
  .lib.lib_NegSize = 0,
  .lib.lib_PosSize = 0,
  .lib.lib_Version = LIBRARY_VERSION,
  .lib.lib_Revision = LIBRARY_REVISION,
  .lib.lib_Sum = 0,
  .lib.lib_IDString = (APTR)&version[0],

  .ver = 8,
  .rev = 9,
  .add = 6,
  .sub = 7
};

APTR mylib_OpenLib(struct mylib* mylib_p)
{
	mylib_p->lib.lib_OpenCnt++;
	return mylib_p;
}

APTR mylib_CloseLib(struct mylib* mylib_p)
{
	mylib_p->lib.lib_OpenCnt--;
	return mylib_p;
}

APTR mylib_ExpungeLib(struct mylib* mylib_p)
{
	return mylib_p;
}

APTR mylib_ExtFuncLib(struct mylib* mylib_p)
{
	return mylib_p;
}

int mylib_add(struct mylib* mylib_p, int a, int b)
{
	APTR SysBase = mylib_p->SysBase;
	DPrintF("add: mylib sysbase is: %p\n", mylib_p->SysBase);
	mylib_p->add = a+b;
	return mylib_p->add;
}

int mylib_sub(struct mylib* mylib_p, int a, int b)
{
	APTR SysBase = mylib_p->SysBase;
	DPrintF("sub: mylib sysbase is: %p\n", SysBase);
	mylib_p->sub = a-b;
	return mylib_p->sub;
}

static volatile APTR mylib_func_table[] =
{
	(void(*)) mylib_OpenLib,
	(void(*)) mylib_CloseLib,
	(void(*)) mylib_ExpungeLib,
	(void(*)) mylib_ExtFuncLib,

	(void(*)) mylib_add,
	(void(*)) mylib_sub,
	(APTR) ((UINT32)-1)
};

struct mylib* mylib_init(struct mylib* mylib_base, UINT32 *segList, struct SysBase *SysBase)
{
	mylib_base->SysBase = SysBase;
	DPrintF("mylib sysbase is: %p\n", mylib_base->SysBase);
	return mylib_base;
}

static volatile const APTR init_table[4]=
{
	(APTR)sizeof(struct mylib),
	(APTR)mylib_func_table,
	(APTR)&mylib_data,
	(APTR)mylib_init
};


static volatile const struct Resident mylib_rom_tag =
{
	RTC_MATCHWORD,
	(struct Resident *)&mylib_rom_tag,
	(APTR)&EndResident,
	RTF_TESTCASE,
	LIBRARY_VERSION,
	NT_LIBRARY,
	-97,
	(char *)name,
	(char*)&version[0],
	0,
	&init_table
};


void test_library(APTR SysBase)
{
	struct Library *library = NULL;

	library = MakeLibrary((APTR)mylib_func_table, (APTR)&mylib_data, (APTR)mylib_init, sizeof(struct mylib),0);
	if(library != NULL)
	{
		AddLibrary(library);
		DPrintF("mylib addlib done\n");
		struct Library* MyLibBase = NULL;
		MyLibBase = OpenLibrary("mylib.library", 0);
		if(MyLibBase != 0)
		{
			DPrintF("after open, mylib ptr is: %p\n", MyLibBase);
			int c = add(2, 3);
			DPrintF("after add, answer is: %d\n", c);
			c = sub(9, 7);
			DPrintF("after sub, answer is: %d\n", c);
		}
		CloseLibrary(MyLibBase);
	}
	RemLibrary(library);
	DisposeLibrary(library);
}
