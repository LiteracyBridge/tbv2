/*  mp3.c   TBook mp3 decoder interface to libMAD 
 *  July 2020
 */

#include <stdio.h>
#include <stdint.h>
#include "wav.h"
#include "mad.h"
#include "tbook.h"
#include "fileOps.h"

const int BUFF_SIZ      = 4096;
const int MAX_WAV_BYTES = 16000 * 2 * 200;

typedef struct {    // decode state passed to mp3 callbacks
    int freq;       // samples/sec from decoded pcm structure

    FILE *fmp3;
    int  in_pos;

    FILE *fwav;

    unsigned char *in_buff;
}         decoder_state_t;

static bool initialized = false;
static decoder_state_t decoder_state;
const bool             reportAllMp3Errs = false; // true;   // all files seem to get "lost sync" at start

//
static void *wav_header( int samplesPerSecond, int channels, int bitsPerSample, int audioSize ) {                     // => ptr to valid .wav header
    static char   hdr[44]         = "RIFFsizeWAVEfmt \x10\0\0\0\1\0ch_hz_abpsbabsdatasize";
    unsigned long bytesPerSecond = (bitsPerSample * channels * samplesPerSecond) / 8;
    unsigned int  blockSize      = (bitsPerSample * channels) / 8;
    // @formatter:off
    *(int32_t *)(void*)(hdr + 0x04) = 44 + audioSize - 8;   /* File size - 8 */
    *(int16_t *)(void*)(hdr + 0x14) = 1;                     /* Integer PCM format */
    *(int16_t *)(void*)(hdr + 0x16) = channels;
    *(int32_t *)(void*)(hdr + 0x18) = samplesPerSecond;
    *(int32_t *)(void*)(hdr + 0x1C) = bytesPerSecond;
    *(int16_t *)(void*)(hdr + 0x20) = blockSize;
    *(int16_t *)(void*)(hdr + 0x22) = bitsPerSample;
    *(int32_t *)(void*)(hdr + 0x28) = audioSize;
    // @formatter:on

    WavFileHeader_t header;
    header = newWavHeader;
    header.wavHeader.waveSize       = 44 + audioSize - 8; // file size -8 (eventually)
    header.fmtData.formatCode       = 1; // PCM
    header.fmtData.numChannels      = channels;
    header.fmtData.samplesPerSecond = samplesPerSecond;
    header.fmtData.bytesPerSecond   = bytesPerSecond;
    header.fmtData.blockSize        = blockSize;
    header.fmtData.bitsPerSample    = bitsPerSample;
    header.audioHeader.chunkSize    = audioSize; // The initial value is just made up

    // @TODO: verify that the "named" header matches the "hand-rolled" one, then return it.
    int cmp = memcmp(&header, &hdr, sizeof(header));

    return hdr;
}


static enum mad_flow input( void *st, struct mad_stream *stream ) {                               // cb fn to refill buffer from .mp3
    /*
     * This is the input callback. The purpose of this callback is to (re)fill
     * the stream buffer which is to be decoded.
     */
    decoder_state_t *decoder_state = (decoder_state_t *) st;

    if ( decoder_state->fmp3 == NULL ) // file already closed, stop
        return MAD_FLOW_STOP;

    uint8_t *rdaddr = &decoder_state->in_buff[0];
    int     rdlen   = BUFF_SIZ;
    if ( stream->next_frame > stream->buffer ) {     // SAVE UNUSED PART OF BUFFER
        rdlen      = stream->next_frame - decoder_state->in_buff;  // amount that's been processed
        int cpylen = BUFF_SIZ - rdlen;     // len of unprocessed data to keep
        memcpy( decoder_state->in_buff, stream->next_frame, cpylen );
        rdaddr = (uint8_t *) decoder_state->in_buff + cpylen;   // read new data after tail just copied
    }

    showProgress( "R_", 200 );   // decode MP3's

    int len = fread( rdaddr, 1, rdlen, decoder_state->fmp3 );     // fill rest of buffer
    dbgLog( "7 mp3 in: nf=%x addr=%x rdlen=%d len=%d\n", stream->next_frame, rdaddr, rdlen, len );
    if ( len < rdlen ) {  // end of file encountered
        memset( rdaddr + len, 0, rdlen - len ); // set rest of buff to 0
        tbFclose( decoder_state->fmp3 );   // fclose( decoder_state->fmp3 );
        decoder_state->fmp3 = NULL;   // mark as ready to stop
    }

    decoder_state->in_pos += len;
    mad_stream_buffer( stream, &decoder_state->in_buff[0], len );   // if 0 => buff of 0's
    return MAD_FLOW_CONTINUE;
}

static enum mad_flow hdr( void *st, struct mad_header const *stream ) {                                // cb fn for frame header
    //printf("hdr: %x \n", (uint32_t) stream );
    return MAD_FLOW_CONTINUE;
}

static inline int scale( mad_fixed_t sample ) {                                                // TODO: replace with dithering?
    /*
     * The following utility routine performs simple rounding, clipping, and
     * scaling of MAD's high-resolution samples down to 16 bits. It does not
     * perform any dithering or noise shaping, which would be recommended to
     * obtain any exceptional audio quality. It is therefore not recommended to
     * use this routine if high-quality output is desired.
     */

    sample += ( 1L << ( MAD_F_FRACBITS - 16 ));  /* round */

    /* clip */
    if ( sample >= MAD_F_ONE )
        sample = MAD_F_ONE - 1;
    else if ( sample < -MAD_F_ONE )
        sample = -MAD_F_ONE;

    return sample >> ( MAD_F_FRACBITS + 1 - 16 );  /* quantize */
}


