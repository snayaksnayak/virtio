#ifndef device_error_h
#define device_error_h

#define	TDERR_NotSpecified	20	/* general catchall */
#define	TDERR_NoSecHdr		21	/* couldn't even find a sector */
#define	TDERR_BadSecPreamble	22	/* sector looked wrong */
#define	TDERR_BadSecID		23	/* ditto */
#define	TDERR_BadHdrSum		24	/* header had incorrect checksum */
#define	TDERR_BadSecSum		25	/* data had incorrect checksum */
#define	TDERR_TooFewSecs	26	/* couldn't find enough sectors */
#define	TDERR_BadSecHdr		27	/* another "sector looked wrong" */
#define	TDERR_WriteProt		28	/* can't write to a protected disk */
#define	TDERR_DiskChanged	29	/* no disk in the drive */
#define	TDERR_SeekError		30	/* couldn't find track 0 */
#define	TDERR_NoMem		31	/* ran out of memory */
#define	TDERR_BadUnitNum	32	/* asked for a unit > NUMUNITS */
#define	TDERR_BadDriveType	33	/* not a drive that trackdisk groks */
#define	TDERR_DriveInUse	34	/* someone else allocated the drive */
#define	TDERR_PostReset		35	/* user hit reset; awaiting doom */

/*----- SCSI io_Error values -----*/
#define	HFERR_SelfUnit		40	/* cannot issue SCSI command to self */
#define	HFERR_DMA		41	/* DMA error */
#define	HFERR_Phase		42	/* illegal or unexpected SCSI phase */
#define	HFERR_Parity		43	/* SCSI parity error */
#define	HFERR_SelTimeout	44	/* Select timed out */
#define	HFERR_BadStatus		45	/* status and/or sense error */

/*----- OpenDevice io_Error values -----*/
#define	HFERR_NoBoard		50	/* Open failed for non-existant board */

#endif //device_error_h
