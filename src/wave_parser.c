#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "wave_parser.h"

#ifdef WAVE_DEBUG
#define DEBUG_P(fmt, ...) printf(fmt, VA_ARGS)
#else
#define DEBUG_P(fmt, ...)
#endif

void wave_init(wave_decoder_t * decoder, uint8_t * buf1, uint8_t * buf2, size_t buf_size)
{
  memset(decoder, '\0', sizeof(wave_decoder_t));
  if (buf_size < 64)
  {
    // Unsupported small buffer
    return;
  }
  
  decoder->buf1 = buf1;
  decoder->buf2 = buf2;
  decoder->buf_size = buf_size;
  decoder->cb = NULL;
  decoder->volume = 0.9;
  
}

static bool wave_match4(const void *needle, const void * haystack)
{
  uint8_t * n = (uint8_t *)needle;
  uint8_t * h = (uint8_t *)haystack;
  for(size_t i = 0; i < 4; ++i)
  {
    if (n[i] != h[i]) return false;
  }
  return true;
}

uint8_t * wave_get_free_buf(wave_decoder_t * decoder)
{
  if (decoder->curbuf == decoder->buf1) 
    return decoder->buf2;
  else
    return decoder->buf1;
}


uint8_t * wave_get_cur_buf(wave_decoder_t * decoder)
{
  if (decoder->curbuf == decoder->buf1) 
    return decoder->buf1;
  else
    return decoder->buf2;
}


static uint32_t read_header(wave_header_t * header, uint8_t * buf)
{
  if (NULL == header || NULL == buf) 
    return 0;
  
  const char * header_type = ((wave_header_t*)buf)->chunk_id;
  const uint32_t len = ((wave_header_t*)buf)->chunk_size;
  DEBUG_P("=> Reading header '%.4s' (%u bytes)\n", header_type, len);
  
  memcpy(header, buf, sizeof(wave_header_t));

  return sizeof(wave_header_t);
}

static uint32_t read_fmt_subchunk_header(wave_subchunk_header_t * header, uint8_t * buf)
{
  if (NULL == header || NULL == buf) 
    return 0;
    
  const char * header_type = ((wave_subchunk_header_t*)buf)->chunk_id;
  uint32_t len = ((wave_subchunk_header_t*)buf)->chunk_size;
  DEBUG_P("=> Reading subchunk header '%.4s' (%u bytes)\n", header_type, len);

  // If we're reading a fmt subchunk header, blast right through the
  // 8-byte generic header struct entry and fill the fmt structure as well.
  //
  len = sizeof(wave_subchunk_header_t);
  if (wave_match4(header_type, "fmt "))
  {
    len += sizeof(wave_fmt_subchunk_header_t);
  }
  
  memcpy(header, buf, len);

  return len;
}

const char * wave_state_name(wav_decode_state_t s)
{
  switch(s)
  {
    case wav_decode_no_header: return "wav_decode_no_header";
    case wav_ignore_unknown_chunk: return "wav_ignore_unknown_chunk";
    case wav_decode_read_subchunk_header: return "wav_decode_read_subchunk_header";
    case wav_decode_read_subchunk_data: return "wav_decode_read_subchunk_data";
    default: return "<unknown>";
  }
}


