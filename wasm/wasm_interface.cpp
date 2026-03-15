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

    EMSCRIPTEN_KEEPALIVE const char* get_file_tree_json(const char* path)
    {
        static std::string result;
        result = file_utils::getFileTreeJson(path);
        return result.c_str();
    }

    EMSCRIPTEN_KEEPALIVE void run_validation(const char* path)
    {
        const ModelChecker validator;
        validator.validate(path);
    }
}
