# BlueGachiin

BlueGachiin is an ultra-resilient, bare-metal steganographic watermarking engine designed to anchor a 64-byte cryptographic signature directly into the mid-frequency spatial domain of an image. Built for maximum survivability against hostile real-world interference, compression artifacts, and tampering, it bypasses bloated runtimes and heavyweight abstractions to execute in native C—delivering strict deterministic memory layouts, cache-aligned efficiency, and blistering bare-metal speed.

---

## Architecture

### PNG:

**1. Error Shielding (Reed-Solomon FEC)**

The raw secret payload is fortified using **Reed-Solomon Forward Error Correction** (`libfec`) with **32 parity bytes**. This introduces a mathematical safety net, allowing full reconstruction of the hidden message even in the event of localized noise or bit degradation.

**2. Frequency Decomposition (2D Haar Wavelet Transform)**

- Isolates the **Blue color channel**, capitalizing on the Human Visual System's (HVS) lower spatial and chromatic sensitivity to high-frequency blue alterations.
- Decomposes the spatial pixel data via a **2D Haar Wavelet Transform (DWT)** into four distinct subbands:
  - **LL**: Low-frequency approximations
  - **LH**: Horizontal high frequencies
  - **HL**: Vertical details
  - **HH**: Diagonal high frequencies

**3. Spectral Spreading (DSSS across HL Subband)**

- Modulates the RS-encoded bitstream using **Direct-Sequence Spread Spectrum (DSSS)**.
- Spreads the energy of each payload bit across pseudorandom sequences targeting the **HL (vertical detail) wavelet subband coefficients**.
- By distributing payload power beneath noise floors, the embedded signature evades standard spatial histogram anomalies and visual artifacts.

**4. Spatial Synthesis & Export**

- Applies the **2D Inverse Haar Wavelet Transform (IDWT)** to reconstruct the modified blue channel back to spatial pixel values.
- Merges the color planes and encodes the lossless PNG image file to disk using `stbi_image_write.h`.


### JPEG:

**1. Pre-Embedding: Reed-Solomon FEC Protection**

Before any frequency-domain manipulation occurs, the raw payload is processed with **Reed-Solomon Forward Error Correction (FEC)** via `libfec`. This guarantees recovery against burst errors, bit-flips, and localized coefficient decay.

**2. Intercepting the Grids (Frequency Domain Extraction)**

- Utilizes `jpeg_read_coefficients()` from `libjpeg`.
- Performs **Huffman Decoding** and **Run-Length Decoding** directly into raw quantized **8×8 Discrete Cosine Transform (DCT)** coefficient matrices.
- Eliminates spatial domain decompression/recompression generational loss.

**3. Scattering the Blocks (PRNG Keyed Permutation)**

- Filters out all DC components (preserving fundamental lighting/color tone) and zero-value AC coefficients.
- Constructs a linearized array of **valid non-zero AC coefficients**.
- Executes a cryptographic **Fisher-Yates Shuffle** seeded with a private shared key ($K_{shared}$), scattering payload bit targets pseudorandomly across the entire image canvas to defeat spatial and localized clustering steganalysis.

**4. Injecting the Payload (Matrix Encoding via Hamming Codes)**

- Applies $(2^k - 1, k)$ **Hamming-based Matrix Encoding** to drastically boost embedding efficiency.
- Embeds $k$ payload bits per block while altering at most **one single coefficient** per code block.
- **Histogram Preservation**: Modifications are strictly performed by decrementing the absolute value of the coefficient toward zero ($|c| - 1$), preserving the natural Laplace/bell-curve distribution of JPEG DCT coefficients and preventing Chi-square ($\chi^2$) steganalysis detection.

**5. Re-sealing the Grids (Lossless Recompression)**

- Re-populates the modified coefficient blocks back into the JPEG structure.
- Re-encodes the image stream with **Huffman Encoding** using `jpeg_write_coefficients()` from `libjpeg`.
- Generates a fully compliant, standard-compatible JPEG image indistinguishable from conventional compression artifacts.

---

## Dependencies:

- **`stb_image_write.h`**: Image writing and PNG serialization
- **`stb_image.h`**: Lightweight pixel and channel extraction
- **`libjpeg`**: Direct access to DCT coefficient arrays via low-level transcoding API
- **`libfec`**: High-performance Reed-Solomon forward error correction

---

## Build & Run:

```bash
make
./test.sh <-e|-d>       # -e for Encoding, -d for Decoding
```

**Clean generated artifacts**:
```bash
make clean
```

---

## LICENSE

This project is developed and maintained under the [Apache 2.0 License](LICENSE).

---
