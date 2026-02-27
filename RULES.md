# FSLint - Checked Rules

This file lists all rules currently checked by FSLint, categorized by standard and version.

### Rule Statuses

Each rule check in FSLint results in one of the following statuses:

- **PASS**: The rule is fully satisfied.
- **FAIL**: The rule **must** be satisfied. Violation indicates non-compliance with the FMI or SSP specification that would prevent the model from working correctly in most tools.
- **WARNING**: The rule **should** be satisfied. Violation indicates deviation from recommended best practices, use of deprecated features, or unusual values that might indicate an error.

## Archive Validation (FMU and SSP)

These rules apply to the ZIP archive itself for both FMU and SSP files.

- **File Extension**: Files **must** have the `.fmu` or `.ssp` extension respectively.
- **Disk Spanning**: Split or spanned ZIP archives **must not** be used.
- **Compression Methods**: Only `store` (0) and `deflate` (8) compression methods **must** be used.
- **Version Needed**: The maximum version needed to extract **must** be `2.0` (for compatibility).
- **Encryption**: Encrypted files **must not** be used.
- **Path Format**:
  - Only forward slashes `/` **must** be used (no backslashes `\`).
  - Absolute paths **must not** be used (must not start with `/`).
  - Drive letters or device paths (e.g., `C:`) **must not** be used.
  - Parent directory traversal (`..`) **must not** be used.
  - Non-ASCII characters in paths **should** be avoided.
  - Leading `./` or multiple consecutive slashes `//` **should** be avoided.
- **Symbolic Links**: Symbolic links **must not** be used within the archive.
- **Language Encoding Flag**: Bit 11 **should not** be set (UTF-8 recommended for paths, but keeping bit 11 at 0 is better for old tools).
- **Data Descriptor Consistency**: General purpose bit 3 (data descriptor) is only allowed with `deflate` compression.

## Common FMI Rules (All Versions)

These rules are applied to the `modelDescription.xml` file regardless of the FMI version.

### Metadata and Format
- **Schema Validation**: `modelDescription.xml` is validated against the official FMI XSD schemas. Optional files like `buildDescription.xml` and `terminalsAndIcons.xml` are also schema-validated if present.
- **XML Declaration**: Must be XML version 1.0.
- **UTF-8 Encoding**:
  - FMI 2.0 and 3.0: XML files **must** use UTF-8 encoding.
  - FMI 1.0: UTF-8 encoding is highly **recommended**.
- **Model Name Format**: The `modelName` attribute **must** be present and non-empty.
- **FMI Version Format**: The `fmiVersion` attribute **must** match the standard version string ("1.0", "2.0", or a version matching the FMI 3.0+ regex). For FMI 1.0 and 2.0, only "1.0" and "2.0" are allowed respectively. For FMI 3.0, minor and patch versions are supported (e.g., "3.0", "3.0.1").
- **Generation Date and Time**:
  - **Must** be in ISO 8601 format (e.g., `YYYY-MM-DDThh:mm:ssZ`).
  - **Must** be a valid date in the past (not after the current system time).
  - **Should** not be unreasonably old (before 2010 for FMI 1.0, 2014 for FMI 2.0, 2022 for FMI 3.0).
- **Model Version Format**: The `version` attribute of the model. Semantic versioning (`MAJOR.MINOR.PATCH`) is **recommended**.
- **Copyright Notice**:
  - **Should** begin with ©, "Copyright", or "Copr.".
  - **Should** include the year of publication and the copyright holder name.
- **Traceability Attributes**: The `license`, `author`, and `generationTool` attributes **should** be present and non-empty.
- **Model Identifier Format**:
  - **Must** be a valid C identifier (starts with letter/underscore, followed by alphanumerics/underscores).
  - **Should** be under 64 characters; the absolute maximum is 200 characters (**must** not exceed).
- **Log Categories**: Category names **must** be unique within the `LogCategories` element.

### Variable and Type Consistency
- **Unique Variable Names**: All variables **must** have unique names.
- **Type and Variable Name Clashes**: Type definition names **must** not clash with variable names.
- **Variable Naming Convention**:
  - `flat`: No illegal control characters (U+000D, U+000A, U+0009).
- **Type and Unit References**: All references to types (`declaredType`) and units must exist in `TypeDefinitions` or `UnitDefinitions`.
- **Unused Definitions**: All type and unit definitions **should** be referenced by at least one variable.
- **Min/Max/Start Constraints**:
  - The `start` value **must** be within the `[min, max]` range.
  - The `max` attribute **must** be `>= min`.

### Model Structure and Interfaces
- **Implemented Interfaces**: At least one interface (`CoSimulation`, `ModelExchange`, or `ScheduledExecution`) **must** be implemented.
- **Default Experiment**: The `startTime`, `stopTime`, `tolerance`, and `stepSize` attributes **must** be non-negative and consistent (e.g., `stopTime > startTime`).

### Directory Structure
- **Mandatory Files**: `modelDescription.xml` **must** be present in the FMU root.
- **Distribution**: The FMU **must** contain at least one implementation (a binary for at least one platform or source code).
- **Effectively Empty Directories**: Standard directories like `documentation/` or `resources/` **should not** be effectively empty.
- **Root Entries**: Unknown files or directories in the FMU root **should** be avoided.

### Recursive Validation
- **Nested Models**: FMUs and SSPs located in the `resources/` directory are automatically discovered and validated recursively.

---

## FMI 1.0 Rules

### Model Description
- **GUID Presence**: The `guid` attribute **must** be present and non-empty.
- **Model Identifier Matching**: The `modelIdentifier` **must** match the FMU filename stem (ZIP name).
- **URI-based File References**: In CS `CoSimulation_Tool`, `entryPoint` and `file` attributes **must** use a valid URI scheme (`fmu://`, `file://`, `http://`, or `https://`).
  - URIs using `fmu://` **must** point to existing files within the archive.
  - URIs using `http://` or `https://` **should** be reachable; a warning is issued if the source appears to be offline or unreachable.
  - URIs using `file://` **should** point to existing files if they use an absolute path; a warning is issued if they do not exist on the current system (this may affect portability).
- **Vendor Annotations**: Tool names within `VendorAnnotations` **must** be unique.

### Variable Consistency
- **Legal Variability**: Variability **must** be compatible with the variable's type and causality. Only floating-point types (`Real`) can be `continuous`.
- **Required Start Values**: Variables **must** have a `start` value if `causality` is `input`, `variability` is `constant`, or `causality` is `parameter` and `fixed="true"` (or `fixed` is missing).
- **Illegal Start Values**: Variables **should** only provide a `start` value if allowed by their causality and variability.
- **Causality/Variability/Initial Combinations**: Combinations **must** follow the allowed set defined in the FMI 1.0 specification.

### Binary Exports
- **Function Prefixing**: All exported functions must be prefixed with `<modelIdentifier>_`.
- **Mandatory Functions (CS)**:
  `fmiGetTypesPlatform`, `fmiGetVersion`, `fmiInstantiateSlave`, `fmiInitializeSlave`, `fmiTerminateSlave`, `fmiResetSlave`, `fmiFreeSlaveInstance`, `fmiSetDebugLogging`, `fmiSetReal`, `fmiSetInteger`, `fmiSetBoolean`, `fmiSetString`, `fmiSetRealInputDerivatives`, `fmiGetReal`, `fmiGetInteger`, `fmiGetBoolean`, `fmiGetString`, `fmiGetRealOutputDerivatives`, `fmiDoStep`, `fmiCancelStep`, `fmiGetStatus`, `fmiGetRealStatus`, `fmiGetIntegerStatus`, `fmiGetBooleanStatus`, `fmiGetStringStatus`.
- **Mandatory Functions (ME)**:
  `fmiGetModelTypesPlatform`, `fmiGetVersion`, `fmiInstantiateModel`, `fmiFreeModelInstance`, `fmiSetDebugLogging`, `fmiSetTime`, `fmiSetContinuousStates`, `fmiCompletedIntegratorStep`, `fmiSetReal`, `fmiSetInteger`, `fmiSetBoolean`, `fmiSetString`, `fmiInitialize`, `fmiGetDerivatives`, `fmiGetEventIndicators`, `fmiGetReal`, `fmiGetInteger`, `fmiGetBoolean`, `fmiGetString`, `fmiEventUpdate`, `fmiGetContinuousStates`, `fmiGetNominalContinuousStates`, `fmiGetStateValueReferences`, `fmiTerminate`.

### Directory Structure
- **Documentation Entry Point**: The **recommended** entry point `documentation/_main.html` **should** be present if documentation exists.
- **Standard Headers**: The `sources/` directory **should not** include standard FMI 1.0 headers: `fmiFunctions.h`, `fmiModelFunctions.h`, `fmiModelTypes.h`, `fmiPlatformTypes.h`.

---

## FMI 2.0 Rules

### Schema Validation
- **modelDescription.xml**: **Must** be valid against `fmi2ModelDescription.xsd`.
- **buildDescription.xml**: **Must** be valid against `fmi3BuildDescription.xsd` (if present in `sources/`).
- **terminalsAndIcons.xml**: **Must** be valid against `fmi3TerminalsAndIcons.xsd` (if present in `terminalsAndIcons/`).

### Model Description
- **Enumeration Variables**: **Must** have a `declaredType` attribute.
- **Variable Naming Convention**:
  - `structured`: **Must** follow the structured name syntax. This includes:
    - Identifiers: `name.subname`
    - Array indices: `name[1,2]`
    - Quoted names for special characters: `'name with spaces'` (supports escapes like `\'`, `\n`, etc.)
    - Derivatives: `der(name)` or `der(name, 2)`
- **Alias Variables (same VR)**:
  - At most one variable in an alias set **can** be settable with `fmi2SetXXX`.
  - At most one variable in an alias set (where at least one is not constant) **can** have a `start` attribute.
  - Constants **can** only be aliased to other constants and **must** have identical `start` values.
  - All variables in an alias set **must** have the same unit.
- **Independent Variable**: At most one **is** allowed; it **must** be of type `Real`, have no `start`/`initial` attribute, and have `variability="continuous"`.
- **Reinit Attribute**: Allowed **only** for continuous-time states; **not allowed** in Co-Simulation only FMUs.
- **Multiple Set Attribute**: `canHandleMultipleSetPerTimeInstant` **only** allowed for inputs; **not allowed** in Co-Simulation only FMUs.
- **Continuous-time States and Derivatives**:
  - Derivatives **must** have `variability="continuous"`.
  - States **must** have `causality="local"` or `"output"` and `variability="continuous"`.
  - All **must** be of type `Real`.
- **Model Structure**:
  - `Outputs`, `Derivatives`, and `InitialUnknowns` **must** be complete (match all variables with respective causalities/attributes) and correctly ordered.
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
- **Unique Names**: Terminal and member names **must** be unique within their respective levels.
- **Variable References**: All `variableName` references **must** exist in `modelDescription.xml`.
- **Causality Constraints**: Variables used as stream members or flows **must** have compatible causalities (input, output, parameter, etc.).
- **Graphical Representation**:
  - Icons **must** exist (`iconBaseName` must point to a PNG file).
  - Colors **must** be valid RGB (3 space-separated values).
- **Stream/Flow Constraints**: Only one inflow/outflow **is** allowed when a stream variable is present in a terminal.

### Binary Exports
- **Mandatory Functions**:
  `fmi2GetTypesPlatform`, `fmi2GetVersion`, `fmi2SetDebugLogging`, `fmi2Instantiate`, `fmi2FreeInstance`, `fmi2SetupExperiment`, `fmi2EnterInitializationMode`, `fmi2ExitInitializationMode`, `fmi2Terminate`, `fmi2Reset`, `fmi2GetReal`, `fmi2GetInteger`, `fmi2GetBoolean`, `fmi2GetString`, `fmi2SetReal`, `fmi2SetInteger`, `fmi2SetBoolean`, `fmi2SetString`, `fmi2GetFMUstate`, `fmi2SetFMUstate`, `fmi2FreeFMUstate`, `fmi2SerializedFMUstateSize`, `fmi2SerializeFMUstate`, `fmi2DeSerializeFMUstate`, `fmi2GetDirectionalDerivative`, `fmi2EnterEventMode`, `fmi2NewDiscreteStates`, `fmi2EnterContinuousTimeMode`, `fmi2CompletedIntegratorStep`, `fmi2SetTime`, `fmi2SetContinuousStates`, `fmi2GetDerivatives`, `fmi2GetEventIndicators`, `fmi2GetContinuousStates`, `fmi2GetNominalsOfContinuousStates`, `fmi2SetRealInputDerivatives`, `fmi2GetRealOutputDerivatives`, `fmi2DoStep`, `fmi2CancelStep`, `fmi2GetStatus`, `fmi2GetRealStatus`, `fmi2GetIntegerStatus`, `fmi2GetBooleanStatus`, `fmi2GetStringStatus`.

### Directory Structure
- **model.png Existence**: It is **recommended** to provide an icon `model.png` in the FMU root.
- **External Dependencies**: If `needsExecutionTool="true"`, `documentation/externalDependencies.{txt|html}` **must** be present.
- **Licenses**: If a `licenses/` directory exists, it **should** contain an entry point (`license.txt` or `license.html`).
- **Source Files Consistency**:
  - Typical source files (extensions: `.c`, `.cc`, `.cpp`, `.cxx`, `.C`, `.c++`) in `sources/` **should** be listed in `modelDescription.xml` `<SourceFiles>`.
  - `SourceFiles/File` entries **must** point to existing files in `sources/`.
- **FMI 2.0.4 Compatibility**: Source-only FMUs **should** provide both `<SourceFiles>` in `modelDescription.xml` and a `buildDescription.xml` for maximum compatibility.
- **Standard Headers**: The `sources/` directory **should not** include standard FMI 2.0 headers: `fmi2Functions.h`, `fmi2FunctionTypes.h`, `fmi2TypesPlatform.h`.

### Build Description
- **fmiVersion Check**: `buildDescription.xml` **must** have `fmiVersion="3.0"`. This attribute is fixed to "3.0" for FMI 2.0 FMUs because the feature was backported from FMI 3.0.
- **Source/Include Validation**: All listed `SourceFile` and `IncludeDirectory` entries **must** exist in `sources/` and **must not** contain `..` traversal.
- **Attribute Validation**: The `language` and `compiler` attributes **should** be from the suggested sets:
  - `language`: `C89`, `C90`, `C99`, `C11`, `C17`, `C18`, `C23`, `C++98`, `C++03`, `C++11`, `C++14`, `C++17`, `C++20`, `C++23`, `C++26`.
  - `compiler`: `gcc`, `clang`, `msvc`.
- **Model Identifier Match**: `modelIdentifier` in `BuildConfiguration` must match an identifier from `modelDescription.xml`.

---

## FMI 3.0 Rules

### Model Description
- **instantiationToken**: The `instantiationToken` **should** follow the GUID format.
- **Variable Naming Convention**:
  - `structured`: **Must** follow the structured name syntax. This includes:
    - Identifiers: `name.subname`
    - Array indices: `name[1,2]`
    - Quoted names for special characters: `'name with spaces'` (supports escapes like `\'`, `\n`, etc.)
    - Derivatives: `der(name)` or `der(name, 2)`
- **Annotations**: Annotation types within `Annotations` **must** be unique within their container.
- **Independent Variable**: Exactly one **is** allowed; it **must** be `Float32` or `Float64`, and have no `initial` or `start` attribute.
- **Derivative Consistency**:
  - Derivatives and states **must** have `variability="continuous"`.
  - **Must** be `Float32` or `Float64`.
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
  - `Output`, `ContinuousStateDerivative`, `ClockedState`, `InitialUnknown`, and `EventIndicator` elements **must** be complete and unique.
  - Mandatory `InitialUnknowns` **must** be provided for non-clocked outputs, calculated parameters, and states/derivatives with `initial="approx"` or `"calculated"`.
- **Variable Dependencies**: `dependenciesKind` **must** be restricted to allowed types and is **not allowed** for `InitialUnknown`.

### Variable Consistency
- **Legal Variability**: Variability **must** be compatible with the variable's type and causality. Only floating-point types (`Float32`, `Float64`) **can** be `continuous`. Parameters (`causality="parameter"`, `"calculatedParameter"`, or `"structuralParameter"`) **must** be `"fixed"` or `"tunable"`.
- **Required Start Values**: Variables **must** have a `start` value if `causality` is `input`, `parameter`, or `structuralParameter`, `variability` is `constant`, or `initial` is `exact` or `approx`. Variables of type `Clock` are excluded from this requirement.
- **Illegal Start Values**: Variables with `initial="calculated"` or `causality="independent"` **must not** provide a `start` value.
- **Causality/Variability/Initial Combinations**: Combinations **must** follow the allowed set defined in the FMI 3.0 specification.

### Terminals and Icons
- **Unique Names**: Terminal and member names **must** be unique within their respective levels.
- **Variable References**: All `variableName` references **must** exist in `modelDescription.xml`.
- **Causality Constraints**: Variables used as stream members or flows **must** have compatible causalities (input, output, parameter, etc.).
- **Graphical Representation**:
  - Icons **must** exist (`iconBaseName` must point to a PNG file).
  - PNG fallback required if SVG is provided in `terminalsAndIcons/`.
  - Colors **must** be valid RGB (3 space-separated values).
- **Stream/Flow Constraints**: Only one inflow/outflow allowed when a stream variable is present in a terminal.

### Binary Exports
- **Mandatory Functions**:
  `fmi3GetVersion`, `fmi3SetDebugLogging`, `fmi3InstantiateModelExchange`, `fmi3InstantiateCoSimulation`, `fmi3InstantiateScheduledExecution`, `fmi3FreeInstance`, `fmi3EnterInitializationMode`, `fmi3ExitInitializationMode`, `fmi3EnterEventMode`, `fmi3Terminate`, `fmi3Reset`, `fmi3GetFloat32`, `fmi3GetFloat64`, `fmi3GetInt8`, `fmi3GetUInt8`, `fmi3GetInt16`, `fmi3GetUInt16`, `fmi3GetInt32`, `fmi3GetUInt32`, `fmi3GetInt64`, `fmi3GetUInt64`, `fmi3GetBoolean`, `fmi3GetString`, `fmi3GetBinary`, `fmi3GetClock`, `fmi3SetFloat32`, `fmi3SetFloat64`, `fmi3SetInt8`, `fmi3SetUInt8`, `fmi3SetInt16`, `fmi3SetUInt16`, `fmi3SetInt32`, `fmi3SetUInt32`, `fmi3SetInt64`, `fmi3SetUInt64`, `fmi3SetBoolean`, `fmi3SetString`, `fmi3SetBinary`, `fmi3SetClock`, `fmi3GetNumberOfVariableDependencies`, `fmi3GetVariableDependencies`, `fmi3GetFMUState`, `fmi3SetFMUState`, `fmi3FreeFMUState`, `fmi3SerializedFMUStateSize`, `fmi3SerializeFMUState`, `fmi3DeserializeFMUState`, `fmi3GetDirectionalDerivative`, `fmi3GetAdjointDerivative`, `fmi3EnterConfigurationMode`, `fmi3ExitConfigurationMode`, `fmi3GetIntervalDecimal`, `fmi3GetIntervalFraction`, `fmi3GetShiftDecimal`, `fmi3GetShiftFraction`, `fmi3SetIntervalDecimal`, `fmi3SetIntervalFraction`, `fmi3SetShiftDecimal`, `fmi3SetShiftFraction`, `fmi3EvaluateDiscreteStates`, `fmi3UpdateDiscreteStates`, `fmi3EnterContinuousTimeMode`, `fmi3CompletedIntegratorStep`, `fmi3SetTime`, `fmi3SetContinuousStates`, `fmi3GetContinuousStateDerivatives`, `fmi3GetEventIndicators`, `fmi3GetContinuousStates`, `fmi3GetNominalsOfContinuousStates`, `fmi3GetNumberOfEventIndicators`, `fmi3GetNumberOfContinuousStates`, `fmi3EnterStepMode`, `fmi3GetOutputDerivatives`, `fmi3DoStep`, `fmi3ActivateModelPartition`.

### Directory Structure
- **External Dependencies**: If `needsExecutionTool="true"`, `documentation/externalDependencies.{txt|html}` **must** be present.
- **Documentation Entry Point**: `documentation/index.html` is the **recommended** entry point.
- **Diagrams**: If `documentation/diagram.svg` is provided, a `documentation/diagram.png` fallback **must** also be present.
- **Build Description**: If the `sources/` directory exists, `sources/buildDescription.xml` **must** be present.
- **Platform Tuples**: Platform directories **should** follow the `<arch>-<sys>[-<abi>]` format.
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

---

## SSP Rules (1.0 & 2.0)

### Schema Validation
- **SystemStructure.ssd**: **Must** be valid against `SystemStructureDescription.xsd`.
- **Parameter Mapping (.ssm)**: **Must** be valid against `SystemStructureParameterMapping.xsd`.
- **Parameter Values (.ssv)**: **Must** be valid against `SystemStructureParameterValues.xsd`.
- **Signal Dictionary (.ssb)**: **Must** be valid against `SystemStructureSignalDictionary.xsd`.
- **Recursive Validation**: All nested SSD/SSM/SSV/SSB files are recursively discovered and **must** be valid against their respective schemas.
