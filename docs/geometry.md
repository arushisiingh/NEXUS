# Geometry Lab

Geometry Lab is the first geometry/result-inspection module in NEXUS. It keeps file parsing and numerical data ownership in C++17, then sends structured JSON to the React viewer.

## ASCII STL parser

`core/geometry/geometry_parser.cpp` accepts ASCII STL files with `facet normal`, `outer loop`, exactly three `vertex` records, `endloop`, and `endfacet`. It rejects empty input, binary/unsupported content, incomplete facets, non-finite coordinates, and malformed records. The parser returns triangles, normals, and an axis-aligned bounding box in the file's source units; NEXUS labels the displayed dimensions as metres because the sample contract uses metres.

## Temperature CSV parser

The supported header is exactly:

```text
node_id,x_m,y_m,z_m,temperature_C
```

Each row contains a unique integer node ID, finite coordinates, and a finite Celsius temperature. The C++ parser computes node count, temperature range, average, and a spatial bounding box.

## API contract

Both upload endpoints accept UTF-8 JSON text, not multipart form data:

```json
{"filename":"sample_cube.stl","content":"solid ..."}
```

Endpoints:

- `POST /api/geometry/stl/parse` returns triangle count, bounds, dimensions, processing duration, and triangle vertices/normals.
- `POST /api/geometry/csv/parse` returns node count, temperature statistics, bounds, processing duration, and nodes.

The request body is limited to 2 MiB. Invalid input returns HTTP 400 with `error: invalid_file`; oversized requests return HTTP 413. The frontend centralizes these calls in `frontend/src/api/client.ts`, enforces a request timeout, and reports connection, HTTP, and malformed-response failures.

## Viewer and provenance

The frontend uses Three.js with orbit controls. It displays the actual parsed triangle mesh as solid or wireframe and maps actual temperature nodes to a blue-to-red point-cloud scale. The sample files are synthetic fixtures in `data/geometry/` and are duplicated under `frontend/public/samples/` only so the browser can exercise the same upload API without relying on a native file picker.

Current limitations are intentional: binary STL is not accepted, CSV quoting/multiline fields are not implemented, and there is no mesh-to-temperature interpolation or registration because the API currently receives independent geometry and result files.

## Verification

```text
cmake --build build --config Release
ctest --test-dir build --output-on-failure
curl -X POST http://127.0.0.1:8081/api/geometry/stl/parse ...
curl -X POST http://127.0.0.1:8081/api/geometry/csv/parse ...
cd frontend && npm run build
```

The browser verification loads both checked-in sample files through the live Vite frontend, sends them through the C++ API proxy, switches to Temperature Cloud, and selects a node. This caught the practical integration requirement that the development server must proxy `/api` to port 8081; a production build alone cannot prove that path.
