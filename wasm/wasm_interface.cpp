#include "file_utils.h"
#include "model_checker.h"

#include <emscripten.h>
#include <format>
#include <string>

extern "C"
{
    /// @brief Entry point for WASM-based validation.
    /// @param path Path to the model file in the Emscripten filesystem.
    /// @return JSON string containing validation results.
    EMSCRIPTEN_KEEPALIVE const char* run_validation(const char* const path)
    {
        const ModelChecker validator;
        Certificate initial_cert;

        initial_cert.setContinueCallback(
            [](const TestResult& test) -> bool
            {
                // Escape single quotes for JS string
                std::string escaped_name = test.getName();
                size_t pos = 0;
                while ((pos = escaped_name.find('\'', pos)) != std::string::npos)
                {
                    escaped_name.replace(pos, 1, "\\'");
                    pos += 2;
                }

                // We use EM_ASM_INT to call window.confirm in the browser.
                // We pass the test name to the confirm dialog.
                const std::string script =
                    std::format("window.confirm('SECURITY ISSUE DETECTED: {}\\n\\nDo you want to continue "
                                "validation?') ? 1 : 0",
                                escaped_name);
                return emscripten_run_script_int(script.c_str()) != 0;
            });

        const Certificate cert = validator.validate(path, false, false, std::move(initial_cert));

        static std::string result;
        result = cert.toJson(path);
        return result.c_str();
    }

    /// @brief Adds a certificate to a model.
    /// @param path Path to the model.
    /// @return True if successful.
    EMSCRIPTEN_KEEPALIVE bool add_certificate(const char* const path)
    {
        try
        {
            const ModelChecker validator;
            return validator.addCertificate(path, [](const TestResult& test) -> bool {
                // Escape single quotes for JS string
                std::string escaped_name = test.getName();
                size_t pos = 0;
                while ((pos = escaped_name.find('\'', pos)) != std::string::npos)
                {
                    escaped_name.replace(pos, 1, "\\'");
                    pos += 2;
                }

                const std::string script =
                    std::format("window.confirm('SECURITY ISSUE DETECTED: {}\\n\\nDo you want to continue "
                                "adding certificate?') ? 1 : 0",
                                escaped_name);
                return emscripten_run_script_int(script.c_str()) != 0;
            });
        }
        catch (const std::exception& e)
        {
            const std::string error_msg = std::format("Error in add_certificate: {}\n", e.what());
            emscripten_run_script(std::format("console.error('{}')", error_msg).c_str());
            return false;
        }
    }

    /// @brief Packages a model from an extraction directory into an archive.
    /// @param extract_dir Extraction directory.
    /// @param model_path Target archive path.
    /// @return True if successful.
    EMSCRIPTEN_KEEPALIVE bool package_model(const char* const extract_dir, const char* const model_path)
    {
        try
        {
            const ModelChecker validator;
            return validator.package(extract_dir, model_path);
        }
        catch (const std::exception& e)
        {
            const std::string error_msg = std::format("Error in package_model: {}\n", e.what());
            emscripten_run_script(std::format("console.error('{}')", error_msg).c_str());
            return false;
        }
    }
}
