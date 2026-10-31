# Thermal Studio numerical method

Thermal Studio solves the steady one-dimensional rod problem

`-d/dx(k A dT/dx) = q A`

with constant conductivity, area, and volumetric generation. Two-node linear elements are used. For an element of length `l_e`, the element matrices are:

```text
K_e = (k A / l_e) [ [ 1, -1 ], [ -1, 1 ] ]
F_e = (q A l_e / 2) [ 1, 1 ]^T
```

The solver assembles these contributions into the global stiffness matrix and load vector. Prescribed temperatures at the first and last node are applied by adjusting the remaining right-hand side for the known column, zeroing the constrained row and column, setting the diagonal to one, and setting the constrained load to the prescribed temperature. The resulting dense system is solved with partial-pivot Gaussian elimination.

For the assignment case, the independent analytical solution is:

`T_exact(x) = T_left + (T_right - T_left) x/L + [q/(2k)] x(L-x)`

At each element midpoint, the FE temperature is the linear interpolation of the two neighboring nodal temperatures. The reported error is the absolute difference between that interpolated value and the analytical value at the same midpoint; nodal values alone are not used for the required midpoint error.

## Verification

The exporter creates the actual CSV and SVG artifacts in `results/thermal/`. The measured default-case maximum midpoint errors are:

| Elements | Element length (m) | Maximum midpoint error (°C) |
|---:|---:|---:|
| 10 | 0.10000 | 1.25000 |
| 20 | 0.050000 | 0.312500 |
| 40 | 0.025000 | 0.078125 |
| 80 | 0.012500 | 0.0195313 |

The approximately fourfold reduction under each mesh refinement is the expected second-order midpoint interpolation trend for this quadratic exact field. A genuine implementation insight was that the FEM nodal solution reproduces the analytical nodal temperatures for this uniform problem, while the required midpoint interpolation still has a nonzero error; comparing nodal values alone would incorrectly report zero error.

Generate the artifacts with:

```text
./build/nexus-thermal-export results/thermal
```
