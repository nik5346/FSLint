# FSLint - Checked Rules

This file lists all rules currently checked by FSLint, categorized by standard and version.

### Rule Statuses

Each rule check in FSLint results in one of the following statuses:

- **PASS**: The rule is fully satisfied.
- **FAIL**: The rule **must** be satisfied. Violation indicates non-compliance with the FMI or SSP specification that would prevent the model from working correctly in most tools.
- **WARN**: The rule **should** be satisfied. Violation indicates deviation from recommended best practices, use of deprecated features, or unusual values that might indicate an error.

## Model Detection (FMU and SSP)

- **Model Identification**: The tool **must** be able to identify the model type.
  - An **archive file** (regular file) **must** have the `.fmu` or `.ssp` extension for identification.
  - A **directory** is identified by the presence of `modelDescription.xml` (for FMU) or `SystemStructure.ssd` (for SSP) in its root.
  - If the model type cannot be identified or the mandatory identification files are missing, validation **must** fail.

## Archive Validation (FMU and SSP)

These rules apply to the ZIP archive itself for both FMU and SSP files.

- **File Extension**: Files **must** have the `.fmu` or `.ssp` extension respectively.
- **Disk Spanning**: Split or spanned ZIP archives **must not** be used.
- **Compression Methods**: Only `store` (0) and `deflate` (8) compression methods **must** be used.
- **Version Needed**: The maximum version needed to extract **must** be `2.0` (for compatibility).
- **Encryption**: Encrypted files **must not** be used.
- **Path Format and Security**:
  - Only forward slashes `/` **must** be used (no backslashes `\`).
  - Absolute paths **must not** be used (must not start with `/`).
  - Drive letters or device paths (e.g., `C:`) **must not** be used.
  - **[SECURITY] Path Traversal & Zip Slip**:
    - Parent directory traversal (`..`) **must not** be used.
    - Normalized paths **must not** escape the archive root (e.g., `foo/../../etc/passwd`).
  - **[SECURITY] Control Characters**: Paths **must not** contain control characters (U+0000–U+001F) or null bytes (`\0`).
  - **Illegal Characters**: Paths **must not** contain illegal characters for maximum cross-platform compatibility (`<`, `>`, `"`, `|`, `?`, `*`).
  - Non-ASCII characters in paths **should** be avoided.
  - Leading `./` or multiple consecutive slashes `//` **should** be avoided.
- **Symbolic Links**: Symbolic links **must not** be used within the archive.
- **Language Encoding Flag**:
  - Bit 11 **must** be set for every file whose filename in the archive contains non-ASCII characters (to indicate UTF-8 encoding).
  - Bit 11 **should** be 0 for files whose filenames only contain ASCII characters (for maximum portability with old tools).
- **Data Descriptor Consistency**: General purpose bit 3 (data descriptor) is only allowed with `deflate` compression.
- **[SECURITY] Zip Bomb (Decompression Bomb)**:
  - The compression ratio of each entry **must** be below 100:1 (uncompressed vs. compressed size).
  - The uncompressed size of any single entry **must** be less than 1 GB.
  - The total uncompressed size of all entries **must** be less than 10 GB.
- **[SECURITY] Central Directory Integrity**:
  - Metadata (filenames, sizes, offsets) **must** be consistent between the central directory and local file headers.
  - The total number of entries reported in the central directory **must** match the actual count of local file records found.
  - The local file header signature (`PK\x03\x04`) **must** be present at the reported offset for every central directory entry.
- **[SECURITY] Overlapping File Entries**: No two local file data regions (headers + compressed data) **must** overlap in the archive.
- **[SECURITY] Duplicate Entry Names**:
  - Multiple entries with the same name **must not** be used.
  - Entry names that only differ in case (e.g., `Model.xml` and `model.xml`) **must not** be used (to prevent collisions on case-insensitive filesystems).
- **[SECURITY] Extra Field and Comment Integrity**:
  - The archive comment length **must** match the declared length.
  - Extra field lengths **must not** overflow the header or entry bounds.

## Common FMI Rules (All Versions)

These rules are applied to the `modelDescription.xml` file regardless of the FMI version.

### Metadata and Format

- **Schema Validation**: `modelDescription.xml` is validated against the official FMI XSD schemas. Optional files like `buildDescription.xml` and `terminalsAndIcons.xml` are also schema-validated if present.
- **[SECURITY] XXE (XML External Entity)**: XML files **must not** contain `<!DOCTYPE>` declarations or external entity references.
- **XML Declaration**: Must be XML version 1.0.
- **UTF-8 Encoding**:
  - FMI 2.0 and 3.0: XML files **must** use UTF-8 encoding.
  - For FMI 1.0, UTF-8 encoding is highly **recommended**.
- **Model Name Format**: The `modelName` attribute **must** be present and non-empty.
- **FMI Version Format**: The `fmiVersion` attribute **must** match the standard version string. For FMI 1.0, 2.0, and 3.0, only "1.0", "2.0", and "3.0" are allowed respectively.
- **Generation Date and Time**:
  - **Must** be a valid ISO 8601 date-time string.
  - **Must** be a valid date in the past (not after the current system time).
- **Model Version Format**: The `version` attribute of the model. Semantic versioning (`MAJOR.MINOR.PATCH`) is **recommended**.
- **Copyright Notice**:
  - **Should** be provided if the `author` attribute is missing or empty.
  - **Should** begin with ©, "Copyright", or "Copr.".
  - **Should** include the year of publication and the copyright holder name.
- **Traceability Attributes**:
  - The `author` attribute **should** be present and non-empty if the `copyright` attribute is missing or empty.
  - The `license` and `generationTool` attributes **should** be present and non-empty. For manually created FMUs, it is recommended to set `generationTool` to "Handmade".
- **Model Identifier Format**:
  - **Must** be a valid C identifier (starts with letter/underscore, followed by alphanumerics/underscores).
  - **Should** be under 64 characters; the absolute maximum is 200 characters (**must** not exceed).
- **Log Categories**: Category names **must** be unique within the `LogCategories` element.

### Variable and Type Consistency

- **Unique Variable Names**: All variables **must** have unique names.
- **Unique Type Names**: All type definitions **must** have unique names.
- **Unique Unit Names**: All unit definitions **must** have unique names.
- **Variable Naming Convention**:
  - `flat`:
    ```bnf
    name         = Unicode-char { Unicode-char }
    Unicode-char = any Unicode character without carriage return (#xD), line feed (#xA) nor tab (#x9)
    ```
  - `structured`: **Must** follow the structured name syntax.
    ```bnf
    name            = identifier | "der(" identifier ["," unsignedInteger ] ")"
    identifier      = B-name [ arrayIndices ] { "." B-name [ arrayIndices ] }
    B-name          = nondigit { digit | nondigit } | Q-name
    Q-name          = "'" ( Q-char | escape ) { Q-char | escape } "'"
    Q-char          = nondigit | digit | "!" | "#" | "$" | "%" | "&" | "(" | ")" | "*" | "+" | "," | "-" | "." | "/" | ":" | ";" | "<" | ">" | "=" | "?" | "@" | "[" | "]" | "^" | "{" | "}" | "|" | "~" | " "
    escape          = "\'" | "\"" | "\?" | "\\" | "\a" | "\b" | "\f" | "\n" | "\r" | "\t" | "\v"
    arrayIndices    = "[" unsignedInteger { "," unsignedInteger } "]"
    unsignedInteger = digit { digit }
    nondigit        = "_" | "a" | ... | "z" | "A" | ... | "Z"
    digit           = "0" | "1" | ... | "9"
    ```
- **Type and Unit References**: All references to types (`declaredType`) and units must exist in `TypeDefinitions` or `UnitDefinitions`.
- **Unused Definitions**: All type and unit definitions **should** be referenced by at least one variable.
- **Min/Max/Start Constraints**:
  - The `start` value **must** be within the `[min, max]` range.
  - The `max` attribute **must** be `>= min`.

### Model Structure and Interfaces

- **Implemented Interfaces**: At least one interface (`CoSimulation`, `ModelExchange`, or `ScheduledExecution`) **must** be implemented.
- **Default Experiment**: The `startTime` and `stopTime` attributes **must** be non-negative. The `tolerance` and `stepSize` attributes **must** be greater than zero. If both `startTime` and a finite `stopTime` are provided, `stopTime` **must** be greater than `startTime`.

### Directory Structure

- **Mandatory Files**: `modelDescription.xml` **must** be present in the FMU root.
- **Documentation**:
  - Providing documentation is **recommended**; a warning is issued if the `documentation/` directory is missing.
  - If the `documentation/` directory exists, the standard entry point **must** be present (`_main.html` for FMI 1.0, `index.html` for FMI 2.0 and later).
- **Licenses**:
  - Providing license information is **recommended**; a warning is issued if the `documentation/licenses/` directory is missing.
  - If the `documentation/licenses/` directory exists, the standard entry point **must** be present (`license.txt` or `license.html` for FMI 1.0 and 2.0; `license.spdx`, `license.txt`, or `license.html` for FMI 3.0).
- **Distribution**:
  - The FMU **must** contain at least one implementation (a binary for at least one platform or source code).
  - Every platform directory in `binaries/` **must** contain a binary for each `modelIdentifier` defined in `modelDescription.xml`.
- **Effectively Empty Directories**: Standard directories like `documentation/` or `resources/` **should not** be effectively empty.
- **Root Entries**: Unknown files or directories in the FMU root **should** be avoided.

### Recursive Validation

- **Nested Models**: FMUs and SSPs located in the `resources/` directory are automatically discovered and validated recursively.

### Binary Exports

The binary file **must** be a shared library (dynamic library). Its format, extension, and architecture **must** match the platform identifier (the directory name under `binaries/`):

- **Windows**:
  - Format: **PE** (Portable Executable).
  - Extension: `.dll`.
  - Matching: Must match the bitness/architecture of the identifier (e.g., `win64` or `x86_64-windows` requires a 64-bit x86 binary).
- **Linux**:
  - Format: **ELF** (Executable and Linkable Format).
  - Extension: `.so`.
  - Matching: Must match the bitness/architecture of the identifier (e.g., `linux64` or `x86_64-linux` requires a 64-bit x86 binary).
- **macOS**:
  - Format: **Mach-O**.
  - Extension: `.dylib`.
  - Matching: Must match the bitness/architecture of the identifier (e.g., `darwin64` or `x86_64-darwin` requires a 64-bit x86 binary).
  - **Multi-architecture (Fat) Binaries**: For Fat binaries, at least one contained architecture **must** match the platform identifier.

---

## FMI 1.0 Rules

### Model Description

- **Generation Date and Time**: **Should** not be unreasonably old (before 2010).
- **GUID Presence**: The `guid` attribute **must** be present and non-empty.
- **Model Identifier Matching**: The `modelIdentifier` **must** match the FMU filename stem (ZIP name).
- **URI-based File References**: In CS `CoSimulation_Tool`, `entryPoint` and `file` attributes **should** use the `fmu://` URI scheme.
  - URIs using `fmu://` **must** point to existing files within the archive.
- **Vendor Annotations**: Tool names within `VendorAnnotations` **must** be unique.

### Variable Consistency

- **Legal Variability**: Only variables of type `Real` **can** have `variability="continuous"`.
- **Required Start Values**: Variables **must** have a `start` attribute if `causality="input"` or `variability="constant"`.
- **Illegal Start Values**:
  - The `fixed` attribute **must** only be present if a `start` attribute is also provided.
  - The `fixed` attribute **must not** be used for variables with `causality="input"` or `causality="none"` (this attribute is only defined for other causalities).
  - For variables with `variability="constant"`, `fixed="false"` (guess value) is **not allowed**.
- **Causality/Variability Combinations**:
  - Variables with `variability="constant"` **must not** have `causality="input"` (logical contradiction: constants cannot be changed from the outside).
- **Alias Variables (same VR)**:
  - Exactly one variable in an alias set **must** be the base variable (marked `noAlias` or having no `alias` attribute).
  - `negatedAlias` **can** only be used for `Real` and `Integer` variables.
  - All variables in an alias set **must** have equivalent `start` values (taking negation into account).
  - If any variable in an alias set has `variability="constant"`, all variables in that set **must** be `constant`.
  - All variables in an alias set **should** have the same `variability`.
- **Direct Dependency References**:
  - `DirectDependency` **must** only be present on variables with `causality="output"`.
  - Each `Name` listed inside `DirectDependency` **must** reference an existing variable with `causality="input"`.

### Binary Exports
- **Function Prefixing**: All exported functions must be prefixed with `<modelIdentifier>_`.
- **Mandatory Functions (CS)**: All functions specified for Co-Simulation **must** be present.
  `fmiGetTypesPlatform`, `fmiGetVersion`, `fmiInstantiateSlave`, `fmiInitializeSlave`, `fmiTerminateSlave`, `fmiResetSlave`, `fmiFreeSlaveInstance`, `fmiSetDebugLogging`, `fmiSetReal`, `fmiSetInteger`, `fmiSetBoolean`, `fmiSetString`, `fmiSetRealInputDerivatives`, `fmiGetReal`, `fmiGetInteger`, `fmiGetBoolean`, `fmiGetString`, `fmiGetRealOutputDerivatives`, `fmiDoStep`, `fmiCancelStep`, `fmiGetStatus`, `fmiGetRealStatus`, `fmiGetIntegerStatus`, `fmiGetBooleanStatus`, `fmiGetStringStatus`.
- **Mandatory Functions (ME)**: All functions specified for Model Exchange **must** be present.
  `fmiGetModelTypesPlatform`, `fmiGetVersion`, `fmiInstantiateModel`, `fmiFreeModelInstance`, `fmiSetDebugLogging`, `fmiSetTime`, `fmiSetContinuousStates`, `fmiCompletedIntegratorStep`, `fmiSetReal`, `fmiSetInteger`, `fmiSetBoolean`, `fmiSetString`, `fmiInitialize`, `fmiGetDerivatives`, `fmiGetEventIndicators`, `fmiGetReal`, `fmiGetInteger`, `fmiGetBoolean`, `fmiGetString`, `fmiEventUpdate`, `fmiGetContinuousStates`, `fmiGetNominalContinuousStates`, `fmiGetStateValueReferences`, `fmiTerminate`.

### Directory Structure

- **model.png Existence**: It is **recommended** to provide an icon `model.png` in the FMU root. PNG icons **should** have at least a size of 100x100 pixels.
- **Documentation**:
  - Providing documentation is **recommended**; a warning is issued if the `documentation/` directory is missing.
  - If the `documentation/` directory exists, the standard entry point `_main.html` **must** be present.
- **Standard Headers**: The `sources/` directory **should not** include standard FMI 1.0 headers: `fmiFunctions.h`, `fmiModelFunctions.h`, `fmiModelTypes.h`, `fmiPlatformTypes.h`.

---

## FMI 2.0 Rules

### Schema Validation

- **modelDescription.xml**: **Must** be valid against `fmi2ModelDescription.xsd`.
- **buildDescription.xml**: **Must** be valid against `fmi3BuildDescription.xsd` (if present in `sources/`).
- **terminalsAndIcons.xml**: **Must** be valid against `fmi3TerminalsAndIcons.xsd` (if present in `terminalsAndIcons/`).

### Model Description

- **Generation Date and Time**: **Should** not be unreasonably old (before 2014).
- **Enumeration Variables**: **Must** have a `declaredType` attribute.
- **Alias Variables (same VR)**:
  - At most one variable in an alias set **can** be settable with `fmi2SetXXX`.
  - At most one variable in an alias set (where at least one is not constant) **can** have a `start` attribute.
  - Constants **can** only be aliased to other constants and **must** have identical `start` values.
  - All variables in an alias set **must** have the same unit.
- **Independent Variable**: At most one **is** allowed; it **must** be of type `Real`, have no `start`/`initial` attribute, and have `variability="continuous"`.
- **Reinit Attribute**: Allowed **only** for continuous-time states; **not allowed** in Co-Simulation only FMUs.
- **Multiple Set Attribute**: `canHandleMultipleSetPerTimeInstant` **only** allowed for inputs; **not allowed** in Co-Simulation only FMUs.
- **Continuous-time States and Derivatives**:
  - All variables with a `derivative` attribute **must** have `variability="continuous"` and be of type `Real`.
  - Variables listed in `ModelStructure/Derivatives` **must** point to a state variable (via the `derivative` attribute) that has `causality="local"` or `"output"`, `variability="continuous"`, and is of type `Real`.
  - Having a `derivative` attribute alone does not make a variable (or its target) a continuous-time state unless it is listed in `ModelStructure/Derivatives`.
- **Model Structure**:
  - `Outputs` and `InitialUnknowns` **must** be complete (containing exactly one representative from each respective alias set) and correctly ordered.
  - Mandatory `InitialUnknowns` **must** be provided for:
    - Outputs with `initial="calculated"` or `"approx"`.
    - Calculated parameters.
    - Continuous-time states and their derivatives (as defined by `ModelStructure/Derivatives`) with `initial="calculated"` or `"approx"`.
  - `Derivatives` defines the set of continuous-time states; all listed entries **must** be Real variables with a `derivative` attribute pointing to a continuous Real state.
  - Dependencies and `dependenciesKind` **must** be consistent in size.
- **Vendor Annotations**: Tool names within `VendorAnnotations` **must** be unique.
- **Prohibited Special Floats**: Attributes of type `Real` **must not** contain `NaN` or `INF`. This applies to:
  - `ScalarVariable`: `min`, `max`, `start`, `nominal`.
  - `SimpleType`: `min`, `max`, `nominal`.
  - `BaseUnit` / `DisplayUnit`: `factor`, `offset`.
  - `DefaultExperiment`: `startTime`, `stopTime`, `tolerance`, `stepSize`.

### Variable Consistency

- **Legal Variability**: Variability **must** be compatible with the variable's type and causality. Only floating-point types (`Real`) **can** be `continuous`. Parameters (`causality="parameter"` or `"calculatedParameter"`) **must** be `"fixed"` or `"tunable"`.
- **Required Start Values**: Variables **must** have a `start` value if `causality` is `input` or `parameter`, `variability` is `constant`, or `initial` is `exact` or `approx`.
- **Illegal Start Values**: Variables with `initial="calculated"` or `causality="independent"` **must not** provide a `start` value.
- **Causality/Variability/Initial Combinations**: Combinations **must** follow the allowed set defined in the FMI 2.0 specification tables.

### Terminals and Icons

- **fmiVersion Check**: `terminalsAndIcons.xml` **must** have `fmiVersion="3.0"`. This attribute is fixed to "3.0" for FMI 2.0 FMUs because the feature was backported from FMI 3.0.
- **Unique Names**: Terminal and member names **must** be unique within their respective levels.
- **VariableReferences**:
  - All `variableName` references **must** exist in `modelDescription.xml`.
  - `variableKind` **must** be one of: `signal`, `inflow`, `outflow`.
- **Matching Rules**: `matchingRule` on each terminal **must** be one of: `plug`, `bus`, `signal`.
- **Causality Constraints**: Variables used as stream members or flows **must** have compatible causalities (input, output, parameter, etc.).
- **Graphical Representation**:
  - Icons **must** exist (`iconBaseName` must point to a PNG file). PNG icons **should** have at least a size of 100x100 pixels.
  - Colors **must** be valid RGB: `defaultConnectionColor` **must** have exactly 3 space-separated values, and each **must** be an integer in range 0–255.
  - Extent Validity: `TerminalGraphicalRepresentation` coordinates **must** satisfy `x1 < x2` and `y1 < y2`.
  - Coordinate System: If a top-level `CoordinateSystem` is present, the terminal's extent **must** lie within those bounds.
  - Orphan Files: PNG files in `terminalsAndIcons/` that are not referenced by any terminal's `iconBaseName` **should** be avoided (**WARN**).
- **Stream/Flow Constraints**: Only one inflow/outflow **is** allowed when a stream variable is present in a terminal.

### Binary Exports

- **Mandatory Functions**: The implementation **must** implement all functions for the supported interface types (Model Exchange and/or Co-Simulation). All functions specified for a supported interface type **must** be present, even if they are only needed for optional capabilities.
  - **Common functions**: `fmi2GetTypesPlatform`, `fmi2GetVersion`, `fmi2SetDebugLogging`, `fmi2Instantiate`, `fmi2FreeInstance`, `fmi2SetupExperiment`, `fmi2EnterInitializationMode`, `fmi2ExitInitializationMode`, `fmi2Terminate`, `fmi2Reset`, `fmi2GetReal`, `fmi2GetInteger`, `fmi2GetBoolean`, `fmi2GetString`, `fmi2SetReal`, `fmi2SetInteger`, `fmi2SetBoolean`, `fmi2SetString`, `fmi2GetFMUstate`, `fmi2SetFMUstate`, `fmi2FreeFMUstate`, `fmi2SerializedFMUstateSize`, `fmi2SerializeFMUstate`, `fmi2DeSerializeFMUstate`, `fmi2GetDirectionalDerivative`.
  - **Model Exchange functions**: `fmi2EnterEventMode`, `fmi2NewDiscreteStates`, `fmi2EnterContinuousTimeMode`, `fmi2CompletedIntegratorStep`, `fmi2SetTime`, `fmi2SetContinuousStates`, `fmi2GetDerivatives`, `fmi2GetEventIndicators`, `fmi2GetContinuousStates`, `fmi2GetNominalsOfContinuousStates`.
  - **Co-Simulation functions**: `fmi2SetRealInputDerivatives`, `fmi2GetRealOutputDerivatives`, `fmi2DoStep`, `fmi2CancelStep`, `fmi2GetStatus`, `fmi2GetRealStatus`, `fmi2GetIntegerStatus`, `fmi2GetBooleanStatus`, `fmi2GetStringStatus`.

### Directory Structure

- **model.png Existence**: It is **recommended** to provide an icon `model.png` in the FMU root. PNG icons **should** have at least a size of 100x100 pixels.
- **Documentation**:
  - Providing documentation is **recommended**; a warning is issued if the `documentation/` directory is missing.
  - If the `documentation/` directory exists, the standard entry point `index.html` **must** be present.
- **External Dependencies**: Since `needsExecutionTool='true'`, 'documentation/externalDependencies.{txt|html}' **should** be present to document the external resources the FMU depends on.
- **Source Files Consistency**:
  - Typical source files (extensions: `.c`, `.cc`, `.cpp`, `.cxx`, `.C`, `.c++`) in `sources/` **should** be listed in `modelDescription.xml` `<SourceFiles>`.
  - `SourceFiles/File` entries **must** point to existing files in `sources/`.
- **Platform Names**: Platform directories in `binaries/` **should** use standardized names: `win32`, `win64`, `linux32`, `linux64`, `darwin32`, `darwin64`.
- **FMI 2.0.4 Compatibility**: Source-only FMUs **should** provide both `<SourceFiles>` in `modelDescription.xml` and a `buildDescription.xml` for maximum compatibility.
- **Standard Headers**: The `sources/` directory **should not** include standard FMI 2.0 headers: `fmi2Functions.h`, `fmi2FunctionTypes.h`, `fmi2TypesPlatform.h`.

### Build Description

- **fmiVersion Check**: `buildDescription.xml` **must** have `fmiVersion="3.0"`. This attribute is fixed to "3.0" for FMI 2.0 FMUs because the feature was backported from FMI 3.0.
- **Source/Include Validation**: All listed `SourceFile` and `IncludeDirectory` entries **must** exist in `sources/` and **must not** contain `..` traversal.
- **Attribute Validation**: The `language` and `compiler` attributes **should** be from the suggested sets:
  - `language`: `C89`, `C90`, `C99`, `C11`, `C17`, `C18`, `C23`, `C++98`, `C++03`, `C++11`, `C++14`, `C++17`, `C++20`, `C++23`, `C++26`.
  - `compiler`: `gcc`, `clang`, `msvc`.
- **Model Identifier Match**: `modelIdentifier` in `BuildConfiguration` must match an identifier from `modelDescription.xml`.
- **Absolute Paths**: `SourceFile` and `IncludeDirectory` `name` attributes **must not** be absolute paths and **must** resolve to an existing file or directory respectively.
- **PreprocessorDefinition Validation**: Each `<PreprocessorDefinition>` **must** have a `name` attribute that is a valid C preprocessor identifier (`[A-Za-z_][A-Za-z0-9_]*`). If `optional="true"`, at least one `<Option>` child **should** be present.
- **Library Validation**: Each `<Library>` **must** have a non-empty `name` attribute. Internal libraries (where `external` is absent or `"false"`) **should** have a matching file present under `binaries/`.
- **Empty SourceFileSet**: A `<SourceFileSet>` with no `<SourceFile>` children **should** be avoided.
- **compilerOptions without compiler**: Specifying `compilerOptions` without a `compiler` attribute **should** be avoided.

---

## FMI 3.0 Rules

### Model Description

- **Generation Date and Time**: **Should** not be unreasonably old (before 2022).
- **instantiationToken**: The `instantiationToken` **should** follow the GUID format.
- **Annotations**: Annotation types within `Annotations` **must** be unique within their container.
- **Independent Variable**: Exactly one **is** allowed; it **must** be `Float32` or `Float64`, and have no `initial` or `start` attribute.
- **Derivative Consistency**:
  - All variables with a `derivative` attribute **must** have `variability="continuous"` and be of type `Float32` or `Float64`.
  - If a variable is listed in `ContinuousStateDerivative`, its target state variable **must** have `variability="continuous"`.
  - Having a `derivative` attribute alone does not make a variable (or its target) a continuous-time state unless it is listed in `ContinuousStateDerivative`.
  - Dimensions of a derivative **must** match dimensions of the state.
- **Structural Parameters**: **Must** be of type `UInt64`; if used in `<Dimension>`, the `start` value **must** be `> 0`.
- **Dimensions**:
  - **Must** have either `start` or `valueReference`, but not both.
  - Fixed dimensions (`start`) **must** be `> 0`.
- **Array Start Values**: The number of values **must** match the total array size or be exactly 1 (for broadcast).
- **Clocks**:
  - Clock references **must** exist and point to variables of type `Clock`.
  - Clocks **must not** reference themselves.
  - Clocked variables **must** be discrete and have specific causality.
  - Clock types **must** have consistent `intervalVariability` and interval attributes.
- **Model Structure**:
  - `Output` and `InitialUnknown` **must** be complete (containing exactly one representative from each respective mandatory alias set) and unique.
  - `Output`: Must contain exactly one representative per alias set for every variable with `causality="output"`.
  - Mandatory `InitialUnknowns` **must** be provided for outputs, calculated parameters, and active states/derivatives (listed in `ContinuousStateDerivative`) with `initial="approx"` or `"calculated"`.
  - Optional clocked variables are allowed in `InitialUnknown`.
  - `ContinuousStateDerivative` defines the set of continuous-time states; all listed entries **must** correspond to variables with a `derivative` attribute.
  - `ClockedState`: Must list all variables that have a `previous` attribute.
  - `EventIndicator` elements **must** be complete and unique.
- **Variable Dependencies**: `dependenciesKind` **must** be restricted to allowed types and is **not allowed** for `InitialUnknown`.
- **Capability Flags**:
  - `canSerializeFMUState="true"` **must** have `canGetAndSetFMUState="true"`.
  - **Co-Simulation**:
    - `canReturnEarlyAfterIntermediateUpdate="true"` **must** have `providesIntermediateUpdate="true"`.
    - `mightReturnEarlyFromDoStep="true"` **should** have `providesIntermediateUpdate="true"` (**WARN**).

### Variable Consistency

- **Legal Variability**: Variability **must** be compatible with the variable's type and causality. Only floating-point types (`Float32`, `Float64`) **can** be `continuous`. Parameters (`causality="parameter"`, `"calculatedParameter"`, or `"structuralParameter"`) **must** be `"fixed"` or `"tunable"`.
- **Required Start Values**: Variables **must** have a `start` value if `causality` is `input`, `parameter`, or `structuralParameter`, `variability` is `constant`, or `initial` is `exact` or `approx`. Variables of type `Clock` are excluded from this requirement.
- **Illegal Start Values**: Variables with `initial="calculated"` or `causality="independent"` **must not** provide a `start` value.
- **Causality/Variability/Initial Combinations**: Combinations **must** follow the allowed set defined in the FMI 3.0 specification.

### Terminals and Icons

- **fmiVersion Match**: The `fmiVersion` attribute in `terminalsAndIcons.xml` **must** match the `fmiVersion` in `modelDescription.xml`.
- **Unique Names**: Terminal and member names **must** be unique within their respective levels.
- **VariableReferences**:
  - All `variableName` references **must** exist in `modelDescription.xml`.
  - `variableKind` **must** be one of: `signal`, `inflow`, `outflow`.
- **Matching Rules**: `matchingRule` on each terminal **must** be one of: `plug`, `bus`, `signal`.
- **Causality Constraints**: Variables used as stream members or flows **must** have compatible causalities (input, output, parameter, etc.).
- **Graphical Representation**:
  - Icons **must** exist (`iconBaseName` must point to a PNG file). PNG icons **should** have at least a size of 100x100 pixels.
  - PNG fallback required if SVG is provided in `terminalsAndIcons/`.
  - Colors **must** be valid RGB: `defaultConnectionColor` **must** have exactly 3 space-separated values, and each **must** be an integer in range 0–255.
  - Extent Validity: `TerminalGraphicalRepresentation` coordinates **must** satisfy `x1 < x2` and `y1 < y2`.
  - Coordinate System: If a top-level `CoordinateSystem` is present, the terminal's extent **must** lie within those bounds.
  - Orphan Files: PNG files in `terminalsAndIcons/` that are not referenced by any terminal's `iconBaseName` **should** be avoided (**WARN**).
- **Stream/Flow Constraints**: Only one inflow/outflow allowed when a stream variable is present in a terminal.

### Binary Exports
- **Mandatory Functions**: The implementation **must** implement all common API functions and all functions for at least one supported interface type (Model Exchange, Co-Simulation, or Scheduled Execution). All functions specified for a supported interface type **must** be present, even if they are only needed for optional capabilities.
  - **Common functions**: `fmi3GetVersion`, `fmi3SetDebugLogging`, `fmi3FreeInstance`, `fmi3EnterInitializationMode`, `fmi3ExitInitializationMode`, `fmi3EnterEventMode`, `fmi3Terminate`, `fmi3Reset`, `fmi3GetFloat32`, `fmi3GetFloat64`, `fmi3GetInt8`, `fmi3GetUInt8`, `fmi3GetInt16`, `fmi3GetUInt16`, `fmi3GetInt32`, `fmi3GetUInt32`, `fmi3GetInt64`, `fmi3GetUInt64`, `fmi3GetBoolean`, `fmi3GetString`, `fmi3GetBinary`, `fmi3GetClock`, `fmi3SetFloat32`, `fmi3SetFloat64`, `fmi3SetInt8`, `fmi3SetUInt8`, `fmi3SetInt16`, `fmi3SetUInt16`, `fmi3SetInt32`, `fmi3SetUInt32`, `fmi3SetInt64`, `fmi3SetUInt64`, `fmi3SetBoolean`, `fmi3SetString`, `fmi3SetBinary`, `fmi3SetClock`, `fmi3GetNumberOfVariableDependencies`, `fmi3GetVariableDependencies`, `fmi3GetFMUState`, `fmi3SetFMUState`, `fmi3FreeFMUState`, `fmi3SerializedFMUStateSize`, `fmi3SerializeFMUState`, `fmi3DeserializeFMUState`, `fmi3GetDirectionalDerivative`, `fmi3GetAdjointDerivative`, `fmi3EnterConfigurationMode`, `fmi3ExitConfigurationMode`, `fmi3GetIntervalDecimal`, `fmi3GetIntervalFraction`, `fmi3GetShiftDecimal`, `fmi3GetShiftFraction`, `fmi3SetIntervalDecimal`, `fmi3SetIntervalFraction`, `fmi3SetShiftDecimal`, `fmi3SetShiftFraction`, `fmi3EvaluateDiscreteStates`, `fmi3UpdateDiscreteStates`.
  - **Model Exchange functions**: `fmi3InstantiateModelExchange`, `fmi3EnterContinuousTimeMode`, `fmi3CompletedIntegratorStep`, `fmi3SetTime`, `fmi3SetContinuousStates`, `fmi3GetContinuousStateDerivatives`, `fmi3GetEventIndicators`, `fmi3GetContinuousStates`, `fmi3GetNominalsOfContinuousStates`, `fmi3GetNumberOfEventIndicators`, `fmi3GetNumberOfContinuousStates`.
  - **Co-Simulation functions**: `fmi3InstantiateCoSimulation`, `fmi3EnterStepMode`, `fmi3GetOutputDerivatives`, `fmi3DoStep`.
  - **Scheduled Execution functions**: `fmi3InstantiateScheduledExecution`, `fmi3ActivateModelPartition`.

### Directory Structure

- **External Dependencies**: Since `needsExecutionTool='true'`, 'documentation/externalDependencies.{txt|html}' **must** be present to document the external resources the FMU depends on.
- **Documentation**:
  - Providing documentation is **recommended**; a warning is issued if the `documentation/` directory is missing.
  - If the `documentation/` directory exists, the standard entry point `index.html` **must** be present.
- **Diagrams**: If `documentation/diagram.svg` is provided, a `documentation/diagram.png` fallback **must** also be present. PNG diagrams **should** have at least a size of 100x100 pixels.
- **icon.png Existence**:
  - It is **recommended** to provide an icon `terminalsAndIcons/icon.png` if the `terminalsAndIcons/` directory exists.
  - If `terminalsAndIcons/icon.svg` is provided, a `terminalsAndIcons/icon.png` fallback **must** also be present.
  - PNG icons **should** have at least a size of 100x100 pixels.
- **Build Description**: If the `sources/` directory exists, `sources/buildDescription.xml` **must** be present.
- **Platform Tuples**:
  - Platform directories **should** follow the `<arch>-<sys>[-<abi>]` format.
  - Architecture `<arch>` should be one of: `aarch32`, `aarch64`, `riscv32`, `riscv64`, `x86`, `x86_64`, `ppc32`, `ppc64`.
  - Operating system `<sys>` should be one of: `darwin`, `linux`, `windows`.
- **Static Linking**: If static libraries are present, `documentation/staticLinking.{txt|html}` **must** be present.
- **Extra Directory**: Subdirectories in `extra/` **should** use reverse domain name notation (e.g., `com.example`).
- **Standard Headers**: The `sources/` directory **should not** include standard FMI 3.0 headers: `fmi3Functions.h`, `fmi3FunctionTypes.h`, `fmi3PlatformTypes.h`.

### Build Description

- **fmiVersion Check**: `buildDescription.xml` **must** have an `fmiVersion` attribute that matches the `fmiVersion` attribute in `modelDescription.xml`.
- **Source/Include Validation**: All listed `SourceFile` and `IncludeDirectory` entries **must** exist in `sources/` and **must not** contain `..` traversal.
- **Attribute Validation**: The `language` and `compiler` attributes **should** be from the suggested sets:
  - `language`: `C89`, `C90`, `C99`, `C11`, `C17`, `C18`, `C23`, `C++98`, `C++03`, `C++11`, `C++14`, `C++17`, `C++20`, `C++23`, `C++26`.
  - `compiler`: `gcc`, `clang`, `msvc`.
- **Model Identifier Match**: `modelIdentifier` in `BuildConfiguration` must match an identifier from `modelDescription.xml`.
- **Absolute Paths**: `SourceFile` and `IncludeDirectory` `name` attributes **must not** be absolute paths and **must** resolve to an existing file or directory respectively.
- **PreprocessorDefinition Validation**: Each `<PreprocessorDefinition>` **must** have a `name` attribute that is a valid C preprocessor identifier (`[A-Za-z_][A-Za-z0-9_]*`). If `optional="true"`, at least one `<Option>` child **should** be present.
- **Library Validation**: Each `<Library>` **must** have a non-empty `name` attribute. Internal libraries (where `external` is absent or `"false"`) **should** have a matching file present under `binaries/`.
- **Empty SourceFileSet**: A `<SourceFileSet>` with no `<SourceFile>` children **should** be avoided.
- **compilerOptions without compiler**: Specifying `compilerOptions` without a `compiler` attribute **should** be avoided.

---

## SSP Rules (1.0 & 2.0)

### Schema Validation

- **SystemStructure.ssd**: **Must** be valid against `SystemStructureDescription.xsd`.
- **Parameter Mapping (.ssm)**: **Must** be valid against `SystemStructureParameterMapping.xsd`.
- **Parameter Values (.ssv)**: **Must** be valid against `SystemStructureParameterValues.xsd`.
- **Signal Dictionary (.ssb)**: **Must** be valid against `SystemStructureSignalDictionary.xsd`.
- **Recursive Validation**: All nested SSD/SSM/SSV/SSB files are recursively discovered and **must** be valid against their respective schemas.
