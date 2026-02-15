# FSLint - Checked Rules

This file lists all rules currently checked by FSLint, categorized by standard and version.

## Archive Validation (FMU and SSP)

These rules apply to the ZIP archive itself for both FMU and SSP files.

- **File Extension**: Must have `.fmu` or `.ssp` extension respectively.
- **Disk Spanning**: Split or spanned ZIP archives are not allowed.
- **Compression Methods**: Only `store` (0) and `deflate` (8) compression methods are allowed.
- **Version Needed**: Maximum version needed to extract must be `2.0` (for compatibility).
- **Encryption**: Encrypted files are not allowed.
- **Path Format**:
  - Only forward slashes `/` are allowed (no backslashes `\`).
  - No absolute paths (must not start with `/`).
  - No drive letters or device paths (e.g., `C:`).
  - No parent directory traversal (`..`).
  - WARNING for non-ASCII characters.
  - WARNING for leading `./` or multiple consecutive slashes `//`.
- **Symbolic Links**: Symbolic links are not allowed within the archive.
- **Language Encoding Flag**: WARNING if bit 11 is set (UTF-8 recommended for paths, but keeping bit 11 at 0 is better for old tools).
- **Data Descriptor Consistency**: General purpose bit 3 (data descriptor) is only allowed with `deflate` compression.

## Common FMI Rules (All Versions)

These rules are applied to the `modelDescription.xml` file regardless of the FMI version.

### Metadata and Format
- **Schema Validation**: `modelDescription.xml` is validated against the official FMI XSD schemas. Optional files like `buildDescription.xml` and `terminalsAndIcons.xml` are also schema-validated if present.
- **XML Declaration**: Must be XML version 1.0.
- **UTF-8 Encoding**:
  - Required for FMI 2.0 and 3.0.
  - Highly recommended for FMI 1.0 (issued as a WARNING if other encodings are used).
- **Model Name Format**: `modelName` attribute must be present and non-empty.
- **FMI Version Format**: `fmiVersion` attribute must match the expected version string ("1.0", "2.0", or "3.0").
- **Generation Date and Time**:
  - Must be in ISO 8601 format (e.g., `YYYY-MM-DDThh:mm:ssZ`).
  - Must be a valid date in the past (not after the current system time).
  - Should not be unreasonably old (before 2010).
- **Model Version Format**: Semantic versioning (`MAJOR.MINOR.PATCH`) is recommended.
- **Copyright Notice**:
  - Should begin with ©, "Copyright", or "Copr.".
  - Should include the year of publication and the copyright holder name.
- **Traceability Attributes**: `license`, `author`, and `generationTool` presence and non-emptiness are checked (WARNING if missing).
- **Model Identifier Format**:
  - Must be a valid C identifier (starts with letter/underscore, followed by alphanumerics/underscores).
  - Recommended length under 64 characters; absolute maximum 200 characters.
- **Log Categories**: Category names must be unique within the `LogCategories` element.
- **Vendor Annotations**: Tool names within `VendorAnnotations` must be unique.

### Variable and Type Consistency
- **Unique Variable Names**: All `ScalarVariable` names must be unique.
- **Type and Variable Name Clashes**: Type definition names must not clash with variable names.
- **Variable Naming Convention**:
  - `flat`: No illegal control characters (U+000D, U+000A, U+0009).
  - `structured`: Must follow the structured name syntax (e.g., `a.b[1]`).
- **Legal Variability**: Variability must be compatible with the variable's type and causality. Only floating-point types can be `continuous`.
- **Required Start Values**: Variables with certain causality/variability/initial combinations (e.g., `causality="parameter"`, `initial="exact"`) must have a `start` value.
- **Illegal Start Values**: Variables with `initial="calculated"` or `causality="independent"` should not provide a `start` value.
- **Causality/Variability/Initial Combinations**: Combinations must follow the allowed set defined in the FMI specification tables.
- **Type and Unit References**: All references to types (`declaredType`) and units must exist in `TypeDefinitions` or `UnitDefinitions`.
- **Unused Definitions**: WARNING if a type or unit definition is not referenced by any variable.
- **Min/Max/Start Constraints**: `start` values must be within the `[min, max]` range. `max` must be `>= min`.

### Model Structure and Interfaces
- **Implemented Interfaces**: At least one interface (`CoSimulation`, `ModelExchange`, or `ScheduledExecution`) must be implemented.
- **Default Experiment**: `startTime`, `stopTime`, `tolerance`, and `stepSize` must be non-negative and consistent (e.g., `stopTime > startTime`).

### Directory Structure
- **Mandatory Files**: `modelDescription.xml` must be present in the FMU root.
- **Standard Headers**: `sources/` should not include standard FMI headers (e.g., `fmi2Functions.h`, `fmi3PlatformTypes.h`).
- **Distribution**: The FMU must contain at least one implementation (binary for at least one platform or source code).
- **Effectively Empty Directories**: WARNING if standard directories like `documentation/` or `resources/` are empty (contain only `.gitkeep`).

---

## FMI 1.0 Rules

### Schema Validation
- **modelDescription.xml**: Validated against `fmiModelDescription.xsd`.

### Model Description
- **GUID Presence**: `guid` attribute is mandatory and must not be empty.
- **Model Identifier Matching**: `modelIdentifier` must match the FMU filename stem (ZIP name).
- **Implementation Check**: Co-Simulation FMUs must have an `<Implementation>` element.
- **URI-based File References**: In `CoSimulation_Tool`, `entryPoint` and `file` attributes using `fmu://` must point to existing files within the archive.

### Binary Exports
- **Function Prefixing**: All exported functions must be prefixed with `<modelIdentifier>_`.
- **Mandatory Functions (CS)**: `fmiGetTypesPlatform`, `fmiGetVersion`, `fmiInstantiateSlave`, `fmiInitializeSlave`, `fmiTerminateSlave`, `fmiResetSlave`, `fmiFreeSlaveInstance`, `fmiSetDebugLogging`, `fmiSetReal`, `fmiSetInteger`, `fmiSetBoolean`, `fmiSetString`, `fmiSetRealInputDerivatives`, `fmiGetReal`, `fmiGetInteger`, `fmiGetBoolean`, `fmiGetString`, `fmiGetRealOutputDerivatives`, `fmiDoStep`, `fmiCancelStep`, `fmiGetStatus`, `fmiGetRealStatus`, `fmiGetIntegerStatus`, `fmiGetBooleanStatus`, `fmiGetStringStatus`.
- **Mandatory Functions (ME)**: `fmiGetModelTypesPlatform`, `fmiGetVersion`, `fmiInstantiateModel`, `fmiFreeModelInstance`, `fmiSetDebugLogging`, `fmiSetTime`, `fmiSetContinuousStates`, `fmiCompletedIntegratorStep`, `fmiSetReal`, `fmiSetInteger`, `fmiSetBoolean`, `fmiSetString`, `fmiInitialize`, `fmiGetDerivatives`, `fmiGetEventIndicators`, `fmiGetReal`, `fmiGetInteger`, `fmiGetBoolean`, `fmiGetString`, `fmiEventUpdate`, `fmiGetContinuousStates`, `fmiGetNominalContinuousStates`, `fmiGetStateValueReferences`, `fmiTerminate`.

### Directory Structure
- **Documentation Entry Point**: Recommended entry point `documentation/_main.html` should be present if documentation exists.

---

## FMI 2.0 Rules

### Schema Validation
- **modelDescription.xml**: Validated against `fmi2ModelDescription.xsd`.
- **buildDescription.xml**: Validated against `fmi3BuildDescription.xsd` (if present in `sources/`).
- **terminalsAndIcons.xml**: Validated against `fmi3TerminalsAndIcons.xsd` (if present in `terminalsAndIcons/`).

### Model Description
- **Enumeration Variables**: Must have a `declaredType` attribute.
- **Alias Variables (same VR)**:
  - At most one variable in an alias set can be settable with `fmi2SetXXX`.
  - At most one variable in an alias set (where at least one is not constant) can have a `start` attribute.
  - Constants can only be aliased to other constants and must have identical `start` values.
  - All variables in an alias set must have the same unit.
- **Independent Variable**: At most one allowed; must be type `Real`, have no `start`/`initial`, and have `variability="continuous"`.
- **Reinit Attribute**: Allowed only for continuous-time states; not allowed in Co-Simulation only FMUs.
- **Multiple Set Attribute**: `canHandleMultipleSetPerTimeInstant` only allowed for inputs; not allowed in Co-Simulation only FMUs.
- **Continuous-time States and Derivatives**:
  - Derivatives must have `variability="continuous"`.
  - States must have `causality="local"` or `"output"` and `variability="continuous"`.
  - All must be of type `Real`.
- **Model Structure**:
  - `Outputs`, `Derivatives`, and `InitialUnknowns` must be complete (match all variables with respective causalities/attributes) and correctly ordered.
  - `Unknown` indices must be 1-based and within range.
  - Dependencies and `dependenciesKind` must be consistent in size.
- **Prohibited Special Floats**: Attributes like `min`, `max`, `nominal`, `factor`, `offset`, `startTime`, `stopTime`, `tolerance`, and `stepSize` must not contain `NaN` or `INF`.

### Binary Exports
- **Mandatory Functions**: `fmi2GetTypesPlatform`, `fmi2GetVersion`, `fmi2SetDebugLogging`, `fmi2Instantiate`, `fmi2FreeInstance`, `fmi2SetupExperiment`, `fmi2EnterInitializationMode`, `fmi2ExitInitializationMode`, `fmi2Terminate`, `fmi2Reset`, `fmi2GetReal`, `fmi2GetInteger`, `fmi2GetBoolean`, `fmi2GetString`, `fmi2SetReal`, `fmi2SetInteger`, `fmi2SetBoolean`, `fmi2SetString`, `fmi2GetFMUstate`, `fmi2SetFMUstate`, `fmi2FreeFMUstate`, `fmi2SerializedFMUstateSize`, `fmi2SerializeFMUstate`, `fmi2DeSerializeFMUstate`, `fmi2GetDirectionalDerivative`, `fmi2EnterEventMode`, `fmi2NewDiscreteStates`, `fmi2EnterContinuousTimeMode`, `fmi2CompletedIntegratorStep`, `fmi2SetTime`, `fmi2SetContinuousStates`, `fmi2GetDerivatives`, `fmi2GetEventIndicators`, `fmi2GetContinuousStates`, `fmi2GetNominalsOfContinuousStates`, `fmi2SetRealInputDerivatives`, `fmi2GetRealOutputDerivatives`, `fmi2DoStep`, `fmi2CancelStep`, `fmi2GetStatus`, `fmi2GetRealStatus`, `fmi2GetIntegerStatus`, `fmi2GetBooleanStatus`, `fmi2GetStringStatus`.

### Directory Structure
- **model.png Existence**: Recommended to provide an icon.
- **External Dependencies**: If `needsExecutionTool="true"`, `documentation/externalDependencies.{txt|html}` must be present.
- **Licenses**: If a `licenses/` directory exists, it should contain an entry point (`license.txt` or `license.html`).
- **Source Files Consistency**: Files present in `sources/` should be listed in `modelDescription.xml` `<SourceFiles>`.
- **FMI 2.0.4 Compatibility**: Source-only FMUs should ideally provide both `<SourceFiles>` and `buildDescription.xml`.

---

## FMI 3.0 Rules

### Schema Validation
- **modelDescription.xml**: Validated against `fmi3ModelDescription.xsd`.
- **buildDescription.xml**: Validated against `fmi3BuildDescription.xsd` (if present in `sources/`).
- **terminalsAndIcons.xml**: Validated against `fmi3TerminalsAndIcons.xsd` (if present in `terminalsAndIcons/`).
- **fmi-ls-manifest.xml**: Validated against `fmi3LayeredStandardManifest.xsd` (if present in `extra/`).

### Model Description
- **instantiationToken**: Recommended to follow GUID format.
- **Independent Variable**: Exactly one allowed; must be `Float32` or `Float64`, and have no `initial` or `start` attribute.
- **Derivative Consistency**:
  - Derivatives and states must have `variability="continuous"`.
  - Must be `Float32` or `Float64`.
  - Dimensions of derivative must match dimensions of the state.
- **Structural Parameters**: Must be of type `UInt64`; if used in `<Dimension>`, `start` must be `> 0`.
- **Dimensions**:
  - Must have either `start` or `valueReference`, but not both.
  - Fixed dimensions (`start`) must be `> 0`.
- **Array Start Values**: Number of values must match total array size or be exactly 1 (for broadcast).
- **Clocks**:
  - Clock references must exist and point to variables of type `Clock`.
  - Clocks cannot reference themselves.
  - Clocked variables must be discrete and have specific causality.
  - Clock types must have consistent `intervalVariability` and interval attributes.
- **Model Structure**:
  - `Output`, `ContinuousStateDerivative`, `ClockedState`, `InitialUnknown`, and `EventIndicator` elements must be complete and unique.
  - Mandatory `InitialUnknowns` include:
    - Outputs with `initial="approx"` or `"calculated"` (excluding clocked).
    - Calculated parameters.
    - Continuous-time states and derivatives with `initial="approx"` or `"calculated"`.
- **Variable Dependencies**: `dependenciesKind` is restricted for certain types and disallowed for `InitialUnknown`.

### Terminals and Icons
- **Unique Names**: Terminal and member names must be unique within their respective levels.
- **Variable References**: All `variableName` references must exist in `modelDescription.xml`.
- **Causality Constraints**: Variables used as stream members or flows must have compatible causalities (input, output, parameter, etc.).
- **Graphical Representation**: Icons must exist (PNG fallback for SVG), and colors must be valid RGB (3 space-separated values).
- **Stream/Flow Constraints**: Only one inflow/outflow allowed when a stream variable is present in a terminal.

### Binary Exports
- **Mandatory Functions**: `fmi3GetVersion`, `fmi3SetDebugLogging`, `fmi3InstantiateModelExchange`, `fmi3InstantiateCoSimulation`, `fmi3InstantiateScheduledExecution`, `fmi3FreeInstance`, `fmi3EnterInitializationMode`, `fmi3ExitInitializationMode`, `fmi3EnterEventMode`, `fmi3Terminate`, `fmi3Reset`, `fmi3GetFloat32`, `fmi3GetFloat64`, `fmi3GetInt8`, `fmi3GetUInt8`, `fmi3GetInt16`, `fmi3GetUInt16`, `fmi3GetInt32`, `fmi3GetUInt32`, `fmi3GetInt64`, `fmi3GetUInt64`, `fmi3GetBoolean`, `fmi3GetString`, `fmi3GetBinary`, `fmi3GetClock`, `fmi3SetFloat32`, `fmi3SetFloat64`, `fmi3SetInt8`, `fmi3SetUInt8`, `fmi3SetInt16`, `fmi3SetUInt16`, `fmi3SetInt32`, `fmi3SetUInt32`, `fmi3SetInt64`, `fmi3SetUInt64`, `fmi3SetBoolean`, `fmi3SetString`, `fmi3SetBinary`, `fmi3SetClock`, `fmi3GetNumberOfVariableDependencies`, `fmi3GetVariableDependencies`, `fmi3GetFMUState`, `fmi3SetFMUState`, `fmi3FreeFMUState`, `fmi3SerializedFMUStateSize`, `fmi3SerializeFMUState`, `fmi3DeserializeFMUState`, `fmi3GetDirectionalDerivative`, `fmi3GetAdjointDerivative`, `fmi3EnterConfigurationMode`, `fmi3ExitConfigurationMode`, `fmi3GetIntervalDecimal`, `fmi3GetIntervalFraction`, `fmi3GetShiftDecimal`, `fmi3GetShiftFraction`, `fmi3SetIntervalDecimal`, `fmi3SetIntervalFraction`, `fmi3SetShiftDecimal`, `fmi3SetShiftFraction`, `fmi3EvaluateDiscreteStates`, `fmi3UpdateDiscreteStates`, `fmi3EnterContinuousTimeMode`, `fmi3CompletedIntegratorStep`, `fmi3SetTime`, `fmi3SetContinuousStates`, `fmi3GetContinuousStateDerivatives`, `fmi3GetEventIndicators`, `fmi3GetContinuousStates`, `fmi3GetNominalsOfContinuousStates`, `fmi3GetNumberOfEventIndicators`, `fmi3GetNumberOfContinuousStates`, `fmi3EnterStepMode`, `fmi3GetOutputDerivatives`, `fmi3DoStep`, `fmi3ActivateModelPartition`.

### Directory Structure
- **External Dependencies**: If `needsExecutionTool="true"`, `documentation/externalDependencies.{txt|html}` is mandatory.
- **Documentation Entry Point**: `documentation/index.html` is the recommended entry point.
- **Diagrams**: `diagram.svg` requires a `diagram.png` fallback.
- **Build Description**: If `sources/` exists, `sources/buildDescription.xml` is mandatory.
- **Platform Tuples**: Platform directories should follow `<arch>-<sys>[-<abi>]` format.
- **Static Linking**: If static libraries are present, `documentation/staticLinking.{txt|html}` is mandatory.
- **Extra Directory**: Subdirectories should use reverse domain name notation (e.g., `com.example`).

---

## SSP Rules (1.0 & 2.0)

### Schema Validation
- **SystemStructure.ssd**: Validated against `SystemStructureDescription.xsd`.
- **Parameter Mapping (.ssm)**: Validated against `SystemStructureParameterMapping.xsd`.
- **Parameter Values (.ssv)**: Validated against `SystemStructureParameterValues.xsd`.
- **Signal Dictionary (.ssb)**: Validated against `SystemStructureSignalDictionary.xsd`.
- **Recursive Validation**: All nested SSD/SSM/SSV/SSB files are recursively discovered and validated against their respective schemas.
