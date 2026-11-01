# Handwritten backpropagation guide

**Manual candidate action:** rewrite this derivation by hand, sign/date it, and attach a scan or photograph to the submission. This file is a guide only and is not a substitute for the handwritten appendix.

## Assignment

```text
x = 1, y = 2, eta = 0.1
w = 0.5, b = 0, v = 1, c = 0

z = wx + b
h = tanh(z)
y_hat = vh + c
J = 0.5(y_hat - y)^2
```

## Forward pass

```text
z = (0.5)(1) + 0 = 0.5
h = tanh(0.5) = 0.4621171573
y_hat = (1)(0.4621171573) + 0 = 0.4621171573
e = y_hat - y = 0.4621171573 - 2 = -1.5378828427
J = 0.5 e^2 = 1.1825418190
```

## Chain rule

```text
dJ/dy_hat = e = -1.5378828427
dJ/dv = e h = (-1.5378828427)(0.4621171573) = -0.7106820475
dJ/dc = e = -1.5378828427

1 - h^2 = 1 - (0.4621171573)^2 = 0.7864477329
dJ/dw = e v (1-h^2) x
       = (-1.5378828427)(1)(0.7864477329)(1)
       = -1.2094644752
dJ/db = e v (1-h^2)
       = -1.2094644752
```

## Simultaneous update

All gradients above use the original parameters. Update only after all four are known:

```text
w_new = 0.5 - 0.1(-1.2094644752) = 0.6209464475
b_new = 0   - 0.1(-1.2094644752) = 0.1209464475
v_new = 1   - 0.1(-0.7106820475)  = 1.0710682047
c_new = 0   - 0.1(-1.5378828427)  = 0.1537882843
```

The updated loss is approximately `0.685773765`. Training then begins from these updated parameters for 100 additional steps and converges to approximately `1.080495e-20` for the recorded default run.

## Finite differences

For each parameter theta, perturb only that parameter with epsilon = 1e-6 and recompute the loss:

```text
numerical gradient = [J(theta + epsilon) - J(theta - epsilon)] / (2 epsilon)
```

Compare each numerical value with the analytical value. The implementation’s recorded maximum absolute difference is approximately `1.13e-10`, below the required `1e-5` tolerance. The four rows are `w`, `b`, `v`, and `c`, and all are PASS.
