#ifndef device_error_h
#define device_error_h

//----- Block Device io_Error values -----
#define	BLK_ERR_Unknown				(-20)	// general catchall
#define	BLK_ERR_BadSectorRequest	(-21)	// sector looked wrong
#define	BLK_ERR_BadSectorDataSum	(-22)	// data had incorrect checksum
#define	BLK_ERR_NotEnoughSectors	(-23)	// couldn't find enough sectors
#define	BLK_ERR_WriteProtected		(-24)	// can't write to a write-protected disk
#define	BLK_ERR_DiskNotFound		(-25)	// no disk found in the drive
#define	BLK_ERR_NoMem				(-26)	// ran out of memory
#define	BLK_ERR_BadUnitNum			(-27)	// asked for a unit >= available number of units
#define	BLK_ERR_BadDriveType		(-28)	// not a drive that block device handles
#define	BLK_ERR_DriveInUse			(-29)	// someone else is using the drive


#endif //device_error_h
