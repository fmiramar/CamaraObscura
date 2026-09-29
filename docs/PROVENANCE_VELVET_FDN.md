# Velvet FDN provenance

Primary reference:

- Jon Fagerström, Benoit Alary, Sebastian J. Schlecht, and Vesa Välimäki,
  “Velvet-Noise Feedback Delay Network,” DAFx-20.
  <https://dafx2020.mdw.ac.at/proceedings/papers/DAFx2020_paper_23.pdf>

The paper was used for the sparse velvet-filter diffusion and filtered
feedback-matrix concepts. No source, filters, measurements, or presets were
copied.

Mode 0 places deterministic fixed sparse FIRs at the FDN input branches and
stereo output branches. Mode 1 is an original explicitly paraunitary
construction `H D(z) H`, using normalized Hadamard transforms and pure
per-line delays. A 1024-frequency test verifies `A(z)^H A(z) = I` to numerical
precision before attenuation.

The format limits taps to 16 and lines to 32. Density selects a prepared tap
prefix and diffusion crossfades sparse versus direct paths; taps are not
mutated inside feedback.

Implementation license: GPL-3.0-or-later.
