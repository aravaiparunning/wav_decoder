#ifndef wave_parser_h
#define wave_parser_h

struct wave_decoder_s;
typedef void wave_sample_cb_t(struct wave_decoder_s * decoder, uint8_t ch, uint8_t *values, size_t length);
typedef void wave_fmt_cb_t(struct wave_decoder_s * decoder);

typedef struct {
  char chunk_id[4];     // e.g. RIFF
  uint32_t chunk_size; 
  char chunk_type[4];   // e.g. WAVE
} wave_header_t;

typedef struct {
  char chunk_id[4];     // e.g. fmt 
  uint32_t chunk_size;
} wave_subchunk_header_t;

typedef struct {
  uint16_t fmt_tag;                // compression code
  uint16_t ch_count;
  uint32_t sample_rate;
  uint32_t byte_rate;
  uint16_t block_align;
  uint16_t sample_width;
} wave_fmt_subchunk_header_t;

// All names imperative
typedef enum {
  wav_decode_no_header = 0,         // expecting to read a chunk header
  wav_ignore_unknown_chunk,         // expecting to ignore bytes in an uninteresting chunk
  wav_decode_read_subchunk_header,  // expecting to read a subchunk header
  wav_decode_read_subchunk_data,    // expecting to read any data in the 
} wav_decode_state_t;

typedef struct wave_decoder_s {
  uint8_t * buf1;
  uint8_t * buf2;
  uint8_t * curbuf;
  uint32_t buf_size;
  wave_sample_cb_t * cb;
  wave_fmt_cb_t * fmt_cb;

  wav_decode_state_t state;
  // uint32_t chunk_loc;  // The location in the file of the last parsed header
  uint32_t skip_until; // set when state is ignore_unknown_chunk
  wave_header_t header;

  wave_subchunk_header_t subchunk_header; // Size of current entry
  wave_fmt_subchunk_header_t fmt;  // This *must* follow the subchunk header entry

  uint32_t processed_bytes;
  
} wave_decoder_t;


void wave_init(wave_decoder_t * decoder, uint8_t * buf1, uint8_t * buf2, size_t buf_size);

// Specify a callback to be called with sample data.
//
static inline void wave_set_sample_cb(wave_decoder_t * decoder, wave_sample_cb_t * cb)
{
  decoder->cb = cb;
}

static inline void wave_set_fmt_cb(wave_decoder_t * decoder, wave_fmt_cb_t * cb)
{
  decoder->fmt_cb = cb;
}

// Return a buffer that is not currently in use. This is limited to
// buf_size provided in the init function.
//
uint8_t * wave_get_free_buf(wave_decoder_t * decoder);

// Get the decoder's currently active buffer
uint8_t * wave_get_cur_buf(wave_decoder_t * decoder);

// Process data read from a source, depending on the state of the decoder.
// This may read a header to determine channel count, bitrate, etc, or it
// may begin triggering sample callbacks, or both.
//
// The buffer must contain an entire header
int32_t wave_process_data(wave_decoder_t * decoder, uint8_t * buf, uint32_t size);
#endif