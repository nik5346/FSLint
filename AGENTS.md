# Jules' Skill for C++ and Web Development

This document defines the coding conventions, skills, and references for Jules when working on the FSLint project. These instructions take precedence over general defaults.

## C++ Coding Conventions

- **Standard:** Use C++20 (`std::c++20`).
- **Formatting:** Always run `clang-format` before submitting any pull request.
- **Linting:** Always run `clang-tidy` before submitting any pull request. Manually address any remaining warnings. If `clang-tidy` crashes due to environment-specific header incompatibilities (common with C++20), perform a thorough manual review against the rules below.
- **Naming:**
  - Classes/Structs: `PascalCase`
  - Functions/Methods: `camelCase`
  - Variables/Members: `snake_case` (private members should NOT have a trailing underscore unless necessary for disambiguation).
  - Constants/Macros: `SCREAMING_SNAKE_CASE`
- **Modern C++ Best Practices:**
  - Prefer `std::optional` or `std::variant` for error handling and results.
  - Use `[[nodiscard]]` for functions where the return value should not be ignored.
  - Prefer `auto` for complex types or when the type is obvious.
  - Use `std::format` (C++20) for string formatting.
  - Prefer `std::filesystem` for all file and directory operations.
  - Use `const` and `constexpr` wherever possible.
  - Avoid raw pointers; use `std::unique_ptr` or `std::shared_ptr`.

## Web Development (TypeScript/React) Conventions

- **Framework:** React 19+ with Vite.
- **Language:** TypeScript 6+. Use strict type checking.
- **Formatting:** Always run `npm run format` (to fix) or `npm run format:check` (to verify) in the `web/` directory.
- **Linting:** Always run `npm run lint` in the `web/` directory. No warnings allowed (`--max-warnings 0`).
- **Type Checking:** Always run `npm run check-types` in the `web/` directory.
- **Documentation (JSDoc):**
  - Every component, hook, interface, and function must have a JSDoc comment.
  - Include `@description`, `@param` (with description), and `@returns` (with description).
  - Types should be omitted from JSDoc as TypeScript handles them.
  - `@returns` is optional for Components and Hooks.
- **Styling:** Use CSS Modules (*.module.css). Avoid global CSS.
- **Component Patterns:**
  - Use functional components and hooks.
  - Destructure props in the function signature.
  - Ensure accessibility (`jsx-a11y` rules).

## FMI & SSP Specifications

Refer to these specifications when implementing or checking FMI/SSP logic:

- **FMI 1.0.1 (Model Exchange):** [FMI 1.0.1 ME Specification](https://fmi-standard.org/assets/releases/FMI_for_ModelExchange_v1.0.1.pdf)
- **FMI 1.0.1 (Co-Simulation):** [FMI 1.0.1 CS Specification](https://fmi-standard.org/assets/releases/FMI_for_CoSimulation_v1.0.1.pdf)
- **FMI 2.0.5:** [FMI 2.0.5 Specification](https://github.com/modelica/fmi-standard/releases/download/v2.0.5/FMI-Specification-2.0.5.pdf)
- **FMI 3.0.2:** [FMI 3.0.2 Specification](https://fmi-standard.org/docs/3.0.2/)
- **SSP 1.0.1:** [SSP 1.0.1 Specification](https://ssp-standard.org/publications/SSP101/SystemStructureAndParameterization101.pdf)
- **SSP 2.0:** [SSP 2.0 Specification](https://ssp-standard.org/docs/2.0/)

## Jules' Skills & Mandatory Steps

Before completing any task, Jules **must**:

1. **Format Code:**
   - C++: `cmake --build build --target clang-format`
   - Web: `npm run format` in `web/`
2. **Lint & Check:**
   - C++: `cmake --build build --target clang-tidy` (using `clang-tidy-18` if `19` is unavailable) and `cmake --build build --target doxygen-check`
   - Web: `npm run lint`, `npm run check-types`, and `npm run format:check` in `web/`
3. **Verify Build & Tests:**
   - C++: `ctest --output-on-failure -C Release` in `build/`
   - Web: `npm run build` in `web/`
4. **Reflect:** Check if any new files or logic adhere to the architectural patterns of the project (e.g., using `CheckerFactory`, following the `SchemaCheckerBase` pattern).
