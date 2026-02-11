#include "resources_checker.h"
#include "certificate.h"
#include "model_checker.h"
#include <filesystem>
#include <string>

void ResourcesChecker::validate(const std::filesystem::path& path, Certificate& cert)
{
    auto resources_dir = path / "resources";
    if (!std::filesystem::exists(resources_dir) || !std::filesystem::is_directory(resources_dir))
        return;

    scanResources(resources_dir, cert);
}

void ResourcesChecker::scanResources(const std::filesystem::path& resources_dir, Certificate& cert)
{
    for (const auto& entry : std::filesystem::directory_iterator(resources_dir))
    {
        if (entry.is_regular_file())
        {
            auto ext = entry.path().extension().string();
            if (ext == ".fmu" || ext == ".ssp")
            {
                ModelChecker nested_checker;
                // validateCore will perform validation and return a certificate
                // The ResourcesChecker inside that validation will handle further nesting
                Certificate nested_cert = nested_checker.validateCore(entry.path());

                NestedModelResult result;
                result.name = entry.path().filename().string();
                result.status = nested_cert.getOverallStatus();
                result.nested_models = nested_cert.getNestedModels();

                cert.addNestedModelResult(result);
            }
        }
        else if (entry.is_directory())
        {
            // For directories, we might want to see if they are directory-based FMUs
            // But for now, let's just recurse to find FMUs in subfolders.
            // If it IS a directory-based FMU, validateCore will handle it.

            // Check if it is a model (has modelDescription.xml or SystemStructure.ssd)
            if (std::filesystem::exists(entry.path() / "modelDescription.xml") ||
                std::filesystem::exists(entry.path() / "SystemStructure.ssd"))
            {
                ModelChecker nested_checker;
                Certificate nested_cert = nested_checker.validateCore(entry.path());

                NestedModelResult result;
                result.name = entry.path().filename().string() + "/";
                result.status = nested_cert.getOverallStatus();
                result.nested_models = nested_cert.getNestedModels();

                cert.addNestedModelResult(result);
            }
            else
            {
                // Just a regular directory, recurse into it
                scanResources(entry.path(), cert);
            }
        }
    }
}
