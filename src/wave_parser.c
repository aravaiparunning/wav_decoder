#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "wave_parser.h"


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
  printf("=> Reading header '%.4s' (%u bytes)\n", header_type, len);
  
  memcpy(header, buf, sizeof(wave_header_t));

  return sizeof(wave_header_t);
}


static uint32_t read_fmt_subchunk_header(wave_subchunk_header_t * header, uint8_t * buf)
{
  if (NULL == header || NULL == buf) 
    return 0;
    
  const char * header_type = ((wave_subchunk_header_t*)buf)->chunk_id;
  uint32_t len = ((wave_subchunk_header_t*)buf)->chunk_size;
  printf("=> Reading subchunk header '%.4s' (%u bytes)\n", header_type, len);

  // If we're reading a fmt subchunk header, blast right through the
  // 8-byte generic header struct entry and fill the fmt structure as well.
  //
  len = sizeof(wave_subchunk_header_t);
  if (0 <= strstr(header_type, "fmt "))
  {
    len += sizeof(wave_fmt_subchunk_header_t);
  }
  
  memcpy(header, buf, len);

  return len;
}


int32_t wave_process_data(wave_decoder_t * decoder, uint8_t * buf, uint32_t size)
{
  uint32_t sz;
  uint8_t * ptr = buf;
  
  #define PROCESSED(_count) do { /*printf("PROCESSED %d\n", _count);*/ size -= _count;  decoder->processed_bytes += _count; ptr += _count; } while (0);
  
  switch (decoder->state) {
    
    case wav_ignore_unknown_chunk: {
      printf("ignore chunk %.4s\n", decoder->header.chunk_id);
      // The location of the header is in decoder->chunk_loc. We need
      // to ignore bytes until decoder->processed_bytes equals chunk_lock + header.chunk_size
      uint32_t chunk_end = decoder->chunk_loc + decoder->header.chunk_size;
      if (chunk_end < decoder->processed_bytes)
      {
        printf("wave: invalid chunk size");
        return -1;
      }
      
      uint32_t delta = chunk_end - decoder->processed_bytes;
      if (delta > size) 
      {
        // We need to ignore everything left in the given buffer
        decoder->processed_bytes += size;
        return size;
      }
      else
      {
        decoder->processed_bytes += delta;
        decoder->state = wav_decode_no_header;
      }
    }
    /* Fall through */
    
    case wav_decode_no_header: {
      if (size < sizeof(wave_header_t))
      {
        // we can't read this header. In fact given that this
        // is a main structure, we can't really do anything.
        printf("wave: could not read header\n");
        return ptr - buf;
      }
      
      decoder->chunk_loc = decoder->processed_bytes;
      sz = read_header(&decoder->header, ptr);
      PROCESSED(sz);
      
      wave_header_t * h = &decoder->header;
      
      printf("File type:  %.4s\n", h->chunk_id);
      printf("Chunk type: %.4s\n", h->chunk_type);
      printf("Chunk size: %d\n", h->chunk_size);
      
      if (0 > strstr(h->chunk_id, "RIFF") || 
          0 > strstr(h->chunk_type, "WAVE"))
      {
        // This chunk is unknown.
        printf("[UNSUPPORTED]\n");
        decoder->state = wav_ignore_unknown_chunk;
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
      uint32_t required_size = *(uint32_t*)(ptr + 4);
      if (size < required_size)
      {
        printf("wave: buf too small for subchunk header\n");
        return ptr - buf;
      }
      
      // HACK: The called code is happy to overflow right through
      // the subchunk_header structure into the fmt_subchunk_header
      // structure immediately following it. 
      // 
      // subchunk_header will contain the length of the subchunk 
      // currently being processed. The fmt header will contain the
      // last fmt information that was read.
      //
      sz = read_fmt_subchunk_header(&decoder->subchunk_header, ptr);
      PROCESSED(sz);
      
      // The fmt  subchunk is mandatory.
      if (0 <= strstr(decoder->header.chunk_type, "WAVE"))
      {
        if (0 <= strstr(decoder->subchunk_header.chunk_id, "fmt "))
        {
          // So we have a subchunk now, which is pretty neat.
          // Expect data next
          //
          printf("ch_count: %d\n", decoder->fmt.ch_count);
          printf("sample_rate: %d\n", decoder->fmt.sample_rate);
          printf("byte_rate: %d\n", decoder->fmt.byte_rate);
          printf("block_align: %d\n", decoder->fmt.block_align);
          printf("sample_width: %d\n", decoder->fmt.sample_width);
          
          if (NULL != decoder->fmt_cb)
            decoder->fmt_cb(decoder);
          
          decoder->state = wav_decode_read_subchunk_data;
          
        }
        else
        if (0 <= strstr(decoder->subchunk_header.chunk_id, "data"))
        {
          // Actual sample data, finally. So I guess just read the
          // rest of the buffer into samples.
          /* FALL THROUGH */
        }
        else
        {
          // We got something that's not fmt, so need to skip it.
          // @TODO should we add a skip_unknown_bytes field? For now
          // @TODO we just ignore the rest of the chunk, including any
          // @TODO subchunks after it. That's probably fine, really.
          decoder->state = wav_ignore_unknown_chunk;
          break;
        }
      }
    }
    
    /* FALL THROUGH */
    
    case wav_decode_read_subchunk_data: {
      // Hokay, so, we are now pointing at some samples.
      // We have `size` bytes left to go. Let's go for it.
      // printf("samples: %d bytes\n", size);
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
            // for(int8_t b=channel_byte_align-1; b>=0; b--)
            for (uint8_t b=0; b<channel_byte_align; b++)
            {
              // printf("-> wr %d %p %d\n", b, buf, endbuf-buf);
              buf[b] = *ptr++;
            }
          }

          buf += channel_byte_align;
        }
      }
      
      decoder->cb(decoder, 0, decoder->curbuf, size);
      
      return size;
    }
  }

  printf("proc done; not sure how much was processed.\n");
  return 0;    
}
