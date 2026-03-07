#include "model_checker.h"

#include "archive_checker.h"
#include "certificate.h"
#include "checker_factory.h"
#include "model_info.h"
#include "zipper.h"

#include "picosha2.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

Certificate ModelChecker::validate(const std::filesystem::path& path, bool quiet) const
{
    Certificate cert;
    cert.setQuiet(quiet);

    if (!quiet)
    {
        const std::string hash = calculateSHA256(path);
        cert.printMainHeader(path.string(), hash);
    }

    std::filesystem::path extract_dir;
    bool is_temporary = false;

    if (std::filesystem::is_directory(path))
    {
        extract_dir = path;
    }
    else
    {
        // Step 1: Archive validation (version-agnostic)
        ArchiveChecker archive_checker;
        archive_checker.validate(path, cert);

        if (cert.isFailed())
        {
            if (!quiet)
                cert.printFooter();
            return cert;
        }

        // Step 2: Extract to temporary directory
        auto now = std::chrono::high_resolution_clock::now();
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
#ifdef __EMSCRIPTEN__
        extract_dir = "/tmp/model_validation_" + std::to_string(nanos);
#else
        extract_dir = std::filesystem::temp_directory_path() / ("model_validation_" + std::to_string(nanos));
#endif
        is_temporary = true;

        Zipper zipper;
        if (!zipper.open(path))
        {
            if (!quiet)
                cert.printFooter();
            return cert;
        }

        if (!zipper.extractAll(extract_dir))
        {
            zipper.close();
            if (std::filesystem::exists(extract_dir))
                std::filesystem::remove_all(extract_dir);
            if (!quiet)
                cert.printFooter();
            return cert;
        }
    }

    // Step 3: Detect model type and create appropriate checkers
    ModelInfo model_info = CheckerFactory::detectModel(extract_dir);
    model_info.original_path = path;

    if (model_info.standard == ModelStandard::UNKNOWN)
    {
        if (is_temporary && std::filesystem::exists(extract_dir))
            std::filesystem::remove_all(extract_dir);

        if (!quiet)
            cert.printFooter();
        return cert;
    }

    // Step 4: Run all appropriate checkers
    auto checkers = CheckerFactory::createCheckers(model_info);

    for (auto& checker : checkers)
        checker->validate(extract_dir, cert);

    // Cleanup temporary directory
    if (is_temporary && std::filesystem::exists(extract_dir))
        std::filesystem::remove_all(extract_dir);

    if (!quiet)
    {
        cert.printNestedModelsTree();
        cert.printFooter();
    }

    return cert;
}