static enum mad_flow output( void *st, struct mad_header const *header, struct mad_pcm *pcm ) {   // cb fn to output next frame of .wav
    /*
     * This is the output callback function. It is called after each frame of
     * MPEG audio data has been completely decoded. The purpose of this callback
     * is to output (or play) the decoded PCM audio.
     */
    decoder_state_t * decoder_state = (decoder_state_t *) st;

    unsigned int      nchannels, nsamples;
    mad_fixed_t const *left_ch, *right_ch;

    /* pcm->samplerate contains the sampling frequency */

    // If the frequency hasn't been set, this must be the first time, so write the header.
    if ( decoder_state->freq == 0 ) {
        decoder_state->freq = pcm->samplerate;
        fwrite( wav_header( decoder_state->freq, pcm->channels, 16, MAX_WAV_BYTES ), 1, 44, decoder_state->fwav );
    }

    nchannels = pcm->channels;
    nsamples  = pcm->length;
    left_ch   = pcm->samples[0];
    right_ch  = pcm->samples[1];

    while (nsamples--) {
        signed int sample;

        /* output sample(s) in 16-bit signed little-endian PCM */

        sample = scale( *left_ch++ );
        fputc(( sample >> 0 ) & 0xff, decoder_state->fwav );
        fputc(( sample >> 8 ) & 0xff, decoder_state->fwav );

        if ( nchannels == 2 ) {
            sample = scale( *right_ch++ );
            fputc(( sample >> 0 ) & 0xff, decoder_state->fwav );
            fputc(( sample >> 8 ) & 0xff, decoder_state->fwav );
        }
    }

    return MAD_FLOW_CONTINUE;
}

static void mp3Err( struct mad_stream *stream, int cd ) {
    const char *s = mad_stream_errorstr( stream );
    errLog( "Mp3 decode err %d: %s \n", cd, s );
}

static enum mad_flow error( void *st, struct mad_stream *stream, struct mad_frame *frame ) {    // report mp3 decoding error
    /*
     * This is the error callback function. It is called whenever a decoding
     * error occurs. The error is indicated by stream->error; the list of
     * possible MAD_ERROR_* errors can be found in the mad.h (or stream.h)
     * header file.
     */
    fpos_t errLoc;
    fgetpos( decoder_state.fwav, &errLoc );    // OUTput file position
    if ( errLoc.__pos > 0 || stream->error != MAD_ERROR_LOSTSYNC ) {
        mp3ErrCnt++;
    }
    if ( mp3ErrCnt == 1 ) {
        mp3FirstErrLoc = (int) ( errLoc.__pos );
        mp3FirstErrMsg = (char *) mad_stream_errorstr( stream );
    }
    if ( reportAllMp3Errs )
        logEvtNSNINS( "mp3Err", "nm", mp3Name, "pos", (int) ( errLoc.__pos ), "msg", mad_stream_errorstr( stream ));

    //dbgLog("! Mp3: %s at %d: %s\n", mp3Name, stream->this_frame, s);
    //mp3Err( stream, stream->error );
    //  fprintf(stderr, "decoding error 0x%04x (%s) in buff at offset %d \n",
    //    stream->error, mad_stream_errorstr(stream), dcdr_st->in_pos );
    //    (unsigned int)(stream->this_frame - buffer->start));

    /* return MAD_FLOW_BREAK here to stop decoding (and propagate an error) */
    return MAD_FLOW_CONTINUE;
}


void mp3ToWav( const char *nm ) {   // decode nm.mp3 to nm.wav
    /*
     * This is the function called by main() above to perform all the decoding.
     * It instantiates a decoder object and configures it with the input,
     * output, and error callback functions above. A single call to
     * mad_decoder_run() continues until a callback function returns
     * MAD_FLOW_STOP (to stop decoding) or MAD_FLOW_BREAK (to stop decoding and
     * signal an error).
     */
    if (!initialized) {
        decoder_state.in_buff = tbAlloc(BUFF_SIZ, "mp3buf");
    }
    decoder_state.freq   = 0;
    decoder_state.fmp3   = NULL;
    decoder_state.in_pos = 0;
    decoder_state.fwav   = NULL;
    memset( decoder_state.in_buff, 0, BUFF_SIZ );

    struct mad_decoder decoder;
    int                result;

    /* initialize decode state structure */
    char fnm[40];
    strcpy( fnm, nm );
    char *pdot = strrchr( fnm, '.' );
    if ( pdot ) *pdot = 0; else pdot = &fnm[strlen( fnm )];
    strcpy( pdot, ".mp3" );

    decoder_state.freq = 0;

    decoder_state.fmp3 = tbOpenReadBinary( fnm ); //fopen( fnm, "rb" );

    strcpy( pdot, ".wav" );
    decoder_state.fwav = tbOpenWriteBinary( fnm ); //fopen( fnm, "wb");

    if ( decoder_state.fmp3 == NULL || decoder_state.fwav == NULL ) {
        dbgLog( "7 Mp3 fopen %s: %d %d \n", fnm, decoder_state.fmp3, decoder_state.fwav );
        return;
    }

    /* configure input, output, and error functions */
    mad_decoder_init( &decoder, &decoder_state, input, hdr, 0 /* filter */, output, error, 0 /* message */);
    result = mad_decoder_run( &decoder, MAD_DECODER_MODE_SYNC );  // start decoding
    if ( result != 0 )
        mp3Err( NULL, result );

    mad_decoder_finish( &decoder );  // release the decoder
    if ( decoder_state.fmp3 != NULL ) {
        tbFclose( decoder_state.fmp3 );
        decoder_state.fmp3 = NULL;
    }
    if ( decoder_state.fwav != NULL ) {
        tbFclose( decoder_state.fwav );
        decoder_state.fwav = NULL;
    }
}
