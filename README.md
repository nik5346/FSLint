# FSLint

FSLint is a comprehensive linting and validation tool for Functional Mock-up Units (FMUs) and System Structure and Parameterization (SSP) files. It ensures that your models comply with the FMI (2.0, 3.0) and SSP (1.0, 2.0) standards, checking for both structural correctness and semantic validity.

## Features

- **FMI Validation**: Supports FMI 2.0 and 3.0 standards.
  - XML Schema validation.
  - Semantic checks for variables, units, and types.
  - Validation of ModelStructure and variable dependencies.
  - Array and clock validation for FMI 3.0.
- **SSP Validation**: Supports SSP 1.0 and 2.0 standards.
  - XML Schema validation for SystemStructure.ssd.
- **Detailed Reporting**: Generates a clear "Certificate" of validation with passed tests, warnings, and detailed error messages including line numbers.
- **Cross-Platform**: Built using CMake, supporting Linux, Windows, and macOS.

## Getting Started

### Prerequisites

- C++23 compatible compiler (e.g., GCC 13+, Clang 16+, MSVC 2022+)
- CMake 3.15 or higher

### Building from Source

```bash
mkdir build
cd build
cmake ..
make -j
```

### Usage

Run the `FSLint-cli` tool on an FMU file or an extracted directory:

```bash
./FSLint-cli path/to/your_model.fmu
```

Example output:
```text
╔════════════════════════════════════════════════════════════╗
║ MODEL VALIDATION REPORT                                    ║
╚════════════════════════════════════════════════════════════╝
Tool:       FSLint 0.0.1
Timestamp:  2026-02-01 17:22:28 UTC
Model Path: /path/to/your_model/model.fmu
SHA256:     0ad0a8b1ac49c7808aad524b171c1534c3ace783cdc1f2681dd13b0b54b8e889

┌────────────────────────────────────────┐
│ ARCHIVE VALIDATION                     │
└────────────────────────────────────────┘

  [✓ PASS] File Extension Check
  [✓ PASS] Disk Spanning Check
  [✓ PASS] Compression Method Check

...

╔════════════════════════════════════════════════════════════╗
║ ✓ MODEL VALIDATION PASSED                                  ║
╚════════════════════════════════════════════════════════════╝
```

## Project Structure

- `cli/`: Command-line interface implementation.
- `lib/`: Core logic for model extraction and validation.
  - `checker/`: Individual validation engines for FMI and SSP.
- `standard/`: XSD schema files for FMI and SSP standards.

## Contributing

Contributions are welcome! See the [CONTRIBUTING](CONTRIBUTING) file for details.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
