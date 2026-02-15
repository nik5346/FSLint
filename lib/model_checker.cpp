#include "model_checker.h"

#include "archive_checker.h"
#include "checker_factory.h"
#include "zipper.h"

#include "picosha2.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>

void ModelChecker::validate(const std::filesystem::path& path) const
{
    Certificate cert;

    // Print header
    std::string hash = calculateSHA256(path);
    cert.printMainHeader(path.string(), hash);

    validateInternal(path, cert);

    cert.printNestedModelsTree();
    cert.printFooter();
}

Certificate ModelChecker::validateCore(const std::filesystem::path& path) const
{
    Certificate cert;
    cert.setQuiet(true);
    validateInternal(path, cert);
    return cert;
}

void ModelChecker::validateInternal(const std::filesystem::path& path, Certificate& cert) const
{
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

        // Step 2: Extract to temporary directory
        auto now = std::chrono::high_resolution_clock::now();
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
        extract_dir = std::filesystem::temp_directory_path() / ("model_validation_" + std::to_string(nanos));
        is_temporary = true;

        Zipper zipper;
        if (!zipper.open(path))
            return;

        if (!zipper.extractAll(extract_dir))
        {
            zipper.close();
            if (std::filesystem::exists(extract_dir))
                std::filesystem::remove_all(extract_dir);
            return;
        }
        zipper.close();
    }

    // Step 3: Detect model type and create appropriate checkers
    ModelInfo model_info = CheckerFactory::detectModel(extract_dir);
    model_info.original_path = path;

    if (model_info.standard == ModelStandard::UNKNOWN)
    {
        if (is_temporary && std::filesystem::exists(extract_dir))
            std::filesystem::remove_all(extract_dir);
        return;
    }

    // Step 4: Run all appropriate checkers
    auto checkers = CheckerFactory::createCheckers(model_info);

    for (auto& checker : checkers)
        checker->validate(extract_dir, cert);

    // Cleanup temporary directory
    if (is_temporary && std::filesystem::exists(extract_dir))
        std::filesystem::remove_all(extract_dir);
}

bool ModelChecker::addCertificate(const std::filesystem::path& path) const
{
    Certificate cert;

    // Print header
    std::string hash = calculateSHA256(path);
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
        extract_dir = std::filesystem::temp_directory_path() / ("model_cert_add_" + std::to_string(std::time(nullptr)));
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
    std::filesystem::path extra_dir = extract_dir / "extra";
    std::filesystem::create_directories(extra_dir);

    // Write certificate to file
    std::filesystem::path cert_file = extra_dir / "validation_certificate.txt";
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
    std::filesystem::path temp_dir =
        std::filesystem::temp_directory_path() / ("model_cert_update_" + std::to_string(std::time(nullptr)));
    if (!extract(path, temp_dir))
    {
        std::cerr << "Error: Failed to extract model\n";
        return false;
    }

    // Remove old certificate if it exists
    std::filesystem::path extra_dir = temp_dir / "extra";
    std::filesystem::path old_cert = extra_dir / "validation_certificate.txt";
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
        std::filesystem::path extra_dir = path / "extra";
        std::filesystem::path cert_file = extra_dir / "validation_certificate.txt";

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

    std::filesystem::path temp_dir =
        std::filesystem::temp_directory_path() / ("model_cert_remove_" + std::to_string(std::time(nullptr)));

    if (!extract(path, temp_dir))
    {
        std::cerr << "Error: Failed to extract model for certificate removal\n";
        return false;
    }

    std::filesystem::path extra_dir = temp_dir / "extra";
    std::filesystem::path cert_file = extra_dir / "validation_certificate.txt";
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
        std::filesystem::path cert_file = path / "extra" / "validation_certificate.txt";
        if (!std::filesystem::exists(cert_file))
        {
            std::cout << "No validation certificate found in directory model\n";
            return false;
        }

        std::ifstream file(cert_file);
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

    std::string cert_content(cert.begin(), cert.end());
    std::cout << "\n" << cert_content << "\n";

    return true;
}

bool ModelChecker::extract(const std::filesystem::path& model_path, const std::filesystem::path& extract_dir) const
{
    Zipper zipper;
    if (!zipper.open(model_path))
        return false;

    bool success = zipper.extractAll(extract_dir);
    zipper.close();
    return success;
}

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
                std::filesystem::path rel_path = std::filesystem::relative(entry.path(), extract_dir);
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
    catch (const std::exception& e)
    {
        std::cerr << "Error while packaging: " << e.what() << "\n";
        zip_handler.close();
        return false;
    }

    zip_handler.close();
    return true;
}

std::string ModelChecker::calculateSHA256(const std::filesystem::path& path) const
{
    if (std::filesystem::is_directory(path))
    {
        if (std::filesystem::exists(path / "modelDescription.xml"))
            return calculateSHA256(path / "modelDescription.xml");
        if (std::filesystem::exists(path / "SystemStructure.ssd"))
            return calculateSHA256(path / "SystemStructure.ssd");
        return "N/A (Directory)";
    }

    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        std::cerr << "Error: Failed to open file for hashing: " << path << "\n";
        return "";
    }

    std::vector<unsigned char> hash(picosha2::k_digest_size);
    picosha2::hash256(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>(), hash.begin(), hash.end());

    return picosha2::bytes_to_hex_string(hash.begin(), hash.end());
}