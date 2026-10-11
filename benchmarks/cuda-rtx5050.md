# CUDA synthesis benchmark: RTX 5050

Measured on the local LXC GPU passthrough machine after GPU recovery, using
S2 Pro Q5_K_M, all 36 transformer layers on CUDA, four CPU threads, CUDA codec,
and CUDA graph replay. The build is Release with CUDA architecture `120a`.
Text: `Hello world. This is a test of speech synthesis.` Generation was held
to 115 frames, giving 235,520 samples at 44,100 Hz (5.34059 seconds).

| Workload | AR generation | Codec decoding | Synthesis total | Real-time factor |
|---|---:|---:|---:|---:|
| No voice profile, greedy sampling | 2.9917 s | 0.6569 s | **3.6497 s** | **0.6834** |
| Saved `yumi` voice, normal sampling, codec auto | 3.3072 s | 0.6301 s | **3.9436 s** | **0.7384** |

Both are faster than real time for this workload. These are individual measured
runs, not a statistical latency estimate. Timings come from `[Metrics] Synthesis`
and exclude model loading, codec backend selection, and writing the output WAV.
The greedy run's initialization took 5.42 seconds; the voice run's automatic
codec selection made initialization take 24.34 seconds. Use a persistent process
and `--codec-follow-backend` to avoid repeating the CPU/GPU benchmark at startup.

The user's earlier measurement was 16.59 seconds. An original-executable control
after recovery measured 6.0037 seconds on the same fixed-length text workload,
using the current GGML libraries. Driver/runtime state and competing processes
changed during this investigation, so 16.59 / 3.65 is not a controlled measurement
of the code changes alone.

## Changes used

- Compute Slow-AR output logits only for semantic tokens and EOS during synthesis.
- Reuse Fast-AR execution graphs and per-layer KV state across sequential codebooks.
- Dequantize the small Fast-AR embedding table into a GPU cache once, avoiding
  repeated CPU gathers and transfers (about 40 MiB extra GPU memory).
- Enable CUDA graph replay by default in new CUDA build directories.
- Use supported fused GPU attention for single-token Slow-AR steps.
- Skip masked logits in sampling and use a direct maximum for greedy sampling.
- Select the codec backend using a warmed 32-frame decode instead of a cold,
  eight-frame decode; suppress backend debug logging in ordinary CLI runs.

The large CUDA embedding tables stay on CPU. Existing local GGML codec patches
were already present in this checkout and are required to reproduce its backend.

## Reproduce the workload

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DS2_CUDA=ON \
  -DGGML_CUDA_GRAPHS=ON -DCMAKE_CUDA_ARCHITECTURES=120a
cmake --build build --parallel 4

./build/s2 --model /opt/tts/models/s2-pro-q5_k_m.gguf \
  --tokenizer tokenizer.json --cuda 0 --gpu-layers -1 \
  --codec-follow-backend --threads 4 \
  --text "Hello world. This is a test of speech synthesis." \
  --max-tokens 115 --min-tokens-before-end 115 --temp 0 --top-k 1 \
  --output /tmp/s2-benchmark.wav
