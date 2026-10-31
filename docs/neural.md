# Neural Lab

Neural Lab implements the exact single-sample regression assignment in C++17 without automatic differentiation or machine-learning frameworks.

## Model

The input and target are `x = 1` and `y = 2`. The network is:

```text
z = wx + b
h = tanh(z)
y_hat = vh + c
J = 0.5 (y_hat - y)^2
```

The default parameters are `w = 0.5`, `b = 0`, `v = 1`, `c = 0`, with learning rate `eta = 0.1`.

## Manual backpropagation

The implementation evaluates the forward pass first, then applies the chain rule using the original parameters:

```text
dJ/dy_hat = y_hat - y
dJ/dv = (y_hat - y) h
dJ/dc = y_hat - y
dJ/dw = (y_hat - y) v (1 - h^2) x
dJ/db = (y_hat - y) v (1 - h^2)
```

All four gradients are calculated before any parameter is changed. The update is simultaneous:

```text
theta_new = theta_old - eta * gradient
```

## Finite-difference validation

Each initial gradient is independently checked with a central difference using `epsilon = 1e-6`:

```text
[J(theta + epsilon) - J(theta - epsilon)] / (2 epsilon)
```

Every absolute difference must be at most `1e-5`. The API returns analytical value, numerical value, absolute difference, and pass/fail status for `w`, `b`, `v`, and `c`.

## Training

Training starts from the parameters after the required first update. The backend performs 100 additional gradient-descent steps and returns loss at step 0 through step 100. For the default assignment, the initial loss is approximately `1.182541819`, the required first-update loss is approximately `0.685773765`, and the final loss is approximately `1.080495e-20`. The exponential decrease is expected for this simple one-sample, over-parameterized regression problem: the network has enough degrees of freedom to drive the single residual near zero.

## API

`POST /api/neural/backpropagation`

The request may omit fields to use the assignment defaults:

```json
{
  "x": 1,
  "target": 2,
  "w": 0.5,
  "b": 0,
  "v": 1,
  "c": 0,
  "learning_rate": 0.1,
  "additional_training_steps": 100
}
```

The response contains forward-pass intermediates, initial loss, gradients, updated parameters/loss, finite-difference rows, all training history, final parameters/loss, and execution duration. Non-finite values, non-positive learning rates, and more than 100,000 training steps return HTTP 400.

## Verification

```text
cmake --build build --config Release
ctest --test-dir build --output-on-failure
cd frontend && npm run build
```

The browser verification runs the live endpoint from Neural Lab, renders the returned loss history, and exports the same backend-generated values to CSV. No chart data or numerical result is hardcoded in the frontend.
