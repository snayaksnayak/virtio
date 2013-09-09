// this file shall go to top most include folder in future

#ifndef lib_cache_h
#define lib_cache_h

#include "types.h"

typedef struct LibCacheBase
{
	struct Library		Library;
	APTR				SysBase;

} LibCacheBase;


//library functions
void CacheConfigure(LibCacheBase *LibCacheBase);
void CacheRead(LibCacheBase *LibCacheBase);
void CacheWrite(LibCacheBase *LibCacheBase);
void CacheDiscard(LibCacheBase *LibCacheBase);
void CacheSync(LibCacheBase *LibCacheBase);


//vectors
#define CacheConfigure(a,b,c) (((void(*)(APTR, UINT16, UINT16, UINT8)) 	_GETVECADDR(LibCacheBase, 5))(LibCacheBase, a, b, c))
#define CacheRead(a,b,c) (((void(*)(APTR, UINT16, UINT16, UINT32)) 	_GETVECADDR(LibCacheBase, 6))(LibCacheBase, a, b, c))
#define CacheWrite(a,b) (((UINT8(*)(APTR, UINT16, UINT16)) 	_GETVECADDR(LibCacheBase, 7))(LibCacheBase, a, b))
#define CacheDiscard(a,b) (((UINT16(*)(APTR, UINT16, UINT16)) 	_GETVECADDR(LibCacheBase, 8))(LibCacheBase, a, b))
#define CacheSync(a) (((int(*)(APTR, VirtioDevice*)) 	_GETVECADDR(LibCacheBase, 9))(LibCacheBase, a))


#endif //lib_cache_h
