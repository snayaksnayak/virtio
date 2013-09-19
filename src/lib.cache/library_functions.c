#include "exec_funcs.h"
#include "cache_internal.h"

#define SysBase CacheBase->SysBase

struct CacheBase* cache_OpenLib(CacheBase *CacheBase)
{
    CacheBase->Library.lib_OpenCnt++;

	return(CacheBase);
}

APTR cache_CloseLib(CacheBase *CacheBase)
{
	CacheBase->Library.lib_OpenCnt--;

	return (CacheBase);
}

APTR cache_ExpungeLib(CacheBase *CacheBase)
{
	return (NULL);
}

APTR cache_ExtFuncLib(void)
{
	return (NULL);
}


//**************


BOOL cache_Configure(CacheBase *CacheBase, struct CacheConfig* Config)
{
	DPrintF("cache_Configure: start\n");

	CacheBase->CacheConfig.UserBase = Config->UserBase;
	CacheBase->CacheConfig.BlockSize = Config->BlockSize;
	CacheBase->CacheConfig.CacheNumBlocks = Config->CacheNumBlocks;
	CacheBase->CacheConfig.SourceNumBlocks = Config->SourceNumBlocks;
	CacheBase->CacheConfig.ReadSourceCallback = Config->ReadSourceCallback;
	CacheBase->CacheConfig.WriteSourceCallback = Config->WriteSourceCallback;

	CacheBase->CacheBuffer = AllocVec(CacheBase->CacheConfig.CacheNumBlocks * CacheBase->CacheConfig.BlockSize, MEMF_FAST|MEMF_CLEAR);
	if(CacheBase->CacheBuffer == NULL)
	{
		return FALSE;
	}
	CacheBase->CacheFlag = CF_INVALID;
	CacheBase->CacheNum = 0;

	return TRUE;
}
BOOL cache_Hit(CacheBase *CacheBase, UINT32 block_offset, UINT32 byte_length)
{
	DPrintF("cache_Hit: start\n");

	UINT32 track_offset = block_offset / CacheBase->CacheConfig.CacheNumBlocks;
	UINT32 sector_offset = block_offset % CacheBase->CacheConfig.CacheNumBlocks;
	UINT32 sectors_to_readwrite = byte_length / CacheBase->CacheConfig.BlockSize;

	if((CacheBase->CacheFlag != CF_INVALID)
	&& (track_offset == CacheBase->CacheNum)
	&& (sectors_to_readwrite <= (CacheBase->CacheConfig.CacheNumBlocks -sector_offset)))
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}
UINT32 cache_Read(CacheBase *CacheBase, UINT32 block_offset, UINT32 byte_length, UINT8* data_ptr)
{
	UINT32 actual=0;
	while(1)
	{
		UINT32 track_offset = block_offset / CacheBase->CacheConfig.CacheNumBlocks;
		UINT32 sector_offset = block_offset % CacheBase->CacheConfig.CacheNumBlocks;
		UINT32 remain_sectors = byte_length / CacheBase->CacheConfig.BlockSize;

		DPrintF("cache_Read: block_offset = %d\n", block_offset);
		DPrintF("cache_Read: byte_length = %d\n", byte_length);

		//work on current track present in cache
		if((track_offset == CacheBase->CacheNum) && (CacheBase->CacheFlag != CF_INVALID))
		{
			if(remain_sectors <= (CacheBase->CacheConfig.CacheNumBlocks) - (sector_offset))
			{

				DPrintF("cache_Read: lengh within track size\n");
				memcpy(data_ptr + actual,
				CacheBase->CacheBuffer + (sector_offset * (CacheBase->CacheConfig.BlockSize)),
				remain_sectors * (CacheBase->CacheConfig.BlockSize));

				actual += remain_sectors * (CacheBase->CacheConfig.BlockSize);

				remain_sectors = 0;
				DPrintF("cache_Read: One request complete\n");
				break;
			}
			else
			{
				DPrintF("cache_Read: lengh outside track size\n");
				memcpy(data_ptr + actual,
				CacheBase->CacheBuffer + (sector_offset * (CacheBase->CacheConfig.BlockSize)),
				((CacheBase->CacheConfig.CacheNumBlocks) - (sector_offset)) * (CacheBase->CacheConfig.BlockSize));

				actual += ((CacheBase->CacheConfig.CacheNumBlocks) - (sector_offset)) * (CacheBase->CacheConfig.BlockSize);

				remain_sectors = remain_sectors - ((CacheBase->CacheConfig.CacheNumBlocks) - (sector_offset));

				track_offset++;
				sector_offset=0;

				block_offset = track_offset * (CacheBase->CacheConfig.CacheNumBlocks);

				byte_length = remain_sectors * (CacheBase->CacheConfig.BlockSize);

				DPrintF("cache_Read: actual = %d\n", actual);
				DPrintF("cache_Read: remain_sectors = %d\n", remain_sectors);
				DPrintF("cache_Read: block_offset = %d\n", block_offset);
				DPrintF("cache_Read: byte_length = %d\n", byte_length);
			}
		}
		else //work on another track from disk
		{
			//if current cache is dirty
			//write back to disk
			if(CacheBase->CacheFlag == CF_DIRTY)
			{
				CacheBase->CacheFlag = CF_CLEAN;

				DPrintF("cache_Read: write dirty cache to disk; wait for callback to return\n");

				((void (*) (void*, UINT32, UINT32, void*))CacheBase->CacheConfig.WriteSourceCallback)
				(CacheBase->CacheConfig.UserBase,
				CacheBase->CacheNum * CacheBase->CacheConfig.CacheNumBlocks,
				CacheBase->CacheConfig.CacheNumBlocks,
				CacheBase->CacheBuffer);
			}

			//read a new track

			CacheBase->CacheNum = track_offset;
			CacheBase->CacheFlag = CF_CLEAN;

			DPrintF("cache_Read: read cachefull data from disk; wait for callback to return\n");

			((void (*) (void*, UINT32, UINT32, void*))CacheBase->CacheConfig.ReadSourceCallback)
			(CacheBase->CacheConfig.UserBase,
			CacheBase->CacheNum * CacheBase->CacheConfig.CacheNumBlocks,
			CacheBase->CacheConfig.CacheNumBlocks,
			CacheBase->CacheBuffer);
		}
	}

	return actual;
}
UINT32 cache_Write(CacheBase *CacheBase, UINT32 block_offset, UINT32 byte_length, UINT8* data_ptr)
{
	UINT32 actual=0;
	while(1)
	{

		UINT32 track_offset = block_offset / CacheBase->CacheConfig.CacheNumBlocks;
		UINT32 sector_offset = block_offset % CacheBase->CacheConfig.CacheNumBlocks;
		UINT32 remain_sectors = byte_length / CacheBase->CacheConfig.BlockSize;

		DPrintF("cache_Write: block_offset = %d\n", block_offset);
		DPrintF("cache_Write: byte_length = %d\n", byte_length);

		//work on current track present in cache
		if((track_offset == CacheBase->CacheNum) && (CacheBase->CacheFlag != CF_INVALID))
		{
			if(remain_sectors <= (CacheBase->CacheConfig.CacheNumBlocks) - (sector_offset))
			{
				DPrintF("cache_Write: lengh within track size\n");
				memcpy(CacheBase->CacheBuffer + (sector_offset * (CacheBase->CacheConfig.BlockSize)),
				data_ptr + actual,
				remain_sectors * (CacheBase->CacheConfig.BlockSize));

				CacheBase->CacheFlag = CF_DIRTY;

				actual += remain_sectors * (CacheBase->CacheConfig.BlockSize);

				remain_sectors = 0;
				DPrintF("cache_Write: One request complete\n");
				break;
			}
			else
			{
				DPrintF("cache_Write: lengh outside track size\n");
				memcpy(CacheBase->CacheBuffer + (sector_offset * (CacheBase->CacheConfig.BlockSize)),
				data_ptr + actual,
				((CacheBase->CacheConfig.CacheNumBlocks) - (sector_offset)) * (CacheBase->CacheConfig.BlockSize));

				CacheBase->CacheFlag = CF_DIRTY;

				actual += ((CacheBase->CacheConfig.CacheNumBlocks) - (sector_offset)) * (CacheBase->CacheConfig.BlockSize);

				remain_sectors = remain_sectors - ((CacheBase->CacheConfig.CacheNumBlocks) - (sector_offset));

				track_offset++;
				sector_offset=0;

				block_offset = track_offset * (CacheBase->CacheConfig.CacheNumBlocks);

				byte_length = remain_sectors * (CacheBase->CacheConfig.BlockSize);

				DPrintF("cache_Write: actual = %d\n", actual);
				DPrintF("cache_Write: remain_sectors = %d\n", remain_sectors);
				DPrintF("cache_Write: block_offset = %d\n", block_offset);
				DPrintF("cache_Write: byte_length = %d\n", byte_length);

			}
		}
		else //work on another track from disk
		{
			//if current cache is dirty
			//write back to disk
			if(CacheBase->CacheFlag == CF_DIRTY)
			{
				CacheBase->CacheFlag = CF_CLEAN;

				DPrintF("cache_Write: write dirty cache to disk; wait for callback to return\n");

				((void (*) (void*, UINT32, UINT32, void*))CacheBase->CacheConfig.WriteSourceCallback)
				(CacheBase->CacheConfig.UserBase,
				CacheBase->CacheNum * CacheBase->CacheConfig.CacheNumBlocks,
				CacheBase->CacheConfig.CacheNumBlocks,
				CacheBase->CacheBuffer);
			}

			if(sector_offset == 0 && remain_sectors >= (CacheBase->CacheConfig.CacheNumBlocks))
			{
				//write, update tracknum, make dirty
				DPrintF("cache_Write: Fill the whole cache, mark it dirty\n");
				memcpy(CacheBase->CacheBuffer + (sector_offset * (CacheBase->CacheConfig.BlockSize)),
				data_ptr + actual,
				(CacheBase->CacheConfig.CacheNumBlocks) * (CacheBase->CacheConfig.BlockSize));

				CacheBase->CacheNum = track_offset;
				CacheBase->CacheFlag = CF_DIRTY;

				actual += (CacheBase->CacheConfig.CacheNumBlocks) * (CacheBase->CacheConfig.BlockSize);

				if(remain_sectors == (CacheBase->CacheConfig.CacheNumBlocks))
				{
					remain_sectors = 0;

					DPrintF("cache_Write: One request complete\n");
					break;
				}
				else
				{
					remain_sectors = remain_sectors - (CacheBase->CacheConfig.CacheNumBlocks);

					track_offset++;
					sector_offset=0;

					block_offset = track_offset * (CacheBase->CacheConfig.CacheNumBlocks);

					byte_length = remain_sectors * (CacheBase->CacheConfig.BlockSize);

					DPrintF("cache_Write: actual = %d\n", actual);
					DPrintF("cache_Write: remain_sectors = %d\n", remain_sectors);
					DPrintF("cache_Write: block_offset = %d\n", block_offset);
					DPrintF("cache_Write: byte_length = %d\n", byte_length);
				}
			}
			else
			{
				//read a new track
				CacheBase->CacheNum = track_offset;
				CacheBase->CacheFlag = CF_CLEAN;

				DPrintF("cache_Write: read cachefull data from disk; wait for callback to return\n");

				((void (*) (void*, UINT32, UINT32, void*))CacheBase->CacheConfig.ReadSourceCallback)
				(CacheBase->CacheConfig.UserBase,
				CacheBase->CacheNum * CacheBase->CacheConfig.CacheNumBlocks,
				CacheBase->CacheConfig.CacheNumBlocks,
				CacheBase->CacheBuffer);
			}
		}
	}

	return actual;
}
void cache_Discard(CacheBase *CacheBase)
{
	DPrintF("cache_Discard: start\n");
	CacheBase->CacheFlag = CF_INVALID;
}
void cache_Sync(CacheBase *CacheBase)
{
	DPrintF("cache_Sync: start\n");
	//if current cache is dirty
	//write back to disk
	if(CacheBase->CacheFlag == CF_DIRTY)
	{
		CacheBase->CacheFlag = CF_CLEAN;

		DPrintF("cache_Sync: write dirty cache to disk; wait for callback to return\n");

		((void (*) (void*, UINT32, UINT32, void*))CacheBase->CacheConfig.WriteSourceCallback)
		(CacheBase->CacheConfig.UserBase,
		CacheBase->CacheNum * CacheBase->CacheConfig.CacheNumBlocks,
		CacheBase->CacheConfig.CacheNumBlocks,
		CacheBase->CacheBuffer);
	}
}
BOOL cache_Dirty(CacheBase *CacheBase)
{
	DPrintF("cache_Dirty: start\n");
	if(CacheBase->CacheFlag == CF_DIRTY)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}