bool ModelChecker::addCertificate(const std::filesystem::path& path) const
{
    Certificate cert;

    // Print header
    const std::string hash = calculateSHA256(path);
    cert.printMainHeader(path.string(), hash);

    std::filesystem::path extract_dir;
    bool is_temporary = false;

    if (std::filesystem::is_directory(path))
    {
        extract_dir = path;
    }
    else
    {
        // Step 1: Archive validation
        ArchiveChecker archive_checker;
        archive_checker.validate(path, cert);

        // Step 2: Extract to temporary directory
#ifdef __EMSCRIPTEN__
        extract_dir = "/tmp/model_cert_add_" + std::to_string(std::time(nullptr));
#else
        extract_dir = std::filesystem::temp_directory_path() / ("model_cert_add_" + std::to_string(std::time(nullptr)));
#endif
        is_temporary = true;

        Zipper zipper;
        if (!zipper.open(path))
        {
            cert.printFooter();
            return false;
        }

        if (!zipper.extractAll(extract_dir))
        {
            zipper.close();
            cert.printFooter();
            if (std::filesystem::exists(extract_dir))
                std::filesystem::remove_all(extract_dir);
            return false;
        }
        zipper.close();
    }

    // Step 3: Detect and validate
    ModelInfo model_info = CheckerFactory::detectModel(extract_dir);
    model_info.original_path = path;

    if (model_info.standard == ModelStandard::UNKNOWN)
    {
        std::cerr << "Error: Could not detect model standard\n";
        cert.printFooter();
        if (is_temporary && std::filesystem::exists(extract_dir))
            std::filesystem::remove_all(extract_dir);
        return false;
    }

    auto checkers = CheckerFactory::createCheckers(model_info);
    for (auto& checker : checkers)
        checker->validate(extract_dir, cert);

    cert.printFooter();

    // Create extra directory if it doesn't exist
    const std::filesystem::path extra_dir = extract_dir / "extra";
    std::filesystem::create_directories(extra_dir);

    // Write certificate to file
    const std::filesystem::path cert_file = extra_dir / "validation_certificate.txt";
    if (!cert.saveToFile(cert_file))
    {
        std::cerr << "Error: Failed to create certificate file\n";
        if (is_temporary && std::filesystem::exists(extract_dir))
            std::filesystem::remove_all(extract_dir);
        return false;
    }

    if (!is_temporary)
    {
        std::cout << "\nCertificate added successfully to directory model\n";
        return true;
    }

    // Create backup
    std::filesystem::path backup_path = path;
    backup_path += ".backup";
    std::filesystem::copy_file(path, backup_path, std::filesystem::copy_options::overwrite_existing);

    // Repackage with certificate
    if (!package(extract_dir, path))
    {
        std::cerr << "Error: Failed to repackage model with certificate\n";
        std::filesystem::copy_file(backup_path, path, std::filesystem::copy_options::overwrite_existing);
        std::filesystem::remove(backup_path);
        if (std::filesystem::exists(extract_dir))
            std::filesystem::remove_all(extract_dir);
        return false;
    }

    // Cleanup
    std::filesystem::remove(backup_path);
    if (std::filesystem::exists(extract_dir))
        std::filesystem::remove_all(extract_dir);

    std::cout << "\nCertificate added successfully to model\n";
    return true;
}

bool ModelChecker::updateCertificate(const std::filesystem::path& path) const
{
    std::cout << "Re-validating model and updating certificate...\n\n";

    if (std::filesystem::is_directory(path))
    {
        removeCertificate(path);
        return addCertificate(path);
    }

    // Remove existing certificate first
#ifdef __EMSCRIPTEN__
    const std::filesystem::path temp_dir = "/tmp/model_cert_update_" + std::to_string(std::time(nullptr));
#else
    const std::filesystem::path temp_dir =
        std::filesystem::temp_directory_path() / ("model_cert_update_" + std::to_string(std::time(nullptr)));
#endif
    if (!extract(path, temp_dir))
    {
        std::cerr << "Error: Failed to extract model\n";
        return false;
    }

    // Remove old certificate if it exists
    const std::filesystem::path extra_dir = temp_dir / "extra";
    const std::filesystem::path old_cert = extra_dir / "validation_certificate.txt";
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
    return addCertificate(path);
}

