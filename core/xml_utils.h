#pragma once

#include <libxml/tree.h> // IWYU pragma: keep

#include <filesystem>

struct _xmlDoc;
using xmlDocPtr = struct _xmlDoc*;

/// @brief Reads an XML file into a libxml2 document.
/// @param path XML file path.
/// @return xmlDocPtr to document (caller must free).
xmlDocPtr readXmlFile(const std::filesystem::path& path);

/// @brief Checks if an XML document contains a DOCTYPE declaration.
/// @param doc The XML document to check.
/// @return True if a DOCTYPE is present.
bool hasDoctype(xmlDocPtr doc);
