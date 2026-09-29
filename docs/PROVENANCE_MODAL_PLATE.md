# Modal plate provenance

Primary reference:

- Michele Ducceschi and Craig J. Webb, “Plate Reverberation: Towards the
  Development of a Real-Time Physical Model for the Working Musician,”
  International Congress on Acoustics, 2016.
  <https://www.ica2016.org.ar/ica2016proceedings/ica2016/ICA2016-0559.pdf>

The publication and standard Kirchhoff-Love theory were used for the physical
plate interpretation. No simulation source, data, or measured plate response
was copied.

For a simply supported rectangle this implementation derives

`f_mn = (pi/2) * sqrt(D/(rho*h)) * ((m/a)^2 + (n/b)^2)`

and uses `sin(m*pi*x) sin(n*pi*y)` excitation and pickup shapes. Residues are
recomputed once per block and smoothed for 20 ms. Frequency-dependent modal
T60 is derived from the supplied proportional damping coefficient.

Only simply supported boundaries are claimed. Runtime pitch and excitation
drive are artistic operations; they are not physical nonlinear tension.

Implementation license: GPL-3.0-or-later.
