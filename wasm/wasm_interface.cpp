#include "file_utils.h"
#include "model_checker.h"

#include <emscripten.h>
#include <string>

extern "C"
{
    EMSCRIPTEN_KEEPALIVE bool is_binary(const char* path)
    {
        return file_utils::isBinary(path);
    }

    EMSCRIPTEN_KEEPALIVE const char* run_validation(const char* path)
    {
        const ModelChecker validator;
        Certificate cert = validator.validate(path);

        static std::string result;
        result = cert.toJson(path);
        return result.c_str();
    }
}
