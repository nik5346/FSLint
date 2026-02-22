#!/usr/bin/env python3
import json
import os
from pathlib import Path

def update_launch():
    # Get the workspace root (parent of the scripts directory)
    workspace_root = Path(__file__).parent.parent.resolve()
    launch_json_path = workspace_root / ".vscode" / "launch.json"
    models_dir = workspace_root / "models"

    if not launch_json_path.exists():
        print(f"Error: {launch_json_path} not found.")
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

    with open(launch_json_path, "r") as f:
        try:
            launch_data = json.load(f)
        except json.JSONDecodeError as e:
            print(f"Error decoding {launch_json_path}: {e}")
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
        print("Error: Could not find input with id 'fmuFile' in launch.json")
        return

    with open(launch_json_path, "w") as f:
        json.dump(launch_data, f, indent=4)
        f.write("\n")

    print(f"Successfully updated {launch_json_path} with {len(options)} files.")

if __name__ == "__main__":
    update_launch()
