#include "model_checker.h"

#include <emscripten.h>
#include <string>

extern "C"
{
    EMSCRIPTEN_KEEPALIVE const char* run_validation(const char* path)
    {
        const ModelChecker validator;
        const Certificate cert = validator.validate(path);

        static std::string result;
        result = cert.toJson();
        return result.c_str();
    }
}
