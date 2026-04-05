#include "file_utils.h"
#include "model_checker.h"

#include <emscripten.h>
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
                // We use EM_ASM_INT to call window.confirm in the browser.
                // We pass the test name to the confirm dialog.
                return emscripten_run_script_int(
                           (std::string("window.confirm('SECURITY ISSUE DETECTED: ") + test.test_name +
                            "\\n\\nDo you want to continue validation?') ? 1 : 0")
                               .c_str()) != 0;
            });

        const Certificate cert = validator.validate(path, false, false, std::move(initial_cert));

        static std::string result;
        result = cert.toJson(path);
        return result.c_str();
    }
}
