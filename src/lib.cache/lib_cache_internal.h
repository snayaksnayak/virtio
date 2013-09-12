#ifndef lib_cache_internal_h
#define lib_cache_internal_h

#include "types.h"
#include "library.h"
#include "arch_config.h"

#define LIB_CACHE_VERSION  0
#define LIB_CACHE_REVISION 1

#define LIB_CACHE_LIBNAME "lib_cache"
#define LIB_CACHE_LIBVER  " 0.1 __DATE__"

//flags for cache
#define CF_CLEAN      (0)
#define CF_DIRTY      (1<<0)
#define CF_INVALID    (1<<1)

struct LibCacheConfig
{
	UINT32 CacheNumBlocks; //number of blocks in the cache
	UINT32 BlockSize; //size of one block in bytes
	UINT32 SourceNumBlocks; //number of blocks in the source
	APTR UserBase;
	APTR ReadSourceCallback; //callback function for read from source
	APTR WriteSourceCallback; //callback function for write to source
};

typedef struct LibCacheBase
{
	struct Library		Library;
	APTR				SysBase;

	struct LibCacheConfig CacheConfig;

	APTR CacheBuffer; //points to data of whole cache
	UINT32 CacheFlag; //maintains state of cache, dirty etc.
	UINT32 CacheNum; //which block starts on cache

} LibCacheBase;


extern char LibCacheLibName[];
extern char LibCacheLibVer[];


//functions
struct LibCacheBase *lib_cache_OpenLib(LibCacheBase *LibCacheBase);
APTR lib_cache_CloseLib(LibCacheBase *LibCacheBase);
APTR lib_cache_ExpungeLib(LibCacheBase *LibCacheBase);
APTR lib_cache_ExtFuncLib(void);

BOOL lib_cache_Configure(LibCacheBase *LibCacheBase, struct LibCacheConfig* Config);
UINT32 lib_cache_Read(LibCacheBase *LibCacheBase, UINT32 block_offset, UINT32 byte_length, UINT8* data_ptr);
UINT32 lib_cache_Write(LibCacheBase *LibCacheBase, UINT32 block_offset, UINT32 byte_length, UINT8* data_ptr);
void lib_cache_Discard(LibCacheBase *LibCacheBase);
void lib_cache_Sync(LibCacheBase *LibCacheBase);
BOOL lib_cache_Hit(LibCacheBase *LibCacheBase, UINT32 block_offset, UINT32 byte_length);
BOOL lib_cache_Dirty(LibCacheBase *LibCacheBase);


//internals
void LibCache_internal_function(LibCacheBase *LibCacheBase);


//outer name (vectors) of our own library functions,
//we can use them inside our library too.
#define CacheConfigure(a) (((BOOL(*)(APTR, struct LibCacheConfig*)) 	_GETVECADDR(LibCacheBase, 5))(LibCacheBase, a))
#define CacheRead(a,b,c) (((UINT32(*)(APTR, UINT32, UINT32, UINT8*)) 	_GETVECADDR(LibCacheBase, 6))(LibCacheBase, a, b, c))
#define CacheWrite(a,b,c) (((UINT32(*)(APTR, UINT32, UINT32, UINT8*)) 	_GETVECADDR(LibCacheBase, 7))(LibCacheBase, a, b, c))
#define CacheDiscard() (((void(*)(APTR)) 	_GETVECADDR(LibCacheBase, 8))(LibCacheBase))
#define CacheSync() (((void(*)(APTR)) 	_GETVECADDR(LibCacheBase, 9))(LibCacheBase))
#define CacheHit(a,b) (((BOOL(*)(APTR, UINT32, UINT32)) 	_GETVECADDR(LibCacheBase, 10))(LibCacheBase, a, b))
#define CacheDirty() (((BOOL(*)(APTR)) 	_GETVECADDR(LibCacheBase, 11))(LibCacheBase))


#endif //lib_cache_internal_h


