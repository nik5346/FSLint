#include "model_checker.h"

#include "archive_checker.h"
#include "certificate.h"
#include "checker_factory.h"
#include "file_utils.h"
#include "model_info.h"
#include "zipper.h"

#include "picosha2.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

Certificate ModelChecker::validate(const std::filesystem::path& path, bool quiet, bool show_tree,
                                   Certificate cert) const
{
    cert.setQuiet(quiet);

    if (!quiet)
    {
        std::string filename = file_utils::pathToUtf8(path.filename());
        if (filename.empty() && path.has_parent_path())
            filename = file_utils::pathToUtf8(path.parent_path().filename());
        std::cout << "Validating: " << filename << "\n";
    }

    const std::string hash = calculateSHA256(path);
    cert.printMainHeader(hash);

    std::filesystem::path extract_dir;
    bool is_temporary = false;

    if (std::filesystem::is_directory(path))
    {
        extract_dir = path;
        cert.setExtractionPath(extract_dir);
    }
    else
    {
        // Step 1: Archive validation (version-agnostic)
        const ArchiveChecker archive_checker;
        archive_checker.validate(path, cert);

        if (cert.shouldAbort())
        {
            if (!quiet)
                cert.printFooter();
            return cert;
        }

        // Step 2: Extract to temporary directory
        static uint64_t temp_dir_counter = 0;
        const auto now = std::chrono::high_resolution_clock::now();
        const auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
        const std::string dir_name =
            "model_validation_" + std::to_string(nanos) + "_" + std::to_string(temp_dir_counter++);
#ifdef __EMSCRIPTEN__
        extract_dir = std::filesystem::current_path() / dir_name;
#else
        extract_dir = std::filesystem::temp_directory_path() / dir_name;
#endif
        is_temporary = true;

        Zipper zipper;
        if (!zipper.open(path))
        {
            cert.printSubsectionHeader("ARCHIVE EXTRACTION");
            cert.printTestResult({"Archive Open", TestStatus::FAIL, {"Failed to open ZIP file for extraction."}});
            cert.printSubsectionSummary(false);
            if (!quiet)
                cert.printFooter();
            return cert;
        }

        cert.setExtractionPath(extract_dir);

        if (!zipper.extractAll(extract_dir))
        {
            zipper.close();
            if (std::filesystem::exists(extract_dir))
                std::filesystem::remove_all(extract_dir);

            cert.printSubsectionHeader("ARCHIVE EXTRACTION");
            cert.printTestResult(
                {"Archive Extraction", TestStatus::FAIL, {"Failed to extract all files from the archive."}});
            cert.printSubsectionSummary(false);

            if (!quiet)
                cert.printFooter();
            return cert;
        }
    }

    // Step 3: Detect model type and create appropriate checkers
    ModelInfo model_info = CheckerFactory::detectModel(extract_dir, path);
    model_info.original_path = path;

    if (model_info.standard == ModelStandard::UNKNOWN)
    {
        cert.printSubsectionHeader("MODEL DETECTION");
        cert.printTestResult({"Model Type Detection",
                              TestStatus::FAIL,
                              {"Could not detect model standard. Missing 'modelDescription.xml' (for FMI) or "
                               "'SystemStructure.ssd' (for SSP)."}});
        cert.printSubsectionSummary(false);

        if (!quiet)
            cert.printFooter();

        if (is_temporary && std::filesystem::exists(extract_dir))
            std::filesystem::remove_all(extract_dir);

        return cert;
    }

    // Step 4: Run all appropriate checkers
    auto checkers = CheckerFactory::createCheckers(model_info);

    for (auto& checker : checkers)
    {
        if (cert.shouldAbort())
            break;
        checker->validate(extract_dir, cert);
    }

    if (show_tree)
        cert.printFileTree(extract_dir);

    cert.printNestedModelsTree();
    cert.printFooter();

    if (cert.isAddingCertificate() && !cert.isFailed())
    {
        const std::filesystem::path cert_file = extract_dir / CERT_RELATIVE_PATH;
        std::filesystem::create_directories(cert_file.parent_path());
        if (!cert.saveToFile(cert_file))
        {
            if (!quiet)
                std::cerr << "Error: Failed to save certificate to " << cert_file << "\n";
        }
        else if (is_temporary)
        {
            // Create backup for safety
            std::filesystem::path backup_path = path;
            backup_path += ".fslint_backup";
            std::error_code ec;
            std::filesystem::copy_file(path, backup_path, std::filesystem::copy_options::overwrite_existing, ec);

            if (!ec)
            {
                // Repackage with certificate
                if (!package(extract_dir, path))
                {
                    if (!quiet)
                        std::cerr << "Error: Failed to repackage model at " << path << "\n";
                    // Restore from backup
                    std::filesystem::copy_file(backup_path, path, std::filesystem::copy_options::overwrite_existing,
                                               ec);
                }
                std::filesystem::remove(backup_path, ec);
            }
            else
            {
                if (!quiet)
                    std::cerr << "Error: Failed to create backup before repackaging: " << ec.message() << "\n";
            }
        }
    }

    // Final cleanup for Emscripten when adding certificate
    if (is_temporary && cert.isAddingCertificate() && std::filesystem::exists(extract_dir))
        std::filesystem::remove_all(extract_dir);

    return cert;
}

