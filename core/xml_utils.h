#pragma once

#include <libxml/tree.h> // IWYU pragma: keep

#include <filesystem>

struct _xmlDoc;
typedef struct _xmlDoc* xmlDocPtr;

/// @brief Reads an XML file into a libxml2 document.
/// @param path XML file path.
/// @return xmlDocPtr to document (caller must free).
xmlDocPtr readXmlFile(const std::filesystem::path& path);
