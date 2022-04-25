=  Wave Parser

Read a wav file, get samples information

This is intended for clients to grab a wav file in a small, reusable buffer, making it ideal for use in an embedded application with statically allocated buffers. The processing core maintains a reentrant state machine, and while no mixing or processing is currently done there is a single codepoint to consider for expansion. 

1. Initialize the state machine with `wave_init`
2. Configure a callback to be informed when format information is available
3. Configure a callback to be informed when samples are available
4. In your `samples_ready` callback, send the samples to a DAC, SDL_QueueAudio, etc.
5. Call `wave_process_data` with the next buffer-full of information. If using a DAC with DMA, wait for the DMA's "empty" or "complete" interrupt. 

If the processor runs out of data to process it may return early. In this case, as long as the headers have been read,
a shorter buffer can be sent back to maintain block alignment. A reference example follows:

```C
void did_receive_samples(wave_decoder_t * decoder, uint8_t ch, uint8_t *values, size_t length)
{
  SDL_QueueAudio(dev, values, length);
}

int main (int argc, char const *argv[])
{
  FILE * f = fopen(argv[...], "rb");

  // Set up the decoder
  wave_init(&wav, wavbuf1, wavbuf2, BUF_SIZE);
  wave_set_sample_cb(&wav, &did_receive_samples);
  wave_set_fmt_cb(&wav, &did_receive_format);

  while (!feof(f))
  {
    int read_count = fread(readbuf, 1, BUF_SIZE, fp);
    int proc_count = wave_process_data(&wav, readbuf, read_count);

    if (proc_count < 1)
    {
      ERR("process_data returned %d\n", proc_count);
      return 1;
    }

    if (proc_count < read_count)
    {
      // Not all data was processed; we need to send some back again.
      wave_process_data(&wav, readbuf+proc_count, read_count - proc_count);
    }
  }
}
```
