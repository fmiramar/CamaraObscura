# Modal reverberator provenance

Primary reference:

- Jonathan S. Abel and Kurt James Werner, “Distortion and Pitch Processing
  Using a Modal Reverberator Architecture,” DAFx-15.
  <https://dafx.de/paper-archive/2015/DAFx-15_submission_72.pdf>

The paper supplied the parallel damped-mode architecture and its creative
pitch/distortion context. No reference source or mode set was copied.

CamaraObscura uses structure-of-arrays complex one-pole state with the
explicit amplitude convention `r = 10^(-3/(T60*fs))`. Phase sine/cosine is
prepared once, coefficients update at block cadence, excitation drive is
bounded, and freeze approaches a radius below one. The model supports
frequency, T60, input residue, stereo residues, and phase per mode.

The optional extractor is an original transparent FFT-peak approximation with
two-band Schroeder decay. It is not a high-resolution modal-identification
algorithm and documents its FFT resolution and noise sensitivity.

Implementation license: GPL-3.0-or-later.
