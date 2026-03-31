#include "file_utils.h"
#include "model_checker.h"

#include <emscripten.h>
#include <string>

extern "C"
{
    /// @brief Entry point for WASM-based validation.
    /// @param path Path to the model file in the Emscripten filesystem.
    /// @return JSON string containing validation results.
    EMSCRIPTEN_KEEPALIVE const char* run_validation(const char* path)
    {
        const ModelChecker validator;
        Certificate cert = validator.validate(path);

        static std::string result;
        result = cert.toJson(path);
        return result.c_str();
    }
}
