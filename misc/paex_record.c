#include <stdio.h>
#include <stdlib.h>
#include <portaudio.h>

// write_wav.h
#ifndef _WAV_WRITER_H
#define _WAV_WRITER_H

/*
 * WAV file writer.
 *
 * Author: Phil Burk
 */

#ifdef __cplusplus
extern "C" {
#endif

    /* Define WAV Chunk and FORM types as 4 byte integers. */
#define RIFF_ID   (('R'<<24) | ('I'<<16) | ('F'<<8) | 'F')
#define WAVE_ID   (('W'<<24) | ('A'<<16) | ('V'<<8) | 'E')
#define FMT_ID    (('f'<<24) | ('m'<<16) | ('t'<<8) | ' ')
#define DATA_ID   (('d'<<24) | ('a'<<16) | ('t'<<8) | 'a')
#define FACT_ID   (('f'<<24) | ('a'<<16) | ('c'<<8) | 't')

/* Errors returned by Audio_ParseSampleImage_WAV */
#define WAV_ERR_CHUNK_SIZE     (-1)   /* Chunk size is illegal or past file size. */
#define WAV_ERR_FILE_TYPE      (-2)   /* Not a WAV file. */
#define WAV_ERR_ILLEGAL_VALUE  (-3)   /* Illegal or unsupported value. Eg. 927 bits/sample */
#define WAV_ERR_FORMAT_TYPE    (-4)   /* Unsupported format, eg. compressed. */
#define WAV_ERR_TRUNCATED      (-5)   /* End of file missing. */

/* WAV PCM data format ID */
#define WAVE_FORMAT_PCM        (1)
#define WAVE_FORMAT_IMA_ADPCM  (0x0011)


    typedef struct WAV_Writer_s
    {
        FILE* fid;
        /* Offset in file for data size. */
        int   dataSizeOffset;
        int   dataSize;
    } WAV_Writer;

    /*********************************************************************************
     * Open named file and write WAV header to the file.
     * The header includes the DATA chunk type and size.
     * Returns number of bytes written to file or negative error code.
     */
    long Audio_WAV_OpenWriter(WAV_Writer* writer, const char* fileName, int frameRate, int samplesPerFrame);

    /*********************************************************************************
     * Write to the data chunk portion of a WAV file.
     * Returns bytes written or negative error code.
     */
    long Audio_WAV_WriteShorts(WAV_Writer* writer,
        short* samples,
        int numSamples
    );

    /*********************************************************************************
     * Close WAV file.
     * Update chunk sizes so it can be read by audio applications.
     */
    long Audio_WAV_CloseWriter(WAV_Writer* writer);

#ifdef __cplusplus
};
#endif

#endif /* _WAV_WRITER_H */

// write_wav.c
static void WriteLongLE(unsigned char** addrPtr, unsigned long data)
{
    unsigned char* addr = *addrPtr;
    *addr++ = (unsigned char)data;
    *addr++ = (unsigned char)(data >> 8);
    *addr++ = (unsigned char)(data >> 16);
    *addr++ = (unsigned char)(data >> 24);
    *addrPtr = addr;
}

/* Write short word data to a little endian format byte array. */
static void WriteShortLE(unsigned char** addrPtr, unsigned short data)
{
    unsigned char* addr = *addrPtr;
    *addr++ = (unsigned char)data;
    *addr++ = (unsigned char)(data >> 8);
    *addrPtr = addr;
}

/* Write IFF ChunkType data to a byte array. */
static void WriteChunkType(unsigned char** addrPtr, unsigned long cktyp)
{
    unsigned char* addr = *addrPtr;
    *addr++ = (unsigned char)(cktyp >> 24);
    *addr++ = (unsigned char)(cktyp >> 16);
    *addr++ = (unsigned char)(cktyp >> 8);
    *addr++ = (unsigned char)cktyp;
    *addrPtr = addr;
}

#define WAV_HEADER_SIZE (4 + 4 + 4 + /* RIFF+size+WAVE */ \
        4 + 4 + 16 + /* fmt chunk */ \
        4 + 4 ) /* data chunk */


/*********************************************************************************
 * Open named file and write WAV header to the file.
 * The header includes the DATA chunk type and size.
 * Returns number of bytes written to file or negative error code.
 */