bool ModelChecker::addCertificate(const std::filesystem::path& path, ContinueCallback callback) const
{
    Certificate initial_cert;
    initial_cert.setIsAddingCertificate(true);
    if (callback)
        initial_cert.setContinueCallback(std::move(callback));
    initial_cert.setQuiet(false);

    const Certificate result_cert = validate(path, false, false, std::move(initial_cert));

    if (result_cert.isFailed())
    {
        std::cerr << "Error: Validation failed. Certificate was not added.\n";
        return false;
    }

    if (std::filesystem::is_directory(path))
        std::cout << "\nCertificate added successfully to directory model\n";
    else
        std::cout << "\nCertificate added successfully to model\n";

    return true;
}

bool ModelChecker::updateCertificate(const std::filesystem::path& path, ContinueCallback callback) const
{
    std::cout << "Re-validating model and updating certificate...\n\n";

    if (std::filesystem::is_directory(path))
    {
        const auto result = removeCertificate(path);
        if (!result)
            return false;
        return addCertificate(path, std::move(callback));
    }

    // Remove existing certificate first
    const std::filesystem::path temp_dir =
        std::filesystem::temp_directory_path() / ("model_cert_update_" + std::to_string(std::time(nullptr)));
    if (!extract(path, temp_dir))
    {
        std::cerr << "Error: Failed to extract model\n";
        return false;
    }

    // Remove old certificate if it exists
    const std::filesystem::path old_cert = temp_dir / CERT_RELATIVE_PATH;
    if (std::filesystem::exists(old_cert))
        std::filesystem::remove(old_cert);

    // Repackage without certificate
    std::filesystem::path backup_path = path;
    backup_path += ".backup";
    std::filesystem::copy_file(path, backup_path, std::filesystem::copy_options::overwrite_existing);

    if (!package(temp_dir, path))
    {
        std::cerr << "Error: Failed to repackage model\n";
        std::filesystem::copy_file(backup_path, path, std::filesystem::copy_options::overwrite_existing);
        std::filesystem::remove(backup_path);
        std::filesystem::remove_all(temp_dir);
        return false;
    }

    std::filesystem::remove(backup_path);
    std::filesystem::remove_all(temp_dir);

    // Now validate and add new certificate
    return addCertificate(path, std::move(callback));
}

