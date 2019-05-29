/*====================================================================================
    EVS Codec 3GPP TS26.442 Nov 13, 2018. Version 12.12.0 / 13.7.0 / 14.3.0 / 15.1.0
  ====================================================================================*/

/** \file jbm_jb4sb.h Jitter Buffer Management Interface */

#ifndef ACE_JB4SB_H
#define ACE_JB4SB_H ACE_JB4SB_H

/** handle for jitter buffer */
typedef struct JB4* JB4_HANDLE;

/** jitter buffer data units (access unit together with RTP seqNo, timestamp, ...) */
struct JB4_DATAUNIT
{
    /** the RTP sequence number (16 bits) */
    Word16 sequenceNumber;
    /** the RTP time stamp (32 bits) of this chunk in timeScale() units */
    Word32 timeStamp;
    /** the duration of this chunk in timeScale() units */
    Word32 duration;
    /** the RTP time scale, which is used for timeStamp() and duration() */
    Word32 timeScale;
    /** the receive time of the RTP packet in milliseconds */
    Word32 rcvTime;
    /** true, if the data unit contains only silence */
    Word16 silenceIndicator;

    /** the binary encoded access unit */
    UWord8 *data;
    /** the size of the binary encoded access unit [bits] */
    Word16 dataSize;

    /** identify if the data unit has a partial copy of a previous frame */
    Word16 partial_frame;
    /** offset of the partial copy contained in that frame or zero */
    Word16 partialCopyOffset;
    Word16 nextCoderType;
};
/** handle for jitter buffer data units */
typedef struct JB4_DATAUNIT* JB4_DATAUNIT_HANDLE;


Word16 JB4_Create( JB4_HANDLE *ph );
void JB4_Destroy( JB4_HANDLE *ph );

Word16 JB4_Init( JB4_HANDLE h, Word16 safetyMargin );

/** Returns a memory slot to store a new data unit */
JB4_DATAUNIT_HANDLE JB4_AllocDataUnit( JB4_HANDLE h );
/** Notifies the JBM that a data unit is no longer used and the memory can be reused */
void JB4_FreeDataUnit( JB4_HANDLE h, JB4_DATAUNIT_HANDLE dataUnit );

Word16 JB4_PushDataUnit( JB4_HANDLE h, JB4_DATAUNIT_HANDLE dataUnit, Word32 rcvTime );
Word16 JB4_PopDataUnit( JB4_HANDLE h, Word32 sysTime, Word32 extBufferedTime,
                        JB4_DATAUNIT_HANDLE *pDataUnit, Word16 *scale, Word16 *maxScaling );

/** function to get the number of data units contained in the buffer */
Word16 JB4_bufferedDataUnits( const JB4_HANDLE h );

Word16 JB4_getFECoffset(JB4_HANDLE h);

Word16 JB4_FECoffset(JB4_HANDLE h);

#endif /* ACE_JB4SB_H */
