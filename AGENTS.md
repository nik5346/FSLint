# Jules Agent Skill

This file documents the coding conventions and requirements for this repository. All changes made by Jules must adhere to these guidelines.

## C++ Development

- **Standard**: C++20 (using `CMAKE_CXX_STANDARD 20`)
- **Compilers**: Clang 19.x is the primary compiler used for development and CI.
- **Library**: Uses `libstdc++` (default GNU library) for stability with standard headers.
- **Style**:
    - Naming: `PascalCase` for classes and structs, `camelCase` for functions and methods, `snake_case` for private members (e.g., `_myMember`).
    - Formatting: Always apply `clang-format` before submitting a PR.
    - Safety: Use `std::optional`, `std::variant`, and avoid raw pointers where possible.
- **Linting**: Ensure all `clang-tidy` checks pass. Fixes should be applied automatically using `clang-tidy --fix`.

## Web & TypeScript Development

- **Framework**: React 19+
- **Styling**: Use CSS Modules (*.module.css). Avoid global CSS and Tailwind.
- **Type Safety**: No `any`. Use interfaces for component props.
- **Documentation**: Use JSDoc for all components and hooks.
- **Linting**: Run `npm run lint` in the `web/` directory.

## FMI & SSP Standards

Refer to the official specifications for validation logic:
- [FMI 1.0.1](https://fmi-standard.org/downloads/)
- [FMI 2.0.5](https://fmi-standard.org/downloads/)
- [FMI 3.0.2](https://fmi-standard.org/downloads/)
- [SSP 1.0.1 / 2.0](https://ssp-standard.org/downloads/)