bool ModelChecker::removeCertificate(const std::filesystem::path& path) const
{
    if (std::filesystem::is_directory(path))
    {
        const std::filesystem::path extra_dir = path / "extra";
        const std::filesystem::path cert_file = extra_dir / "validation_certificate.txt";

        if (std::filesystem::exists(cert_file))
        {
            std::filesystem::remove(cert_file);
            // Remove extra directory if empty
            if (std::filesystem::exists(extra_dir) && std::filesystem::is_empty(extra_dir))
                std::filesystem::remove(extra_dir);
            std::cout << "Validation certificate removed successfully from directory model\n";
        }
        else
        {
            std::cout << "No validation certificate found in directory model\n";
        }
        return true;
    }

#ifdef __EMSCRIPTEN__
    const std::filesystem::path temp_dir = "/tmp/model_cert_remove_" + std::to_string(std::time(nullptr));
#else
    const std::filesystem::path temp_dir =
        std::filesystem::temp_directory_path() / ("model_cert_remove_" + std::to_string(std::time(nullptr)));
#endif

    if (!extract(path, temp_dir))
    {
        std::cerr << "Error: Failed to extract model for certificate removal\n";
        return false;
    }

    const std::filesystem::path extra_dir = temp_dir / "extra";
    const std::filesystem::path cert_file = extra_dir / "validation_certificate.txt";
    bool had_certificate = false;

    if (std::filesystem::exists(cert_file))
    {
        had_certificate = true;
        std::filesystem::remove(cert_file);

        // Remove extra directory if empty
        if (std::filesystem::is_empty(extra_dir))
            std::filesystem::remove(extra_dir);
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
        const std::filesystem::path cert_file = path / "extra" / "validation_certificate.txt";
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
    if (!zipper.extractFile("extra/validation_certificate.txt", cert))
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
        const std::filesystem::path cert_file = path / "extra" / "validation_certificate.txt";
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
        if (!zipper.extractFile("extra/validation_certificate.txt", cert_data))
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

    std::istringstream iss(cert_content);
    std::string line;
    while (std::getline(iss, line))
    {
        if (line.find("Tool:") != std::string::npos)
        {
            const size_t pos = line.find("FSLint");
            if (pos != std::string::npos && line.length() > pos + 7)
            {
                stored_version = line.substr(pos + 7);
                // Trim leading/trailing spaces
                const size_t first = stored_version.find_first_not_of(' ');
                if (std::string::npos != first)
                {
                    const size_t last = stored_version.find_last_not_of(' ');
                    stored_version = stored_version.substr(first, (last - first + 1));
                }
            }
        }
        else if (line.find("SHA256:") != std::string::npos)
        {
            const size_t pos = line.find("SHA256:");
            stored_hash = line.substr(pos + 7);
            // Trim leading/trailing spaces
            const size_t first = stored_hash.find_first_not_of(' ');
            if (std::string::npos != first)
            {
                const size_t last = stored_hash.find_last_not_of(' ');
                stored_hash = stored_hash.substr(first, (last - first + 1));
            }
        }
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
        for (const auto& entry : std::filesystem::recursive_directory_iterator(extract_dir))
        {
            if (entry.is_regular_file())
            {
                const std::filesystem::path rel_path = std::filesystem::relative(entry.path(), extract_dir);
                std::string internal_path = rel_path.string();

                // Convert backslashes to forward slashes for ZIP compatibility
                std::replace(internal_path.begin(), internal_path.end(), '\\', '/');

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
                std::string rel_path_str = rel_path.string();
                std::replace(rel_path_str.begin(), rel_path_str.end(), '\\', '/');

                if (rel_path_str == "extra/validation_certificate.txt")
                    continue;

                files.push_back(rel_path);
            }
        }
        std::sort(files.begin(), files.end());

        for (const auto& rel_path : files)
        {
            std::string rel_path_str = rel_path.string();
            std::replace(rel_path_str.begin(), rel_path_str.end(), '\\', '/');

            // Hash path to include structure in hash
            hasher.process(rel_path_str.begin(), rel_path_str.end());

            // Hash content
            std::ifstream f(path / rel_path, std::ios::binary);
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
    else
    {
        Zipper zipper;
        if (zipper.open(path))
        {
            auto entries = zipper.getEntries();
            std::vector<std::string> names;
            for (const auto& entry : entries)
            {
                // Skip directories and the certificate itself
                if (entry.filename.empty() || entry.filename.back() == '/' ||
                    entry.filename == "extra/validation_certificate.txt")
                    continue;
                names.push_back(entry.filename);
            }
            std::sort(names.begin(), names.end());

            for (const auto& name : names)
            {
                // Hash filename
                hasher.process(name.begin(), name.end());

                // Hash content
                std::vector<uint8_t> data;
                if (zipper.extractFile(name, data))
                    hasher.process(data.begin(), data.end());
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
                std::cerr << "Error: Failed to open file for hashing: " << path << "\n";
                return "";
            }
        }
    }

    hasher.finish();
    std::vector<unsigned char> hash(picosha2::k_digest_size);
    hasher.get_hash_bytes(hash.begin(), hash.end());

    return picosha2::bytes_to_hex_string(hash.begin(), hash.end());
}
