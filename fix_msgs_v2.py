import re

filepath = 'core/checker/model_description/fmi2_model_description_checker.cpp'
with open(filepath, 'r') as f:
    lines = f.readlines()

in_outputs = False
in_derivatives = False
new_lines = []

for line in lines:
    if 'void Fmi2ModelDescriptionChecker::validateOutputs' in line:
        in_outputs = True
    elif 'void Fmi2ModelDescriptionChecker::validateDerivatives' in line:
        in_outputs = False
        in_derivatives = True
    elif 'void Fmi2ModelDescriptionChecker::validateInitialUnknowns' in line:
        in_derivatives = False

    if in_outputs:
        line = line.replace('InitialUnknowns', 'Outputs')
    elif in_derivatives:
        line = line.replace('InitialUnknowns', 'Derivatives')

    new_lines.append(line)

with open(filepath, 'w') as f:
    f.writelines(new_lines)
