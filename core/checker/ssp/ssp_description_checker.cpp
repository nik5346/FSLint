#include "ssp_description_checker.h"

#include "certificate.h"
#include "file_utils.h"
#include "xml_utils.h"

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlmemory.h>
#include <libxml/xmlstring.h>

#include <filesystem>
#include <optional>
#include <string>

namespace
{
std::optional<std::string> getXmlAttribute(xmlNodePtr node, const std::string& attr_name)
{
    if (!node)
        return std::nullopt;
    xmlChar* attr = xmlGetProp(node, reinterpret_cast<const xmlChar*>(attr_name.c_str()));
    if (!attr)
        return std::nullopt;
    std::string value(reinterpret_cast<char*>(attr));
    xmlFree(attr);
    return value;
}

} // namespace

void SspDescriptionChecker::validate(const std::filesystem::path& path, Certificate& cert) const
{
    const auto ssd_path = path / "SystemStructure.ssd";
    if (!std::filesystem::exists(ssd_path))
        return;

    xmlDocPtr doc = readXmlFile(ssd_path);
    if (!doc)
        return;

    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (!root)
    {
        xmlFreeDoc(doc);
        return;
    }

    ModelSummary summary = cert.getSummary();
    summary.standard = "SSP";
    summary.modelName = getXmlAttribute(root, "name").value_or("");
    summary.fmiVersion = getXmlAttribute(root, "version").value_or(""); // Standard version
    summary.author = getXmlAttribute(root, "author").value_or("");
    summary.copyright = getXmlAttribute(root, "copyright").value_or("");
    summary.license = getXmlAttribute(root, "license").value_or("");
    summary.description = getXmlAttribute(root, "description").value_or("");
    summary.generationTool = getXmlAttribute(root, "generationTool").value_or("");
    summary.generationDateAndTime = getXmlAttribute(root, "generationDateAndTime").value_or("");
    summary.fmuTypes.clear();

    // SSP has no top-level icon
    summary.hasIcon = false;

    // Calculate total size
    const auto& original_path = getOriginalPath();
    if (!original_path.empty() && std::filesystem::exists(original_path))
        summary.totalSize = file_utils::getTotalSize(original_path);
    else
        summary.totalSize = file_utils::getTotalSize(path);

    cert.setSummary(summary);

    xmlFreeDoc(doc);
}
