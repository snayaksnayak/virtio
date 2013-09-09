#ifndef lib_cache_internal_h
#define lib_cache_internal_h

#include "types.h"
#include "library.h"
#include "arch_config.h"

#define LIB_CACHE_VERSION  0
#define LIB_CACHE_REVISION 1

#define LIB_CACHE_LIBNAME "lib_cache"
#define LIB_CACHE_LIBVER  " 0.1 __DATE__"

typedef struct LibCacheBase
{
	struct Library		Library;
	APTR				SysBase;

} LibCacheBase;


extern char LibCacheLibName[];
extern char LibCacheLibVer[];


//functions
struct LibCacheBase *lib_cache_OpenLib(struct LibCacheBase *LibCacheBase);
APTR lib_cache_CloseLib(struct LibCacheBase *LibCacheBase);
APTR lib_cache_ExpungeLib(struct LibCacheBase *LibCacheBase);
APTR lib_cache_ExtFuncLib(void);

void lib_cache_Configure();
void lib_cache_Read();
void lib_cache_Write();
void lib_cache_Discard();
void lib_cache_Sync();


//internals
void LibCache_supports(LibCacheBase *LibCacheBase);


//outer name (vectors) of our own library functions,
//we can use them inside our library too.
#define CacheConfigure(a,b,c) (((void(*)(APTR, UINT16, UINT16, UINT8)) 	_GETVECADDR(LibCacheBase, 5))(LibCacheBase, a, b, c))
#define CacheRead(a,b,c) (((void(*)(APTR, UINT16, UINT16, UINT32)) 	_GETVECADDR(LibCacheBase, 6))(LibCacheBase, a, b, c))
#define CacheWrite(a,b) (((UINT8(*)(APTR, UINT16, UINT16)) 	_GETVECADDR(LibCacheBase, 7))(LibCacheBase, a, b))
#define CacheDiscard(a,b) (((UINT16(*)(APTR, UINT16, UINT16)) 	_GETVECADDR(LibCacheBase, 8))(LibCacheBase, a, b))
#define CacheSync(a) (((int(*)(APTR, VirtioDevice*)) 	_GETVECADDR(LibCacheBase, 9))(LibCacheBase, a))


#endif //lib_cache_internal_h


