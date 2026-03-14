#pragma once

#include <libxml/tree.h> // IWYU pragma: keep

#include <filesystem>

struct _xmlDoc;
typedef struct _xmlDoc* xmlDocPtr;

/**
 * Robustly reads an XML file into a libxml2 document.
 * This helper uses std::ifstream to read the file into memory, which avoids
 * issues with passing UTF-8 paths to libxml2's file-based APIs, especially on Windows.
 */
xmlDocPtr readXmlFile(const std::filesystem::path& path);