```

For the voice workload, remove `--temp 0 --top-k 1`, add
`--voice yumi --voice-dir voices`, and use `--codec-auto` in place of
`--codec-follow-backend`. Fixed token counts make durations comparable; ordinary
generation can stop sooner at EOS. Architecture `120a` is specific to this GPU;
use the appropriate architecture when building for another device.

## Verification and limits

CUDA and CPU builds, CLI help, and the CPU shared-library ctypes smoke check
passed. The optional model-backed test passed with Q5_K_M on CPU/CUDA and Q6_K
on CUDA. It checks full versus restricted output heads, fused versus unfused
attention, cached versus full-prefix Fast-AR across codebooks and frames, and
nonsequential-prefix fallback.

Restricted-head logits matched exactly in the tested cases. Quantized cached
decode and fused attention introduce small numerical differences: checks require
relative logit L2 error below 1% and sampling-distribution KL below 0.002. These
checks are not a perceptual speech-quality evaluation, and sampled waveforms
need not match the original decoder bit for bit.

Both benchmark WAVs were validated as finite, non-silent mono float32 audio
with the expected 235,520 samples. A 20-frame saved-voice streaming smoke run
also passed and produced a finalized PCM16 WAV. That short streaming run took
1.057 seconds for 0.929 seconds of audio (RTF 1.138); the offline result does
not establish real-time performance for every streaming chunk size.

Local evidence logs: `/tmp/s2-optimized-final.log`, `/tmp/s2-voice-final.log`,
`/tmp/s2-cuda-final-decode-test.log`, `/tmp/s2-cuda-q6-decode-test.log`,
`/tmp/s2-cpu-final-decode-test.log`, and `/tmp/s2-stream-smoke.log`.

## Longer Yumi paragraph: offline codec batching fix

Tested through the existing HTTP server with normal sampling and this text:

> Hi there, I am Yumi, and I am glad you are here. We can start by talking about
> your day, or work through a question together. If something feels difficult,
> we can break it into a few smaller steps and take our time. Tell me what you
> would like to focus on, and I will help you find a clear place to start.

| Run | Frames / audio | AR generation | Codec decoding | Total | RTF |
|---|---|---:|---:|---:|---:|
| Previous offline path; STT loaded | 311 / 14.44 s | 8.400 s | 13.257 s (11 batches) | 21.687 s synthesis | 1.502 |
| Existing buffered streaming path, no holdback; STT loaded | 311 / 14.44 s | not captured | not captured | 12.717 s HTTP request | 0.880 |
| Single offline batch; STT stopped | 336 / 15.60 s | 9.340 s | 1.735 s (1 batch) | 11.093 s synthesis | 0.711 |
| Fixed default offline path; STT stopped | 317 / 14.72 s | 8.599 s | 3.046 s (3 batches) | 11.662 s synthesis | 0.792 |

The 311-frame buffered streaming test fixed the token count for a duration
comparison; its generated content is not an exact reproduction of the original
waveform. The last two tests used natural EOS. Speech length varies with normal
sampling, and the STT process was stopped between the early and late tests, so
these rows do not establish a controlled speedup factor.

The previous offline path treated 144 codec history frames as future holdback
as well as left history. This repeatedly decoded growing prefixes and large
overlapping windows. It spent more time in the codec than in generation. With
STT also loaded (about 1,958 MiB of GPU memory), a single 311-frame decoder buffer
needed about 1.72 GiB and allocation failed.

The default offline path now uses a 64-frame cadence, fills the first window
once, retains the existing left history, and does not hold back future frames.
With this codec its decode windows stay at or below 208 frames. Generation KV
memory is released before decoding. Short utterances fitting in the first window
still use one decode. An explicit cadence or history override can increase the
window size and memory requirement. Server streaming and stream-file defaults
retain their existing settings.

`s2-codec-decode-test` compared decoding 64 saved-voice code frames with the
prefix of a 96-frame decode on CPU. The PCM matched exactly (maximum absolute
error and relative L2 error both zero), supporting removal of future holdback.
It does not establish equivalence to whole-utterance decoding after left history
is truncated. The faster buffered streaming clip's transcription missed the
last few words because its 311-frame cap could truncate this sampled rendition;
the subsequent natural-EOS tests did not use that cap.

CUDA and CPU rebuilds, CLI help, CPU shared-library ctypes smoke, OpenAPI YAML
loading, and finite/non-silent output checks passed. Local output and timing
artifacts are `build/yumi-paragraph-default-fixed.wav` and
`build/yumi-paragraph-default-fixed.json`; the single-batch run is saved as
`build/yumi-paragraph-single-batch-natural.wav` and its matching JSON file.
The causality check log is `/tmp/s2-codec-causality.log`.

A further default-path request ran with a temporary 1,958 MiB CUDA allocation,
matching the measured idle STT process footprint, while the STT server remained
stopped. It succeeded in 11.476 seconds for 14.164 seconds of audio (HTTP RTF
0.810). GPU telemetry sampled every 250 ms peaked at 7,427 MiB used, with 321 MiB
free. The reservation was released after the test. This tests memory capacity
for both loaded footprints, not concurrent ASR compute or larger ASR inputs.
Results are in `build/yumi-paragraph-stt-vram-budget.json`; telemetry is in
`/tmp/s2-paragraph-reserved-vram.csv`. Keep the bounded default cadence when
sharing the GPU; the 1,536-frame single-batch override already failed with STT
loaded.
