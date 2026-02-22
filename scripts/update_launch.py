#!/usr/bin/env python3
import json
import os
from pathlib import Path

def update_launch():
    # Get the workspace root (parent of the scripts directory)
    workspace_root = Path(__file__).parent.parent.resolve()
    launch_template_path = workspace_root / ".vscode" / "launch.json.default"
    launch_json_path = workspace_root / ".vscode" / "launch.json"
    models_dir = workspace_root / "models"

    if not launch_template_path.exists():
        print(f"Error: {launch_template_path} not found.")
        return

    if not models_dir.exists():
        print(f"Warning: {models_dir} directory not found.")
        all_files = []
    else:
        # Find all .fmu and .ssp files recursively
        fmu_files = list(models_dir.rglob("*.fmu"))
        ssp_files = list(models_dir.rglob("*.ssp"))
        all_files = sorted(fmu_files + ssp_files)

    # Convert to paths relative to workspaceFolder
    options = [
        f"${{workspaceFolder}}/models/{file.relative_to(models_dir).as_posix()}"
        for file in all_files
    ]

    with open(launch_template_path, "r") as f:
        content = f.read()

    # Simple JSONC comment stripping (handles // comments)
    lines = content.splitlines()
    clean_lines = []
    for line in lines:
        if "//" in line:
            # Very basic check, might fail if // is inside a string
            line = line.split("//")[0]
        clean_lines.append(line)
    clean_content = "\n".join(clean_lines)

    try:
        launch_data = json.loads(clean_content)
    except json.JSONDecodeError as e:
        print(f"Error decoding {launch_template_path}: {e}")
        return

    found = False
    if "inputs" in launch_data:
        for input_item in launch_data["inputs"]:
            if input_item.get("id") == "fmuFile":
                input_item["options"] = options
                if options:
                    # Set default to the first option if the current default is not in the list
                    current_default = input_item.get("default")
                    if current_default not in options:
                        input_item["default"] = options[0]
                else:
                    input_item["default"] = ""
                found = True
                break

    if not found:
        print("Error: Could not find input with id 'fmuFile' in launch.json.default")
        return

    with open(launch_json_path, "w") as f:
        json.dump(launch_data, f, indent=4)
        f.write("\n")

    print(f"Successfully generated {launch_json_path} from template with {len(options)} files.")

if __name__ == "__main__":
    update_launch()