long Audio_WAV_OpenWriter(WAV_Writer* writer, const char* fileName, int frameRate, int samplesPerFrame)
{
    unsigned int  bytesPerSecond;
    unsigned char header[WAV_HEADER_SIZE];
    unsigned char* addr = header;
    int numWritten;

    writer->dataSize = 0;
    writer->dataSizeOffset = 0;

    writer->fid = fopen(fileName, "wb");
    if (writer->fid == NULL)
    {
        return -1;
    }

    /* Write RIFF header. */
    WriteChunkType(&addr, RIFF_ID);

    /* Write RIFF size as zero for now. Will patch later. */
    WriteLongLE(&addr, 0);

    /* Write WAVE form ID. */
    WriteChunkType(&addr, WAVE_ID);

    /* Write format chunk based on AudioSample structure. */
    WriteChunkType(&addr, FMT_ID);
    WriteLongLE(&addr, 16);
    WriteShortLE(&addr, WAVE_FORMAT_PCM);
    bytesPerSecond = frameRate * samplesPerFrame * sizeof(short);
    WriteShortLE(&addr, (short)samplesPerFrame);
    WriteLongLE(&addr, frameRate);
    WriteLongLE(&addr, bytesPerSecond);
    WriteShortLE(&addr, (short)(samplesPerFrame * sizeof(short))); /* bytesPerBlock */
    WriteShortLE(&addr, (short)16); /* bits per sample */

    /* Write ID and size for 'data' chunk. */
    WriteChunkType(&addr, DATA_ID);
    /* Save offset so we can patch it later. */
    writer->dataSizeOffset = (int)(addr - header);
    WriteLongLE(&addr, 0);

    numWritten = fwrite(header, 1, sizeof(header), writer->fid);
    if (numWritten != sizeof(header)) return -1;

    return (int)numWritten;
}

/*********************************************************************************
 * Write to the data chunk portion of a WAV file.
 * Returns bytes written or negative error code.
 */
long Audio_WAV_WriteShorts(WAV_Writer* writer,
    short* samples,
    int numSamples
)
{
    unsigned char buffer[2];
    unsigned char* bufferPtr;
    int i;
    short* p = samples;
    int numWritten;
    int bytesWritten;
    if (numSamples <= 0)
    {
        return -1;
    }

    for (i = 0; i < numSamples; i++)
    {
        bufferPtr = buffer;
        WriteShortLE(&bufferPtr, *p++);
        numWritten = fwrite(buffer, 1, sizeof(buffer), writer->fid);
        if (numWritten != sizeof(buffer)) return -1;
    }
    bytesWritten = numSamples * sizeof(short);
    writer->dataSize += bytesWritten;
    return (int)bytesWritten;
}

/*********************************************************************************
 * Close WAV file.
 * Update chunk sizes so it can be read by audio applications.
 */
long Audio_WAV_CloseWriter(WAV_Writer* writer)
{
    unsigned char buffer[4];
    unsigned char* bufferPtr;
    int numWritten;
    int riffSize;

    /* Go back to beginning of file and update DATA size */
    int result = fseek(writer->fid, writer->dataSizeOffset, SEEK_SET);
    if (result < 0) return result;

    bufferPtr = buffer;
    WriteLongLE(&bufferPtr, writer->dataSize);
    numWritten = fwrite(buffer, 1, sizeof(buffer), writer->fid);
    if (numWritten != sizeof(buffer)) return -1;

    /* Update RIFF size */
    result = fseek(writer->fid, 4, SEEK_SET);
    if (result < 0) return result;

    riffSize = writer->dataSize + (WAV_HEADER_SIZE - 8);
    bufferPtr = buffer;
    WriteLongLE(&bufferPtr, riffSize);
    numWritten = fwrite(buffer, 1, sizeof(buffer), writer->fid);
    if (numWritten != sizeof(buffer)) return -1;

    fclose(writer->fid);
    writer->fid = NULL;
    return writer->dataSize;
}

/* #define SAMPLE_RATE  (17932) // Test failure to open with this value. */
#define SAMPLE_RATE  (44100)
#define FRAMES_PER_BUFFER (512)
#define NUM_SECONDS     (5)
#define NUM_CHANNELS    (2)
/* #define DITHER_FLAG     (paDitherOff) */
#define DITHER_FLAG     (0) /**/

/* Select sample format. */
#define PA_SAMPLE_TYPE  paInt16
typedef short SAMPLE;
#define SAMPLE_SILENCE  (0)
#define PRINTF_S_FORMAT "%d"

typedef struct
{
    int          frameIndex;  /* Index into sample array. */
    int          maxFrameIndex;
    SAMPLE      *recordedSamples;
}
paTestData;

