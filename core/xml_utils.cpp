#include "xml_utils.h"

#include "file_utils.h"

#include <libxml/parser.h>
#include <libxml/tree.h> // IWYU pragma: keep

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

xmlDocPtr readXmlFile(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        return nullptr;

    const std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(static_cast<size_t>(size));
    if (!file.read(buffer.data(), size))
        return nullptr;

    const std::string path_str = file_utils::pathToUtf8(path);
    return xmlReadMemory(buffer.data(), static_cast<int>(size), path_str.c_str(), nullptr,
                         XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NOENT | XML_PARSE_NONET);
}

bool hasDoctype(xmlDocPtr doc)
{
    if (!doc)
        return false;

    for (xmlNodePtr node = doc->children; node != nullptr; node = node->next)
        if (node->type == XML_DTD_NODE)
            return true;

    return false;
}
