#include "resources_checker.h"

#include "certificate.h"
#include "file_utils.h"
#include "model_checker.h"

#include <filesystem>
#include <functional>
#include <string>
#include <utility>

void ResourcesChecker::validate(const std::filesystem::path& path, Certificate& cert) const
{
    const auto resources_dir = path / "resources";
    if (!std::filesystem::exists(resources_dir) || !std::filesystem::is_directory(resources_dir))
        return;

    scanResources(resources_dir, cert);
}

void ResourcesChecker::scanResources(const std::filesystem::path& resources_dir, Certificate& cert,
                                     const std::string& logical_prefix) const
{
    for (const auto& entry : std::filesystem::directory_iterator(resources_dir))
    {
        if (entry.is_regular_file())
        {
            const auto ext = file_utils::pathToUtf8(entry.path().extension());
            if (ext == ".fmu" || ext == ".ssp")
            {
                const ModelChecker nested_checker;
                // validate(..., true) will perform validation quietly and return a certificate
                // The ResourcesChecker inside that validation will handle further nesting
                Certificate initial_cert;
                if (cert.getContinueCallback())
                    initial_cert.setContinueCallback(cert.getContinueCallback());

                if (cert.shouldAbort())
                    continue;

                const Certificate nested_cert =
                    nested_checker.validate(entry.path(), true, false, std::move(initial_cert));

                NestedModelResult result;
                result.name = file_utils::pathToUtf8(entry.path().filename());
                result.logical_path = logical_prefix + result.name;
                result.report = nested_cert.getFullReport();
                result.extraction_path = nested_cert.getExtractionPath();
                result.status = nested_cert.getOverallStatus();
                result.summary = nested_cert.getSummary();
                result.results = nested_cert.getResults();
                result.nested_models = nested_cert.getNestedModels();

                std::function<void(NestedModelResult&, const std::string&)> fix_paths =
                    [&fix_paths](NestedModelResult& r, const std::string& prefix)
                {
                    std::string segment = r.name;
                    if (segment.ends_with('/'))
                        segment.pop_back();
                    r.logical_path = prefix + segment;
                    for (auto& child : r.nested_models)
                        fix_paths(child, r.logical_path + "/");
                };

                for (auto& child : result.nested_models)
                    fix_paths(child, result.logical_path + "/");

                cert.addNestedModelResult(result);
            }
        }
        else if (entry.is_directory())
        {
            // For directories, we might want to see if they are directory-based FMUs
            // But for now, let's just recurse to find FMUs in subfolders.
            // If it IS a directory-based FMU, validate() will handle it.

            // Check if it is a model (has modelDescription.xml or SystemStructure.ssd)
            if (std::filesystem::exists(entry.path() / "modelDescription.xml") ||
                std::filesystem::exists(entry.path() / "SystemStructure.ssd"))
            {
                const ModelChecker nested_checker;
                Certificate initial_cert;
                if (cert.getContinueCallback())
                    initial_cert.setContinueCallback(cert.getContinueCallback());

                if (cert.shouldAbort())
                    continue;

                const Certificate nested_cert =
                    nested_checker.validate(entry.path(), true, false, std::move(initial_cert));

                NestedModelResult result;
                result.name = file_utils::pathToUtf8(entry.path().filename()) + "/";
                result.logical_path = logical_prefix + file_utils::pathToUtf8(entry.path().filename());
                result.report = nested_cert.getFullReport();
                result.extraction_path = nested_cert.getExtractionPath();
                result.status = nested_cert.getOverallStatus();
                result.summary = nested_cert.getSummary();
                result.results = nested_cert.getResults();
                result.nested_models = nested_cert.getNestedModels();

                std::function<void(NestedModelResult&, const std::string&)> fix_paths =
                    [&fix_paths](NestedModelResult& r, const std::string& prefix)
                {
                    std::string segment = r.name;
                    if (segment.ends_with('/'))
                        segment.pop_back();
                    r.logical_path = prefix + segment;
                    for (auto& child : r.nested_models)
                        fix_paths(child, r.logical_path + "/");
                };

                for (auto& child : result.nested_models)
                    fix_paths(child, result.logical_path + "/");

                cert.addNestedModelResult(result);
            }
            else
            {
                // Just a regular directory, recurse into it
                const std::string sub_prefix = logical_prefix + file_utils::pathToUtf8(entry.path().filename()) + "/";
                scanResources(entry.path(), cert, sub_prefix);
            }
        }
    }
}
