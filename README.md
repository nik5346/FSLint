<div align="center">
  <img width="400" src="./images/banner.svg" alt="FSLint">
</div>

FSLint is a comprehensive linting and validation tool for Functional Mock-up Units (FMUs) and System Structure and Parameterization (SSP) files. It ensures that your models comply with the [FMI](https://fmi-standard.org/) (2.0, 3.0) and [SSP](https://ssp-standard.org/) (1.0, 2.0) standards, checking for both structural correctness and semantic validity.

## Features

- **FMI Validation**: Supports FMI 2.0.x and 3.0.x standards.
  - XML Schema validation for `modelDescription.xml`.
  - Semantic checks for variables, units, and types.
  - Validation of ModelStructure and variable dependencies.
  - Terminals and Icons validation (FMI 3.0 and backport to 2.0.4+).
  - Build Description validation (`sources/buildDescription.xml`).
  - Binary integrity and export checks (ELF, PE, Mach-O).
  - Distribution requirements (licenses, documentation, platform-specific binaries).
  - SVG fallback and graphical representation rules.
- **SSP Validation**: Supports SSP 1.0 and 2.0 standards.
  - XML Schema validation for `SystemStructure.ssd`.
- **Recursive Validation**: Automatically discovers and validates nested FMUs and SSPs within the `resources/` directory.
- **Detailed Reporting**: Generates a clear "Certificate" of validation with passed tests, warnings, and detailed error messages including line numbers. Supports hierarchical reporting for nested models.
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

Run the `FSLint-cli` tool on an FMU file, an SSP file, or an extracted directory:

```bash
./FSLint-cli path/to/your_model.fmu
```

#### Certificate Management

FSLint can manage validation certificates embedded within the models:

- `-s, --save`: Validate and add a certificate to the FMU/SSP.
- `-u, --update`: Re-validate and update the certificate in the FMU/SSP.
- `-r, --remove`: Remove the certificate from the FMU/SSP.
- `-d, --display`: Display the certificate information from the FMU/SSP.

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

## Standard Compliance

FSLint aims for full compliance with the following standards:

- **FMI (Functional Mock-up Interface)**:
  - Version 2.0.x (2.0 through 2.0.5).
  - Version 3.0.x (3.0 and 3.0.1).
- **SSP (System Structure and Parameterization)**:
  - Version 1.0.
  - Version 2.0.

## Project Structure

- `cli/`: Command-line interface implementation.
- `lib/`: Core logic for model extraction and validation.
  - `checker/`: Individual validation engines for FMI and SSP.
- `standard/`: XSD schema files for FMI and SSP standards.
- `scripts/`: Utility scripts for development and CI (e.g., encoding checks).
- `images/`: Documentation assets and banners.
- `tests/`: Comprehensive test suite and test data.

## Contributing

Contributions are welcome! See the [CONTRIBUTING](CONTRIBUTING) file for details.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