bool ModelChecker::removeCertificate(const std::filesystem::path& path) const
{
    if (std::filesystem::is_directory(path))
    {
        const std::filesystem::path cert_file = path / CERT_RELATIVE_PATH;

        if (std::filesystem::exists(cert_file))
        {
            std::filesystem::remove(cert_file);
            // Remove parent directories if empty
            auto parent = cert_file.parent_path();
            while (parent != path && std::filesystem::exists(parent) && std::filesystem::is_empty(parent))
            {
                std::filesystem::remove(parent);
                parent = parent.parent_path();
            }
            std::cout << "Validation certificate removed successfully from directory model\n";
        }
        else
        {
            std::cout << "No validation certificate found in directory model\n";
        }
        return true;
    }

    const std::filesystem::path temp_dir =
        std::filesystem::temp_directory_path() / ("model_cert_remove_" + std::to_string(std::time(nullptr)));

    if (!extract(path, temp_dir))
    {
        std::cerr << "Error: Failed to extract model for certificate removal\n";
        return false;
    }

    const std::filesystem::path cert_file = temp_dir / CERT_RELATIVE_PATH;
    bool had_certificate = false;

    if (std::filesystem::exists(cert_file))
    {
        had_certificate = true;
        std::filesystem::remove(cert_file);

        // Remove parent directories if empty
        auto parent = cert_file.parent_path();
        while (parent != temp_dir && std::filesystem::exists(parent) && std::filesystem::is_empty(parent))
        {
            std::filesystem::remove(parent);
            parent = parent.parent_path();
        }
    }

    if (!had_certificate)
    {
        std::cout << "No validation certificate found in model\n";
        std::filesystem::remove_all(temp_dir);
        return true;
    }

    std::filesystem::path backup_path = path;
    backup_path += ".backup";
    std::filesystem::copy_file(path, backup_path, std::filesystem::copy_options::overwrite_existing);

    if (!package(temp_dir, path))
    {
        std::cerr << "Error: Failed to repackage model\n";
        std::filesystem::copy_file(backup_path, path, std::filesystem::copy_options::overwrite_existing);
        std::filesystem::remove(backup_path);
        std::filesystem::remove_all(temp_dir);
        return false;
    }

    std::filesystem::remove(backup_path);
    std::filesystem::remove_all(temp_dir);

    std::cout << "Validation certificate removed successfully\n";
    return true;
}

bool ModelChecker::displayCertificate(const std::filesystem::path& path) const
{
    if (std::filesystem::is_directory(path))
    {
        const std::filesystem::path cert_file = path / CERT_RELATIVE_PATH;
        if (!std::filesystem::exists(cert_file))
        {
            std::cout << "No validation certificate found in directory model\n";
            return false;
        }

        const std::ifstream file(cert_file);
        if (!file)
        {
            std::cerr << "Error: Failed to open certificate file\n";
            return false;
        }
        std::stringstream ss;
        ss << file.rdbuf();
        std::cout << "\n" << ss.str() << "\n";
        return true;
    }

    Zipper zipper;

    if (!zipper.open(path))
    {
        std::cerr << "Error: Failed to open model file\n";
        return false;
    }

    std::vector<uint8_t> cert;
    if (!zipper.extractFile(std::string(CERT_RELATIVE_PATH), cert))
    {
        std::cout << "No validation certificate found in model\n";
        zipper.close();
        return false;
    }

    zipper.close();

    const std::string cert_content(cert.begin(), cert.end());
    std::cout << "\n" << cert_content << "\n";

    return true;
}

bool ModelChecker::verifyCertificate(const std::filesystem::path& path) const
{
    std::string cert_content;

    if (std::filesystem::is_directory(path))
    {
        const std::filesystem::path cert_file = path / CERT_RELATIVE_PATH;
        if (!std::filesystem::exists(cert_file))
        {
            std::cout << "No validation certificate found in directory model\n";
            return false;
        }

        const std::ifstream file(cert_file);
        if (!file)
        {
            std::cerr << "Error: Failed to open certificate file\n";
            return false;
        }
        std::stringstream ss;
        ss << file.rdbuf();
        cert_content = ss.str();
    }
    else
    {
        Zipper zipper;
        if (!zipper.open(path))
        {
            std::cerr << "Error: Failed to open model file\n";
            return false;
        }

        std::vector<uint8_t> cert_data;
        if (!zipper.extractFile(std::string(CERT_RELATIVE_PATH), cert_data))
        {
            std::cout << "No validation certificate found in model\n";
            zipper.close();
            return false;
        }
        zipper.close();
        cert_content.assign(cert_data.begin(), cert_data.end());
    }

    // Parse Tool version and SHA256
    std::string stored_version;
    std::string stored_hash;

    const std::regex tool_re(R"(Tool:\s+FSLint\s+(\S+))");
    const std::regex hash_re(R"(SHA256:\s+([0-9a-f]{64}))");
    std::smatch match;
    std::string line;
    std::istringstream iss(cert_content);
    while (std::getline(iss, line))
    {
        if (stored_version.empty() && std::regex_search(line, match, tool_re))
            stored_version = match[1].str();
        if (stored_hash.empty() && std::regex_search(line, match, hash_re))
            stored_hash = match[1].str();
        if (!stored_version.empty() && !stored_hash.empty())
            break;
    }

    if (stored_hash.empty())
    {
        std::cerr << "Error: Could not find SHA256 in certificate\n";
        return false;
    }

    std::cout << "Verifying certificate...\n";
    std::cout << "Stored Tool Version: " << stored_version << "\n";
    std::cout << "Stored SHA256:       " << stored_hash << "\n";

    const std::string current_hash = calculateSHA256(path);
    std::cout << "Current SHA256:      " << current_hash << "\n\n";

    bool valid = true;
    if (current_hash != stored_hash)
    {
        std::cout << "✗ Verification FAILED: Hash mismatch. The model has been modified since the certificate was "
                     "created.\n";
        valid = false;
    }
    else
    {
        std::cout << "✓ Verification PASSED: Hash matches.\n";
    }

    if (isVersionDeprecated(stored_version))
    {
        std::cout << "⚠ Tool version " << stored_version
                  << " is deprecated. It is recommended to re-validate with the latest version.\n";
    }
    else
    {
        std::cout << "✓ Tool version " << stored_version << " is still supported.\n";
    }

    return valid;
}

