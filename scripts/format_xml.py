#!/usr/bin/env python3
import os
import xml.etree.ElementTree as ET
import sys
import io

def format_xml(filepath):
    if os.path.getsize(filepath) == 0:
        return

    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
            if not content.strip():
                return

        # Attempt to parse
        try:
            # Extract namespaces for registration
            namespaces = {}
            # Use a fresh BytesIO for iterparse
            f_bytes = io.BytesIO(content.encode('utf-8'))
            for event, elem in ET.iterparse(f_bytes, events=('start-ns',)):
                prefix, uri = elem
                namespaces[prefix] = uri

            for prefix, uri in namespaces.items():
                ET.register_namespace(prefix, uri)

            parser = ET.XMLParser(target=ET.TreeBuilder(insert_comments=True))
            root = ET.fromstring(content.encode('utf-8'), parser=parser)
            tree = ET.ElementTree(root)

            ET.indent(root, space='  ')

            with open(filepath, 'wb') as f:
                f.write(b'<?xml version="1.0" encoding="UTF-8"?>\n')
                tree.write(f, encoding='UTF-8', xml_declaration=False)
                f.write(b'\n')
        except ET.ParseError:
            # Skip malformed files - they are likely intentional test cases
            print(f"Skipping malformed XML: {filepath}")
            return

    except Exception as e:
        print(f"Error formatting {filepath}: {e}", file=sys.stderr)

def main():
    # Use the script's directory to find the repository root
    script_dir = os.path.dirname(os.path.realpath(__file__))
    repo_root = os.path.abspath(os.path.join(script_dir, '..'))
    test_data_dir = os.path.join(repo_root, 'tests', 'data')

    if not os.path.isdir(test_data_dir):
        print(f"Directory {test_data_dir} not found.")
        sys.exit(1)

    for root_dir, _, files in os.walk(test_data_dir):
        for file in files:
            if file.endswith('.xml') or file.endswith('.ssd'):
                filepath = os.path.join(root_dir, file)
                format_xml(filepath)

if __name__ == "__main__":
    main()
