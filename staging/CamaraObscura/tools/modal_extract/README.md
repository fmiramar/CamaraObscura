# modal_extract

`modal_extract` creates a measured `ModalReverbModel` without third-party
libraries. It selects refined FFT peaks, estimates two-band Schroeder decay,
normalizes modal residues, and writes the shared versioned text format.

```bash
python3 modal_extract.py analyze input.wav room.modal \
    --modes 512 --minimum-frequency 40
```

This first estimator is deliberately transparent and reproducible. It is not
a high-resolution matrix-pencil estimator, and its report states the spectral
resolution and decay-fit limits.

