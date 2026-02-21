#include "model_checker.h"

#include <cstddef>
#include <exception>
#include <filesystem>
#include <iostream>
#include <span>
#include <string>
#include <string_view>

void printUsage(const std::string& program_name)
{
    std::cout << "Usage: " << program_name << " [OPTIONS] <fmu/ssp-file>\n\n";
    std::cout << "Options:\n";
    std::cout << "  -s, --save              Validate and add certificate to FMU/SSP\n";
    std::cout << "  -u, --update            Re-validate and update certificate in FMU/SSP\n";
    std::cout << "  -r, --remove            Remove certificate from FMU/SSP\n";
    std::cout << "  -d, --display           Display certificate information from FMU/SSP\n";
    std::cout << "  -c, --verify            Verify the embedded certificate in FMU/SSP\n";
    std::cout << "  -h, --help              Show this help message\n";
    std::cout << "  -v, --version           Show version information\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " model.fmu/ssp      # Validate FMU/SSP\n";
    std::cout << "  " << program_name << " -s model.fmu/ssp   # Validate and add certificate\n";
    std::cout << "  " << program_name << " -d model.fmu/ssp   # Display certificate\n";
    std::cout << "  " << program_name << " -r model.fmu/ssp   # Remove certificate\n";
    std::cout << "  " << program_name << " -u model.fmu/ssp   # Re-validate and update certificate\n";
}

int main(int argc, char** argv)
{
    const std::span<char*> args(argv, argc);

    if (args.size() < 2)
    {
        printUsage(args[0]);
        return 1;
    }

    std::filesystem::path fmu_path;
    bool save_cert = false;
    bool update_cert = false;
    bool remove_cert = false;
    bool display_cert = false;
    bool verify_cert = false;

    // Parse arguments
    for (size_t i = 1; i < args.size(); ++i)
    {
        const std::string_view arg = args[i];

        if (arg == "-h" || arg == "--help")
        {
            printUsage(args[0]);
            return 0;
        }
        else if (arg == "-v" || arg == "--version")
        {
            std::cout << "FSLint " << PROJECT_VERSION << "\n";
            return 0;
        }
        else if (arg == "-s" || arg == "--save")
            save_cert = true;
        else if (arg == "-u" || arg == "--update")
            update_cert = true;
        else if (arg == "-r" || arg == "--remove")
            remove_cert = true;
        else if (arg == "-d" || arg == "--display")
            display_cert = true;
        else if (arg == "-c" || arg == "--verify")
            verify_cert = true;
        else if (arg[0] != '-')
        {
            if (fmu_path.empty())
            {
                fmu_path = arg;
                fmu_path = fmu_path.make_preferred();
            }
            else
            {
                std::cerr << "Error: Multiple FMU files specified\n";
                return 1;
            }
        }
        else
        {
            std::cerr << "Unknown option: " << arg << "\n";
            return 1;
        }
    }

    if (fmu_path.empty())
    {
        std::cerr << "Error: No FMU file specified\n";
        printUsage(args[0]);
        return 1;
    }

    if (!std::filesystem::exists(fmu_path))
    {
        std::cerr << "Error: File not found: " << fmu_path << "\n";
        return 1;
    }

    // Validate mutual exclusivity of operations
    const size_t operation_count = (save_cert ? 1 : 0) + (update_cert ? 1 : 0) + (remove_cert ? 1 : 0) +
                                   (display_cert ? 1 : 0) + (verify_cert ? 1 : 0);

    if (operation_count > 1)
    {
        std::cerr << "Error: Cannot combine multiple certificate operations\n";
        return 1;
    }

    // Create validator instance
    const ModelChecker validator;

    // Execute requested operation
    try
    {
        if (remove_cert)
            return validator.removeCertificate(fmu_path) ? 0 : 1;
        else if (display_cert)
            return validator.displayCertificate(fmu_path) ? 0 : 1;
        else if (verify_cert)
            return validator.verifyCertificate(fmu_path) ? 0 : 1;
        else if (save_cert)
            return validator.addCertificate(fmu_path) ? 0 : 1;
        else if (update_cert)
            return validator.updateCertificate(fmu_path) ? 0 : 1;

        // Default: validate FMU (without saving certificate)
        validator.validate(fmu_path);

        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}