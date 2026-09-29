# Benchmarks

Configure the dependency-free native kernels without building a server plugin:

```sh
cmake -S . -B build-benchmark \
  -DSCSYNTH=OFF -DSUPERNOVA=OFF -DBUILD_TESTING=OFF \
  -DCREATIVE_REVERBS_BUILD_BENCHMARKS=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-benchmark --parallel
./build-benchmark/creative_reverbs_dsp_benchmark
```

The executable measures process time per sample with `steady_clock` and
converts it to the percentage of one core at a nominal 48 kHz. It is a native
kernel benchmark: it excludes scsynth graph scheduling, audio-driver work,
model upload, UGen construction, and Dark Velvet impulse/FFT preparation.
Partitioned-convolution figures amortize FFT bursts across 48,000 samples.

See [RESULTS_2026-07-30.md](RESULTS_2026-07-30.md) for measured conditions,
results, memory accounting, and interpretation.