/* This routine will be called by the PortAudio engine when audio is needed.
** It may be called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/
static int recordCallback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData )
{
    paTestData *data = (paTestData*)userData;
    const SAMPLE *rptr = (const SAMPLE*)inputBuffer;
    SAMPLE *wptr = &data->recordedSamples[data->frameIndex * NUM_CHANNELS];
    long framesToCalc;
    long i;
  //  int finished;
    unsigned long framesLeft = data->maxFrameIndex - data->frameIndex;

    (void) outputBuffer; /* Prevent unused variable warnings. */
    (void) timeInfo;
    (void) statusFlags;
    (void) userData;

    if( framesLeft < framesPerBuffer )
    {
        framesToCalc = framesLeft;
      //  finished = paComplete;
    }
    else
    {
        framesToCalc = framesPerBuffer;
      //  finished = paContinue;
    }
    if (!framesLeft)
      {
       data->frameIndex = 0;
      }
    if( inputBuffer == NULL )
    {
        for( i=0; i<framesToCalc; i++ )
        {
            *wptr++ = SAMPLE_SILENCE;  /* left */
            if( NUM_CHANNELS == 2 ) *wptr++ = SAMPLE_SILENCE;  /* right */
        }
    }
    else
    {
        for( i=0; i<framesToCalc; i++ )
        {
            *wptr++ = *rptr++;  /* left */
            if( NUM_CHANNELS == 2 ) *wptr++ = *rptr++;  /* right */
        }
    }
    data->frameIndex += framesToCalc;
    //return finished;
    return paContinue;
}

/*******************************************************************/
int main(void);
int main(void)
{
    PaStreamParameters  inputParameters,
                        outputParameters;
    PaStream*           stream;
    PaError             err = paNoError;
    paTestData          data;
    int                 i;
    int                 totalFrames;
    int                 numSamples;
    int                 numBytes;
    SAMPLE              max, val;
    double              average;
    WAV_Writer writer;
    int result;

    data.maxFrameIndex = totalFrames = NUM_SECONDS * SAMPLE_RATE; /* Record for a few seconds. */
    data.frameIndex = 0;
    numSamples = totalFrames * NUM_CHANNELS;
    numBytes = numSamples * sizeof(SAMPLE);
    data.recordedSamples = (SAMPLE *) malloc( numBytes ); /* From now on, recordedSamples is initialised. */
    if( data.recordedSamples == NULL )
    {
        printf("Could not allocate record array.\n");
        goto done;
    }
    for( i=0; i<numSamples; i++ ) data.recordedSamples[i] = 0;

    err = Pa_Initialize();
    if( err != paNoError ) goto done;

    inputParameters.device = Pa_GetDefaultInputDevice(); /* default input device */
    if (inputParameters.device == paNoDevice) {
        fprintf(stderr,"Error: No default input device.\n");
        goto done;
    }
    inputParameters.channelCount = 2;                    /* stereo input */
    inputParameters.sampleFormat = PA_SAMPLE_TYPE;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    /* Record some audio. -------------------------------------------- */
    err = Pa_OpenStream(
              &stream,
              &inputParameters,
              NULL,                  /* &outputParameters, */
              SAMPLE_RATE,
              FRAMES_PER_BUFFER,
              paClipOff,      /* we won't output out of range samples so don't bother clipping them */
              recordCallback,
              &data );
    if( err != paNoError ) goto done;

    err = Pa_StartStream( stream );
    if( err != paNoError ) goto done;
    printf("\n=== Now recording!! Please speak into the microphone. ===\n"); fflush(stdout);

    result = Audio_WAV_OpenWriter(&writer, "rendered_midi.wav", 44100, 2);
    while( ( err = Pa_IsStreamActive( stream ) ) == 1 )
    {
        Pa_Sleep(1000);
        printf("index = %d\n", data.frameIndex ); fflush(stdout);
    }
    result = Audio_WAV_WriteShorts(&writer, data.recordedSamples, numSamples);
    result = Audio_WAV_CloseWriter(&writer);
    if( err < 0 ) goto done;

    err = Pa_CloseStream( stream );
    if( err != paNoError ) goto done;

    /* Write recorded data to a file. */
    /* {
        result = Audio_WAV_OpenWriter(&writer, "rendered_midi.wav", 44100, 2);
        result = Audio_WAV_WriteShorts(&writer, data.recordedSamples, numSamples);
        result = Audio_WAV_CloseWriter(&writer);
    }*/

done:
    Pa_Terminate();
    if( data.recordedSamples )       /* Sure it is NULL or valid. */
        free( data.recordedSamples );
    if( err != paNoError )
    {
        fprintf( stderr, "An error occurred while using the portaudio stream\n" );
        fprintf( stderr, "Error number: %d\n", err );
        fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
        err = 1;          /* Always return 0 or 1, but no other return codes. */
    }
    return err;
}
