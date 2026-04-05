#include "model_checker.h"

#include <cstddef>
#include <exception>
#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <io.h>
#define ISATTY _isatty
#define FILENO _fileno
#else
#include <cstdio>
#include <unistd.h>
#define ISATTY isatty
#define FILENO fileno
#endif

void printUsage(const std::string& program_name)
{
    std::cout << "Usage: " << program_name << " [OPTIONS] <fmu/ssp-file>\n\n";
    std::cout << "Options:\n";
    std::cout << "  -s, --save              Validate and add certificate to FMU/SSP\n";
    std::cout << "  -u, --update            Re-validate and update certificate in FMU/SSP\n";
    std::cout << "  -r, --remove            Remove certificate from FMU/SSP\n";
    std::cout << "  -d, --display           Display certificate information from FMU/SSP\n";
    std::cout << "  -c, --verify            Verify the embedded certificate in FMU/SSP\n";
    std::cout << "  -t, --tree              Show internal file tree of FMU/SSP\n";
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

// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, char** argv)
{
    try
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
        bool show_tree = false;

        // Parse arguments
        for (size_t i = 1; i < args.size(); ++i)
        {
            const std::string_view arg = args[i];

            if (arg.empty())
                continue;

            if (arg == "-h" || arg == "--help")
            {
                printUsage(args[0]);
                return 0;
            }

            if (arg == "-v" || arg == "--version")
            {
                // __DATE__ is in "Mmm dd yyyy" format (e.g., "Jan 26 2010")
                const std::string build_date = __DATE__;
                const std::string build_year = build_date.substr(build_date.size() - 4);

                std::cout << "FSLint " << PROJECT_VERSION << "\n";
                std::cout << "Copyright (c) " << build_year << " FSLint Contributors\n";
                return 0;
            }

            if (arg == "-s" || arg == "--save")
            {
                save_cert = true;
            }
            else if (arg == "-u" || arg == "--update")
            {
                update_cert = true;
            }
            else if (arg == "-r" || arg == "--remove")
            {
                remove_cert = true;
            }
            else if (arg == "-d" || arg == "--display")
            {
                display_cert = true;
            }
            else if (arg == "-c" || arg == "--verify")
            {
                verify_cert = true;
            }
            else if (arg == "-t" || arg == "--tree")
            {
                show_tree = true;
            }
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

        const auto continue_callback = [](const TestResult& test) -> bool
        {
            (void)test;
            if (ISATTY(FILENO(stdin)) == 0)
                return false;

            std::cout << "\n[SECURITY ISSUE DETECTED] Do you want to continue validation? (y/N): ";
            char response = 'n';
            if (!(std::cin >> response))
                return false;

            return response == 'y' || response == 'Y';
        };

        // Execute requested operation
        if (remove_cert)
            return validator.removeCertificate(fmu_path) ? 0 : 1;

        if (display_cert)
            return validator.displayCertificate(fmu_path) ? 0 : 1;

        if (verify_cert)
            return validator.verifyCertificate(fmu_path) ? 0 : 1;

        if (save_cert)
            return validator.addCertificate(fmu_path) ? 0 : 1;

        if (update_cert)
            return validator.updateCertificate(fmu_path) ? 0 : 1;

        // Default: validate FMU (without saving certificate)
        Certificate initial_cert;
        initial_cert.setContinueCallback(continue_callback);
        const Certificate cert = validator.validate(fmu_path, false, show_tree, std::move(initial_cert));

        return cert.isFailed() ? 1 : 0;
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        std::cerr << "Filesystem error: " << e.what() << "\n";
        return 1;
    }
    catch (const std::runtime_error& e)
    {
        std::cerr << "Runtime error: " << e.what() << "\n";
        return 1;
    }
    catch (const std::logic_error& e)
    {
        std::cerr << "Logic error: " << e.what() << "\n";
        return 1;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Unexpected error: " << e.what() << "\n";
        return 1;
    }
    catch (...)
    {
        std::cerr << "An unknown fatal error occurred.\n";
        return 1;
    }
}