int32_t wave_process_data(wave_decoder_t * decoder, uint8_t * buf, uint32_t size)
{
  uint32_t sz;
  uint8_t * ptr = buf;
  
  #define PROCESSED(_count) do { /*DEBUG_P("PROCESSED %d\n", _count);*/ size -= _count;  decoder->processed_bytes += _count; ptr += _count; } while (0);

#ifdef DEBUG_WAV    
  if (decoder->state == wav_decode_read_subchunk_data)
    DEBUG_P("wave: process in state %s from %d\n", wave_state_name(decoder->state), decoder->processed_bytes);
  else
    DEBUG_P("wave: process in state %s from %d (%c %c %c %c)\n", wave_state_name(decoder->state), decoder->processed_bytes,
      buf[0], buf[1], buf[2], buf[3]);
#endif
  
  switch (decoder->state) {
    
    case wav_ignore_unknown_chunk: {
      DEBUG_P("ignore from %d until %d\n", decoder->processed_bytes, decoder->skip_until);
      // The location of the header is in decoder->chunk_loc. We need
      // to ignore bytes until decoder->processed_bytes equals chunk_lock + header.chunk_size
      uint32_t chunk_end = decoder->skip_until;
      if (chunk_end < decoder->processed_bytes)
      {
        DEBUG_P("wave: invalid chunk size");
        return -1;
      }
      
      uint32_t delta = chunk_end - decoder->processed_bytes;
      if (delta > size) 
      {
        // We need to ignore everything left in the given buffer
        decoder->processed_bytes += size;
        DEBUG_P("wave: ignore: processed %d (line %d)\n", size, __LINE__);
        return size;
      }
      else
      {
        decoder->processed_bytes += delta;
        decoder->state = wav_decode_read_subchunk_header;
        DEBUG_P("ignore done (now at %d)\n", decoder->processed_bytes);
        DEBUG_P("wave: ignore: processed %d (line %d)\n", delta, __LINE__);
        return delta;
      }
    }
    /* Fall through */
    
    case wav_decode_no_header: {
      if (size < sizeof(wave_header_t))
      {
        // we can't read this header. In fact given that this
        // is a main structure, we can't really do anything.
        DEBUG_P("wave: could not read header\n");
        return ptr - buf;
      }
      
      uint32_t chunk_loc = decoder->processed_bytes;
      DEBUG_P("wave: will read header at %d\n", chunk_loc);
      sz = read_header(&decoder->header, ptr);
      PROCESSED(sz);
      
      wave_header_t * h = &decoder->header;
      
      DEBUG_P("File type:  %.4s\n", h->chunk_id);
      DEBUG_P("Chunk type: %.4s\n", h->chunk_type);
      DEBUG_P("Chunk size: %d\n", h->chunk_size);

      if (!wave_match4(h->chunk_id, "RIFF") || !wave_match4(h->chunk_type, "WAVE"))
      {
        // This chunk is unknown.
        DEBUG_P("[UNSUPPORTED CHUNK]\n");
        DEBUG_P("skip %d-%d\n", chunk_loc, chunk_loc + h->chunk_size);
        decoder->skip_until = chunk_loc + h->chunk_size;
        decoder->state = wav_ignore_unknown_chunk;
        return ptr - buf;
        // bytes to ignore are in the header size.
        // just need to wait until processed bytes = header_size + header_loc.
      }
      else
      {
        // Time to go read a subchunk
        decoder->state = wav_decode_read_subchunk_header;
      }
      
      if (size <= 0)
        break;

      // Can keep going, I guess.
    }
    /* FALL THROUGH */
    
    case wav_decode_read_subchunk_header: {
      DEBUG_P("=> read subchunk header at %ld\n", ptr - buf);
      // uint32_t required_size = *(uint32_t*)(ptr + 4);
      // if (size < required_size)
      // {
      //   DEBUG_P("wave: buf of %d too small for subchunk header (required size %u)\n", size, required_size);
      //   DEBUG_P("wave: data at ptr: %02x %02x %02x %02x (%c %c %c %c)\n",
      //     ptr[0], ptr[1], ptr[2], ptr[3],
      //     ptr[0], ptr[1], ptr[2], ptr[3]);
      //   return ptr - buf;
      // }
      
      // HACK: The called code is happy to overflow right through
      // the subchunk_header structure into the fmt_subchunk_header
      // structure immediately following it. 
      // 
      // subchunk_header will contain the length of the subchunk 
      // currently being processed. The fmt header will contain the
      // last fmt information that was read.
      //
      uint32_t subchunk_header_loc = decoder->processed_bytes;
      DEBUG_P("=> will read subchunk header at %d\n", subchunk_header_loc);
      sz = read_fmt_subchunk_header(&decoder->subchunk_header, ptr);
      PROCESSED(sz);
      
      DEBUG_P("wave: read chunk header; ptr now: %02x %02x %02x %02x (%c %c %c %c)\n",
        ptr[0], ptr[1], ptr[2], ptr[3],
        ptr[0], ptr[1], ptr[2], ptr[3]);
      
      // The fmt  subchunk is mandatory.
      if (wave_match4(decoder->header.chunk_type, "WAVE"))
      {
        if (wave_match4(decoder->subchunk_header.chunk_id, "fmt "))
        {
          // So we have a subchunk now, which is pretty neat.
          // Expect data next
          //
          DEBUG_P("ch_count: %d\n", decoder->fmt.ch_count);
          DEBUG_P("sample_rate: %d\n", decoder->fmt.sample_rate);
          DEBUG_P("byte_rate: %d\n", decoder->fmt.byte_rate);
          DEBUG_P("block_align: %d\n", decoder->fmt.block_align);
          DEBUG_P("sample_width: %d\n", decoder->fmt.sample_width);
          
          if (NULL != decoder->fmt_cb)
            decoder->fmt_cb(decoder);
          
          // decoder->state = wav_decode_read_subchunk_data;
          // Read next chunk
          decoder->state = wav_decode_read_subchunk_header;
          
          // We aren't guaranteed to get a data subchunk next
          return ptr - buf;
        }
        else
        if (wave_match4(decoder->subchunk_header.chunk_id, "data"))
        {
          // Actual sample data, finally. So I guess just read the
          // rest of the buffer into samples.
          /* FALL THROUGH */
          DEBUG_P("wave: FOUND data chunk\n");
          decoder->state = wav_decode_read_subchunk_data;
        }
        else
        {
          // We got something that's not 'fmt ' or 'data', so need to skip it.
          DEBUG_P("[UNSUPPORTED SUBCHUNK]\n");

          // Skip the rest of this chunk, including the 8 bytes that have already been read.
          //
          decoder->skip_until = subchunk_header_loc + decoder->subchunk_header.chunk_size + 8;
          decoder->state = wav_ignore_unknown_chunk;

          DEBUG_P("skip %d-%d (line %d)\n", subchunk_header_loc, decoder->skip_until, __LINE__);
          DEBUG_P("wave: processed %ld\n", ptr - buf);

          // We aren't guaranteed to get a data subchunk next
          return ptr - buf;
        }
      }
    }
    
    /* FALL THROUGH */
    
    case wav_decode_read_subchunk_data: {
      // DEBUG_P("wave: wav_decode_read_subchunk_data\n");
      // Hokay, so, we are now pointing at some samples.
      // We have `size` bytes left to go. Let's go for it.
      // DEBUG_P("samples: %d bytes\n", size);
      uint8_t channel_byte_align = (decoder->fmt.sample_width / 8);
      uint8_t * buf = wave_get_free_buf(decoder);
      uint8_t * endbuf = buf + size;
      decoder->curbuf = buf;
      
      for(; buf < endbuf; )
      {
        // Fill each channel
        for(uint8_t ch=0; ch < decoder->fmt.ch_count; ch++)
        {
          if (ch == 0)
          {
#ifdef WAV_NO_VOLUME
            for (uint8_t b=0; b<channel_byte_align; b++)
            {
              // DEBUG_P("-> wr %d %p %d\n", b, buf, endbuf-buf);
              buf[b] = *ptr++;
            }
#else
            switch(channel_byte_align) {
              case 2: {
                int16_t samp = *ptr++;
                samp |= (*ptr++ << 8);
                samp = ((samp - ((uint16_t)0xffff >> 1)) * decoder->volume) + (4095 * 0.5);
                
                *((int16_t*)buf) = samp;

                break;
              }
              
              case 1: {
                uint8_t samp = *ptr++;
                samp *= decoder->volume;
                samp = ((samp - ((uint16_t)0xffff >> 1)) * decoder->volume) + (4095 * 0.5);
                *buf = samp;
                break;
              }
              
              case 4: {
                uint32_t samp;
                samp |= *ptr++;;
                samp |= (*ptr++ << 8);
                samp |= (*ptr++ << 16);
                samp |= (*ptr++ << 24);

                samp = ((samp - ((uint32_t)0xffffffff >> 1)) * decoder->volume) + (4095 * 0.5);

                *((int32_t*)buf) = samp;
                break;
              }
            }
#endif /* WAV_NO_VOLUME */
          }

          buf += channel_byte_align;
        }
      }
      
      decoder->cb(decoder, 0, decoder->curbuf, size);
      
      return size;
    }
  }

  DEBUG_P("proc done; not sure how much was processed.\n");
  return 0;    
}