bool ModelChecker::isVersionDeprecated(const std::string& version) const
{
    // Initially, no versions are deprecated.
    // This can be expanded in the future.
    (void)version;
    return false;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
bool ModelChecker::extract(const std::filesystem::path& model_path, const std::filesystem::path& extract_dir) const
{
    Zipper zipper;
    if (!zipper.open(model_path))
        return false;

    const bool success = zipper.extractAll(extract_dir);
    zipper.close();
    return success;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
bool ModelChecker::package(const std::filesystem::path& extract_dir, const std::filesystem::path& model_path) const
{
    // Delete existing file if it exists
    if (std::filesystem::exists(model_path))
        std::filesystem::remove(model_path);

    // Create a new ZIP file from directory contents
    Zipper zip_handler;

    if (!zip_handler.create(model_path))
    {
        std::cerr << "Error: Failed to create output model file\n";
        return false;
    }

    // Recursively add all files from extract_dir
    try
    {
        const std::filesystem::path absolute_model_path = std::filesystem::absolute(model_path);

        for (const auto& entry : std::filesystem::recursive_directory_iterator(extract_dir))
        {
            if (entry.is_regular_file())
            {
                // Skip the output file if it's inside the extract_dir to avoid recursive inclusion
                if (std::filesystem::absolute(entry.path()) == absolute_model_path)
                    continue;

                const std::filesystem::path rel_path = std::filesystem::relative(entry.path(), extract_dir);

                std::string internal_path = file_utils::pathToUtf8(rel_path);

                // Convert backslashes to forward slashes for ZIP compatibility
                std::ranges::replace(internal_path, '\\', '/');

                if (!zip_handler.addFileFromDisk(internal_path, entry.path()))
                {
                    std::cerr << "Error: Failed to add file: " << internal_path << "\n";
                    zip_handler.close();
                    return false;
                }
            }
        }
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        std::cerr << "Filesystem error while packaging: " << e.what() << "\n";
        zip_handler.close();
        return false;
    }
    catch (const std::runtime_error& e)
    {
        std::cerr << "Runtime error while packaging: " << e.what() << "\n";
        zip_handler.close();
        return false;
    }

    zip_handler.close();
    return true;
}

std::string ModelChecker::calculateSHA256(const std::filesystem::path& path) const
{
    picosha2::hash256_one_by_one hasher;

    if (std::filesystem::is_directory(path))
    {
        std::vector<std::filesystem::path> files;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path))
        {
            if (entry.is_regular_file())
            {
                auto rel_path = std::filesystem::relative(entry.path(), path);
                std::string rel_path_str = file_utils::pathToUtf8(rel_path);
                std::ranges::replace(rel_path_str, '\\', '/');

                if (rel_path_str == CERT_RELATIVE_PATH || rel_path_str.ends_with("/" + std::string(CERT_RELATIVE_PATH)))
                    continue;

                files.push_back(rel_path);
            }
        }
        std::ranges::sort(files);

        for (const auto& rel_path : files)
        {
            std::string rel_path_str = file_utils::pathToUtf8(rel_path);
            std::ranges::replace(rel_path_str, '\\', '/');

            // Hash path to include structure in hash
            hasher.process(rel_path_str.begin(), rel_path_str.end());

            const auto full_path = path / rel_path;
            const auto ext = rel_path.extension().string();

            if (ext == ".fmu" || ext == ".ssp")
            {
                // Recursive hash for nested models to be invariant to their certificates
                const std::string nested_hash = calculateSHA256(full_path);
                hasher.process(nested_hash.begin(), nested_hash.end());
            }
            else
            {
                // Hash content
                std::ifstream f(full_path, std::ios::binary);
                if (f)
                {
                    std::vector<char> buffer(4096);
                    while (f.read(buffer.data(), static_cast<std::streamsize>(buffer.size())))
                        hasher.process(buffer.begin(), buffer.end());
                    if (f.gcount() > 0)
                        hasher.process(buffer.begin(), buffer.begin() + f.gcount());
                }
            }
        }
    }
    else
    {
        Zipper zipper;
        if (zipper.open(path))
        {
            auto entries = zipper.getEntries();
            std::vector<std::pair<std::string, std::string>> name_pairs;
            for (const auto& entry : entries)
            {
                // Skip directories and any certificate file
                if (entry.filename.empty() || entry.filename.back() == '/' || entry.filename == CERT_RELATIVE_PATH ||
                    entry.filename.ends_with("/" + std::string(CERT_RELATIVE_PATH)))
                    continue;
                name_pairs.emplace_back(entry.filename, entry.raw_filename);
            }
            // Sort by normalized filename for consistent hashing
            std::ranges::sort(name_pairs, [](const auto& a, const auto& b) { return a.first < b.first; });

            for (const auto& pair : name_pairs)
            {
                const auto& normalized_name = pair.first;
                const auto& raw_name = pair.second;

                // Hash filename (use normalized name for consistency)
                hasher.process(normalized_name.begin(), normalized_name.end());

                if (normalized_name.ends_with(".fmu") || normalized_name.ends_with(".ssp"))
                {
                    // Recursive hash for nested models inside archive
                    std::vector<uint8_t> data;
                    if (zipper.extractFile(raw_name, data))
                    {
                        static std::atomic<uint32_t> temp_counter{0};
                        const std::string temp_name = "temp_nested_hash_" + std::to_string(temp_counter++) + "_" +
                                                      std::to_string(std::hash<std::string>{}(normalized_name)) +
                                                      (normalized_name.ends_with(".fmu") ? ".fmu" : ".ssp");
#ifdef __EMSCRIPTEN__
                        const std::filesystem::path temp_path = std::filesystem::current_path() / temp_name;
#else
                        const std::filesystem::path temp_path = std::filesystem::temp_directory_path() / temp_name;
#endif
                        {
                            std::ofstream f(temp_path, std::ios::binary);
                            std::copy(data.begin(), data.end(), std::ostreambuf_iterator<char>(f));
                        }
                        const std::string nested_hash = calculateSHA256(temp_path);
                        std::filesystem::remove(temp_path);
                        hasher.process(nested_hash.begin(), nested_hash.end());
                    }
                }
                else
                {
                    // Hash content
                    std::vector<uint8_t> data;
                    if (zipper.extractFile(raw_name, data))
                        hasher.process(data.begin(), data.end());
                }
            }
            zipper.close();
        }
        else
        {
            // Plain file
            std::ifstream file(path, std::ios::binary);
            if (file)
            {
                std::vector<char> buffer(4096);
                while (file.read(buffer.data(), static_cast<std::streamsize>(buffer.size())))
                    hasher.process(buffer.begin(), buffer.end());
                if (file.gcount() > 0)
                    hasher.process(buffer.begin(), buffer.begin() + file.gcount());
            }
            else
            {
                std::cerr << "Error: Failed to open file for hashing: " << file_utils::pathToUtf8(path) << "\n";
                return "";
            }
        }
    }

    hasher.finish();
    std::vector<unsigned char> hash(picosha2::k_digest_size);
    hasher.get_hash_bytes(hash.begin(), hash.end());

    return picosha2::bytes_to_hex_string(hash.begin(), hash.end());
}
