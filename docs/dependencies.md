# NEXUS dependency record

| Area | Dependency | Version / policy | Purpose |
|---|---|---|---|
| Frontend | React | 18.3.1 | UI composition |
| Frontend | TypeScript | 5.6.3 | Static typing |
| Frontend | Vite | 5.4.10 | Development and production bundling |
| Frontend | Tailwind CSS | 3.4.17 | Utility styling foundation |
| Frontend | Lucide React | 0.468.0 | Technical iconography |
| Frontend | Three.js | 0.169.0 | WebGL geometry and temperature-field viewer |
| Frontend | @types/three | 0.169.0 | TypeScript declarations for Three.js |
| C++ | CMake | 3.20+ | Build generation |
| C++ | C++ standard library | C++17 | Core runtime, threading and data structures |
| C++ | `std::thread` / `std::atomic` | C++17 standard library | Bounded Performance Lab worker pool and deterministic result collection |
| C++ | `<cmath>` / C++17 standard library | C++17 standard library | Manual `tanh`, loss, chain-rule gradients, and central finite differences |
| C++ | C++17 polymorphism / mutex | C++17 standard library | Simulated hardware adapter interface, state machine, and event history |
| Tests | GoogleTest | 1.14.0 (optional FetchContent) | Unit-test framework for expanded modules |

The API currently uses a small platform socket adapter and deliberately keeps upload payloads as JSON text, so geometry parsing remains dependency-free on the C++ side. Binary STL, quoted/multiline CSV, and persistent mesh storage are outside the current Geometry Lab scope.

No secrets, machine-specific paths, or generated dependency directories belong in Git.
