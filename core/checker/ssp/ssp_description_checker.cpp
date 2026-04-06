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
    if (node == nullptr)
        return std::nullopt;

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    xmlChar* attr = xmlGetProp(node, reinterpret_cast<const xmlChar*>(attr_name.c_str()));
    if (attr == nullptr)
        return std::nullopt;

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
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
    summary.model_name = getXmlAttribute(root, "name").value_or("");
    summary.fmi_version = getXmlAttribute(root, "version").value_or(""); // Standard version
    summary.author = getXmlAttribute(root, "author").value_or("");
    summary.copyright = getXmlAttribute(root, "copyright").value_or("");
    summary.license = getXmlAttribute(root, "license").value_or("");
    summary.description = getXmlAttribute(root, "description").value_or("");
    summary.generation_tool = getXmlAttribute(root, "generationTool").value_or("");
    summary.generation_date_and_time = getXmlAttribute(root, "generationDateAndTime").value_or("");
    summary.fmu_types.clear();

    // SSP has no top-level icon
    summary.has_icon = false;

    // Calculate total size
    const auto& original_path = getOriginalPath();
    if (!original_path.empty() && std::filesystem::exists(original_path))
        summary.total_size = file_utils::getTotalSize(original_path);
    else
        summary.total_size = file_utils::getTotalSize(path);

    cert.setSummary(summary);

    xmlFreeDoc(doc);
}
