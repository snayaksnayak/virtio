// this file shall go to top most include folder in future

#ifndef lib_cache_h
#define lib_cache_h

#include "types.h"

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


//library functions
BOOL CacheConfigure(LibCacheBase *LibCacheBase, struct LibCacheConfig* Config);
UINT32 CacheRead(LibCacheBase *LibCacheBase, UINT32 block_offset, UINT32 byte_length, UINT8* data_ptr);
UINT32 CacheWrite(LibCacheBase *LibCacheBase, UINT32 block_offset, UINT32 byte_length, UINT8* data_ptr);
void CacheDiscard(LibCacheBase *LibCacheBase);
void CacheSync(LibCacheBase *LibCacheBase);
BOOL CacheHit(LibCacheBase *LibCacheBase, UINT32 block_offset, UINT32 byte_length);
BOOL CacheDirty(LibCacheBase *LibCacheBase);

//vectors
#define CacheConfigure(a) (((BOOL(*)(APTR, struct LibCacheConfig*)) 	_GETVECADDR(LibCacheBase, 5))(LibCacheBase, a))
#define CacheRead(a,b,c) (((UINT32(*)(APTR, UINT32, UINT32, UINT8*)) 	_GETVECADDR(LibCacheBase, 6))(LibCacheBase, a, b, c))
#define CacheWrite(a,b,c) (((UINT32(*)(APTR, UINT32, UINT32, UINT8*)) 	_GETVECADDR(LibCacheBase, 7))(LibCacheBase, a, b, c))
#define CacheDiscard() (((void(*)(APTR)) 	_GETVECADDR(LibCacheBase, 8))(LibCacheBase))
#define CacheSync() (((void(*)(APTR)) 	_GETVECADDR(LibCacheBase, 9))(LibCacheBase))
#define CacheHit(a,b) (((BOOL(*)(APTR, UINT32, UINT32)) 	_GETVECADDR(LibCacheBase, 10))(LibCacheBase, a, b))
#define CacheDirty() (((BOOL(*)(APTR)) 	_GETVECADDR(LibCacheBase, 11))(LibCacheBase))


#endif //lib_cache_h
