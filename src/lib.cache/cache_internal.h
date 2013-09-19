#ifndef cache_internal_h
#define cache_internal_h

#include "types.h"
#include "library.h"
#include "arch_config.h"

#define CACHE_VERSION  0
#define CACHE_REVISION 1

//flags for cache
#define CF_CLEAN      (0)
#define CF_DIRTY      (1<<0)
#define CF_INVALID    (1<<1)

struct CacheConfig
{
	UINT32 CacheNumBlocks; //number of blocks in the cache
	UINT32 BlockSize; //size of one block in bytes
	UINT32 SourceNumBlocks; //number of blocks in the source
	APTR UserBase;
	APTR ReadSourceCallback; //callback function for read from source
	APTR WriteSourceCallback; //callback function for write to source
};

typedef struct CacheBase
{
	struct Library		Library;
	APTR				SysBase;

	struct CacheConfig CacheConfig;

	APTR CacheBuffer; //points to data of whole cache
	UINT32 CacheFlag; //maintains state of cache, dirty etc.
	UINT32 CacheNum; //which block starts on cache

} CacheBase;


extern char CacheName[];
extern char CacheVer[];


//functions
struct CacheBase *cache_OpenLib(CacheBase *CacheBase);
APTR cache_CloseLib(CacheBase *CacheBase);
APTR cache_ExpungeLib(CacheBase *CacheBase);
APTR cache_ExtFuncLib(void);

BOOL cache_Configure(CacheBase *CacheBase, struct CacheConfig* Config);
UINT32 cache_Read(CacheBase *CacheBase, UINT32 block_offset, UINT32 byte_length, UINT8* data_ptr);
UINT32 cache_Write(CacheBase *CacheBase, UINT32 block_offset, UINT32 byte_length, UINT8* data_ptr);
void cache_Discard(CacheBase *CacheBase);
void cache_Sync(CacheBase *CacheBase);
BOOL cache_Hit(CacheBase *CacheBase, UINT32 block_offset, UINT32 byte_length);
BOOL cache_Dirty(CacheBase *CacheBase);


//internals
void Cache_internal_function(CacheBase *CacheBase);


//outer name (vectors) of our own library functions,
//we can use them inside our library too.
#define CacheConfigure(a) (((BOOL(*)(APTR, struct CacheConfig*)) 	_GETVECADDR(CacheBase, 5))(CacheBase, a))
#define CacheRead(a,b,c) (((UINT32(*)(APTR, UINT32, UINT32, UINT8*)) 	_GETVECADDR(CacheBase, 6))(CacheBase, a, b, c))
#define CacheWrite(a,b,c) (((UINT32(*)(APTR, UINT32, UINT32, UINT8*)) 	_GETVECADDR(CacheBase, 7))(CacheBase, a, b, c))
#define CacheDiscard() (((void(*)(APTR)) 	_GETVECADDR(CacheBase, 8))(CacheBase))
#define CacheSync() (((void(*)(APTR)) 	_GETVECADDR(CacheBase, 9))(CacheBase))
#define CacheHit(a,b) (((BOOL(*)(APTR, UINT32, UINT32)) 	_GETVECADDR(CacheBase, 10))(CacheBase, a, b))
#define CacheDirty() (((BOOL(*)(APTR)) 	_GETVECADDR(CacheBase, 11))(CacheBase))


#endif //cache_internal_h


