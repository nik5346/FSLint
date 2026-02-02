# Contributing to FSLint

Thank you for your interest in contributing to FSLint! We welcome contributions from the community to help make this tool better for everyone.

## How to Contribute

### Reporting Bugs

- Search the existing issues to see if the bug has already been reported.
- If not, create a new issue with a clear description of the problem and steps to reproduce it.

### Suggesting Features

- Open a new issue to discuss your feature request.

### Pull Requests

1.  Fork the repository and create your branch from `main`.
2.  Ensure your code follows the project's style and quality standards.
3.  Include tests for any new functionality.
4.  Update documentation if necessary.
5.  Submit your pull request for review.

## Code Style

- We use `clang-format` to maintain a consistent code style. Please run `make clang-format` before submitting your PR.
- We use `clang-tidy` for static analysis. Ensure your changes do not introduce new warnings.
- Follow modern C++ practices (C++23).

## Development Environment

FSLint is a cross-platform project that supports Linux, Windows, and macOS.

### Building on macOS

You can build FSLint on macOS using CMake and Clang. All dependencies are handled automatically by CMake:

```bash
mkdir build
cd build
cmake ..
make
```

## Testing

Please run any existing tests and add new ones for your changes. We strive for high test coverage for the core validation logic.

## License

By contributing to FSLint, you agree that your contributions will be licensed under the project's MIT License.
