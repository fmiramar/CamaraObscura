# rir2fdn

`rir2fdn` is the deterministic, dependency-free analyzer for the late-only
`RIRFDN` renderer. It reads integer PCM WAV, detects the direct peak, fits
broadband and two-band Schroeder energy decays, writes the versioned model,
renders a verification impulse, and writes a plain-text report.

```bash
python3 rir2fdn.py analyze input.wav hall.rfdn \
    --late-start 0.08 --delays 8 --sample-rate 48000
```

The tool deliberately does not copy early reflections into the FDN model.
Keep them as an external FIR or use `GeometryReverb`.
