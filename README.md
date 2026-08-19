# BlueGachiin

BlueGachiin is a proper, high-survivability steganographic watermarking engine. It embeds a 64-byte cryptographic signature directly into the high-frequency spatial domain of the image. It is designed to survive real-world host interference, and it is written in C because we care about memory layout and execution speed, not bloated runtimes.

## Architecture

This isn't a magic trick; it's just applied signal processing. The architecture relies on four core mechanisms to guarantee survivability:

**1. 2D Haar Wavelet Transform (DWT)**:
We don't operate on raw pixels. We slice the image into frequency subbands using a 2D Haar transform. The payload is injected specifically into the HL (High-Low) subband. Why? Because modifying the LL subband destroys the image visually, and the HH subband gets aggressively truncated by standard compression. HL is the sweet spot.

**2. Direct Sequence Spread Spectrum (DSSS)**:
Instead of mapping one payload bit to one pixel, we spread each bit across 16-chip pseudo-noise sequences. The payload masquerades as high-frequency static. It is mathematically indistinguishable from natural image noise unless you have the exact PRNG seed used to generate the spread sequence.

**3. Blue Channel Zero-Forcing**:
Host image interference will drown out your signal if the natural frequency of the image spikes in the exact location you are trying to write. Instead of adding our signal to the existing noise, we completely overwrite the localized frequency in the Blue color channel. We zero-force the host interference. It guarantees an absolute, perfectly scaled correlation sum on extraction.

**4. Forward Error Correction (Reed-Solomon)**:
Bit flips are inevitable in the wild. If you aren't using error correction, you are just waiting for your data to silently corrupt. We use libfec to wrap the 64-byte payload in 32 bytes of Reed-Solomon parity. If the image takes damage, the Medbay heals the extracted byte array before it ever reaches user-space.

## instalation 

