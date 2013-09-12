#include "exec_funcs.h"
#include "lib_cache_internal.h"

#define SysBase LibCacheBase->SysBase

struct LibCacheBase* lib_cache_OpenLib(LibCacheBase *LibCacheBase)
{
    LibCacheBase->Library.lib_OpenCnt++;

	return(LibCacheBase);
}

APTR lib_cache_CloseLib(LibCacheBase *LibCacheBase)
{
	LibCacheBase->Library.lib_OpenCnt--;

	return (LibCacheBase);
}

APTR lib_cache_ExpungeLib(LibCacheBase *LibCacheBase)
{
	return (NULL);
}

APTR lib_cache_ExtFuncLib(void)
{
	return (NULL);
}


//**************


BOOL lib_cache_Configure(LibCacheBase *LibCacheBase, struct LibCacheConfig* Config)
{
	DPrintF("lib_cache_Configure: start\n");

	LibCacheBase->CacheConfig.UserBase = Config->UserBase;
	LibCacheBase->CacheConfig.BlockSize = Config->BlockSize;
	LibCacheBase->CacheConfig.CacheNumBlocks = Config->CacheNumBlocks;
	LibCacheBase->CacheConfig.SourceNumBlocks = Config->SourceNumBlocks;
	LibCacheBase->CacheConfig.ReadSourceCallback = Config->ReadSourceCallback;
	LibCacheBase->CacheConfig.WriteSourceCallback = Config->WriteSourceCallback;

	LibCacheBase->CacheBuffer = AllocVec(LibCacheBase->CacheConfig.CacheNumBlocks * LibCacheBase->CacheConfig.BlockSize, MEMF_FAST|MEMF_CLEAR);
	if(LibCacheBase->CacheBuffer == NULL)
	{
		return FALSE;
	}
	LibCacheBase->CacheFlag = CF_INVALID;
	LibCacheBase->CacheNum = 0;

	return TRUE;
}
BOOL lib_cache_Hit(LibCacheBase *LibCacheBase, UINT32 block_offset, UINT32 byte_length)
{
	DPrintF("lib_cache_Hit: start\n");

	UINT32 track_offset = block_offset / LibCacheBase->CacheConfig.CacheNumBlocks;
	UINT32 sector_offset = block_offset % LibCacheBase->CacheConfig.CacheNumBlocks;
	UINT32 sectors_to_readwrite = byte_length / LibCacheBase->CacheConfig.BlockSize;

	if((LibCacheBase->CacheFlag != CF_INVALID)
	&& (track_offset == LibCacheBase->CacheNum)
	&& (sectors_to_readwrite <= (LibCacheBase->CacheConfig.CacheNumBlocks -sector_offset)))
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}
UINT32 lib_cache_Read(LibCacheBase *LibCacheBase, UINT32 block_offset, UINT32 byte_length, UINT8* data_ptr)
{
	UINT32 actual=0;
	while(1)
	{
		UINT32 track_offset = block_offset / LibCacheBase->CacheConfig.CacheNumBlocks;
		UINT32 sector_offset = block_offset % LibCacheBase->CacheConfig.CacheNumBlocks;
		UINT32 remain_sectors = byte_length / LibCacheBase->CacheConfig.BlockSize;

		DPrintF("lib_cache_Read: block_offset = %d\n", block_offset);
		DPrintF("lib_cache_Read: byte_length = %d\n", byte_length);

		//work on current track present in cache
		if((track_offset == LibCacheBase->CacheNum) && (LibCacheBase->CacheFlag != CF_INVALID))
		{
			if(remain_sectors <= (LibCacheBase->CacheConfig.CacheNumBlocks) - (sector_offset))
			{

				DPrintF("lib_cache_Read: lengh within track size\n");
				memcpy(data_ptr + actual,
				LibCacheBase->CacheBuffer + (sector_offset * (LibCacheBase->CacheConfig.BlockSize)),
				remain_sectors * (LibCacheBase->CacheConfig.BlockSize));

				actual += remain_sectors * (LibCacheBase->CacheConfig.BlockSize);

				remain_sectors = 0;
				DPrintF("lib_cache_Read: One request complete\n");
				break;
			}
			else
			{
				DPrintF("lib_cache_Read: lengh outside track size\n");
				memcpy(data_ptr + actual,
				LibCacheBase->CacheBuffer + (sector_offset * (LibCacheBase->CacheConfig.BlockSize)),
				((LibCacheBase->CacheConfig.CacheNumBlocks) - (sector_offset)) * (LibCacheBase->CacheConfig.BlockSize));

				actual += ((LibCacheBase->CacheConfig.CacheNumBlocks) - (sector_offset)) * (LibCacheBase->CacheConfig.BlockSize);

				remain_sectors = remain_sectors - ((LibCacheBase->CacheConfig.CacheNumBlocks) - (sector_offset));

				track_offset++;
				sector_offset=0;

				block_offset = track_offset * (LibCacheBase->CacheConfig.CacheNumBlocks);

				byte_length = remain_sectors * (LibCacheBase->CacheConfig.BlockSize);

				DPrintF("lib_cache_Read: actual = %d\n", actual);
				DPrintF("lib_cache_Read: remain_sectors = %d\n", remain_sectors);
				DPrintF("lib_cache_Read: block_offset = %d\n", block_offset);
				DPrintF("lib_cache_Read: byte_length = %d\n", byte_length);
			}
		}
		else //work on another track from disk
		{
			//if current cache is dirty
			//write back to disk
			if(LibCacheBase->CacheFlag == CF_DIRTY)
			{
				LibCacheBase->CacheFlag = CF_CLEAN;

				DPrintF("lib_cache_Read: write dirty cache to disk; wait for callback to return\n");

				((void (*) (void*, UINT32, UINT32, void*))LibCacheBase->CacheConfig.WriteSourceCallback)
				(LibCacheBase->CacheConfig.UserBase,
				LibCacheBase->CacheNum * LibCacheBase->CacheConfig.CacheNumBlocks,
				LibCacheBase->CacheConfig.CacheNumBlocks,
				LibCacheBase->CacheBuffer);
			}

			//read a new track

			LibCacheBase->CacheNum = track_offset;
			LibCacheBase->CacheFlag = CF_CLEAN;

			DPrintF("lib_cache_Read: read cachefull data from disk; wait for callback to return\n");

			((void (*) (void*, UINT32, UINT32, void*))LibCacheBase->CacheConfig.ReadSourceCallback)
			(LibCacheBase->CacheConfig.UserBase,
			LibCacheBase->CacheNum * LibCacheBase->CacheConfig.CacheNumBlocks,
			LibCacheBase->CacheConfig.CacheNumBlocks,
			LibCacheBase->CacheBuffer);
		}
	}

	return actual;
}
UINT32 lib_cache_Write(LibCacheBase *LibCacheBase, UINT32 block_offset, UINT32 byte_length, UINT8* data_ptr)
{
	UINT32 actual=0;
	while(1)
	{

		UINT32 track_offset = block_offset / LibCacheBase->CacheConfig.CacheNumBlocks;
		UINT32 sector_offset = block_offset % LibCacheBase->CacheConfig.CacheNumBlocks;
		UINT32 remain_sectors = byte_length / LibCacheBase->CacheConfig.BlockSize;

		DPrintF("lib_cache_Write: block_offset = %d\n", block_offset);
		DPrintF("lib_cache_Write: byte_length = %d\n", byte_length);

		//work on current track present in cache
		if((track_offset == LibCacheBase->CacheNum) && (LibCacheBase->CacheFlag != CF_INVALID))
		{
			if(remain_sectors <= (LibCacheBase->CacheConfig.CacheNumBlocks) - (sector_offset))
			{
				DPrintF("lib_cache_Write: lengh within track size\n");
				memcpy(LibCacheBase->CacheBuffer + (sector_offset * (LibCacheBase->CacheConfig.BlockSize)),
				data_ptr + actual,
				remain_sectors * (LibCacheBase->CacheConfig.BlockSize));

				LibCacheBase->CacheFlag = CF_DIRTY;

				actual += remain_sectors * (LibCacheBase->CacheConfig.BlockSize);

				remain_sectors = 0;
				DPrintF("lib_cache_Write: One request complete\n");
				break;
			}
			else
			{
				DPrintF("lib_cache_Write: lengh outside track size\n");
				memcpy(LibCacheBase->CacheBuffer + (sector_offset * (LibCacheBase->CacheConfig.BlockSize)),
				data_ptr + actual,
				((LibCacheBase->CacheConfig.CacheNumBlocks) - (sector_offset)) * (LibCacheBase->CacheConfig.BlockSize));

				LibCacheBase->CacheFlag = CF_DIRTY;

				actual += ((LibCacheBase->CacheConfig.CacheNumBlocks) - (sector_offset)) * (LibCacheBase->CacheConfig.BlockSize);

				remain_sectors = remain_sectors - ((LibCacheBase->CacheConfig.CacheNumBlocks) - (sector_offset));

				track_offset++;
				sector_offset=0;

				block_offset = track_offset * (LibCacheBase->CacheConfig.CacheNumBlocks);

				byte_length = remain_sectors * (LibCacheBase->CacheConfig.BlockSize);

				DPrintF("lib_cache_Write: actual = %d\n", actual);
				DPrintF("lib_cache_Write: remain_sectors = %d\n", remain_sectors);
				DPrintF("lib_cache_Write: block_offset = %d\n", block_offset);
				DPrintF("lib_cache_Write: byte_length = %d\n", byte_length);

			}
		}
		else //work on another track from disk
		{
			//if current cache is dirty
			//write back to disk
			if(LibCacheBase->CacheFlag == CF_DIRTY)
			{
				LibCacheBase->CacheFlag = CF_CLEAN;

				DPrintF("lib_cache_Write: write dirty cache to disk; wait for callback to return\n");

				((void (*) (void*, UINT32, UINT32, void*))LibCacheBase->CacheConfig.WriteSourceCallback)
				(LibCacheBase->CacheConfig.UserBase,
				LibCacheBase->CacheNum * LibCacheBase->CacheConfig.CacheNumBlocks,
				LibCacheBase->CacheConfig.CacheNumBlocks,
				LibCacheBase->CacheBuffer);
			}

			if(sector_offset == 0 && remain_sectors >= (LibCacheBase->CacheConfig.CacheNumBlocks))
			{
				//write, update tracknum, make dirty
				DPrintF("lib_cache_Write: Fill the whole cache, mark it dirty\n");
				memcpy(LibCacheBase->CacheBuffer + (sector_offset * (LibCacheBase->CacheConfig.BlockSize)),
				data_ptr + actual,
				(LibCacheBase->CacheConfig.CacheNumBlocks) * (LibCacheBase->CacheConfig.BlockSize));

				LibCacheBase->CacheNum = track_offset;
				LibCacheBase->CacheFlag = CF_DIRTY;

				actual += (LibCacheBase->CacheConfig.CacheNumBlocks) * (LibCacheBase->CacheConfig.BlockSize);

				if(remain_sectors == (LibCacheBase->CacheConfig.CacheNumBlocks))
				{
					remain_sectors = 0;

					DPrintF("lib_cache_Write: One request complete\n");
					break;
				}
				else
				{
					remain_sectors = remain_sectors - (LibCacheBase->CacheConfig.CacheNumBlocks);

					track_offset++;
					sector_offset=0;

					block_offset = track_offset * (LibCacheBase->CacheConfig.CacheNumBlocks);

					byte_length = remain_sectors * (LibCacheBase->CacheConfig.BlockSize);

					DPrintF("lib_cache_Write: actual = %d\n", actual);
					DPrintF("lib_cache_Write: remain_sectors = %d\n", remain_sectors);
					DPrintF("lib_cache_Write: block_offset = %d\n", block_offset);
					DPrintF("lib_cache_Write: byte_length = %d\n", byte_length);
				}
			}
			else
			{
				//read a new track
				LibCacheBase->CacheNum = track_offset;
				LibCacheBase->CacheFlag = CF_CLEAN;

				DPrintF("lib_cache_Write: read cachefull data from disk; wait for callback to return\n");

				((void (*) (void*, UINT32, UINT32, void*))LibCacheBase->CacheConfig.ReadSourceCallback)
				(LibCacheBase->CacheConfig.UserBase,
				LibCacheBase->CacheNum * LibCacheBase->CacheConfig.CacheNumBlocks,
				LibCacheBase->CacheConfig.CacheNumBlocks,
				LibCacheBase->CacheBuffer);
			}
		}
	}

	return actual;
}
void lib_cache_Discard(LibCacheBase *LibCacheBase)
{
	DPrintF("lib_cache_Discard: start\n");
	LibCacheBase->CacheFlag = CF_INVALID;
}
void lib_cache_Sync(LibCacheBase *LibCacheBase)
{
	DPrintF("lib_cache_Sync: start\n");
	//if current cache is dirty
	//write back to disk
	if(LibCacheBase->CacheFlag == CF_DIRTY)
	{
		LibCacheBase->CacheFlag = CF_CLEAN;

		DPrintF("lib_cache_Sync: write dirty cache to disk; wait for callback to return\n");

		((void (*) (void*, UINT32, UINT32, void*))LibCacheBase->CacheConfig.WriteSourceCallback)
		(LibCacheBase->CacheConfig.UserBase,
		LibCacheBase->CacheNum * LibCacheBase->CacheConfig.CacheNumBlocks,
		LibCacheBase->CacheConfig.CacheNumBlocks,
		LibCacheBase->CacheBuffer);
	}
}
BOOL lib_cache_Dirty(LibCacheBase *LibCacheBase)
{
	DPrintF("lib_cache_Dirty: start\n");
	if(LibCacheBase->CacheFlag == CF_DIRTY)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}