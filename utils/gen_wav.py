
import struct
import math
import sys

def create_wav(filename, freq):
    sample_rate = 44100
    duration = 0.5
    num_samples = int(sample_rate * duration)
    
    # Header
    riff_header = b'RIFF'
    wave_header = b'WAVE'
    fmt_header = b'fmt '
    data_header = b'data'
    
    audio_format = 1 # PCM
    channels = 1
    sample_width = 2
    byte_rate = sample_rate * channels * sample_width
    block_align = channels * sample_width
    
    pcm_data = bytearray()
    for i in range(num_samples):
        t = float(i) / sample_rate
        sample = 0.5 * math.sin(2.0 * math.pi * freq * t)
        val = int(sample * 32767)
        pcm_data.extend(struct.pack('<h', val))
        
    chunk_size = 36 + len(pcm_data)
    
    with open(filename, 'wb') as f:
        f.write(riff_header)
        f.write(struct.pack('<I', chunk_size))
        f.write(wave_header)
        f.write(fmt_header)
        f.write(struct.pack('<I', 16)) # Subchunk1Size
        f.write(struct.pack('<H', audio_format))
        f.write(struct.pack('<H', channels))
        f.write(struct.pack('<I', sample_rate))
        f.write(struct.pack('<I', byte_rate))
        f.write(struct.pack('<H', block_align))
        f.write(struct.pack('<H', sample_width * 8))
        f.write(data_header)
        f.write(struct.pack('<I', len(pcm_data)))
        f.write(pcm_data)
    print(f"Created {filename}")

create_wav('samples/test_audio/sfx1.wav', 440.0) # A4
create_wav('samples/test_audio/sfx2.wav', 880.0) # A5
