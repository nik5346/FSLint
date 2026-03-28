#pragma once

#include <filesystem>

class Certificate;

class Checker
{
  public:
    Checker() = default;
    virtual ~Checker() = default;

    // Disable copying and moving for interface class
    Checker(const Checker&) = delete;
    Checker& operator=(const Checker&) = delete;
    Checker(Checker&&) = delete;
    Checker& operator=(Checker&&) = delete;

    virtual void validate(const std::filesystem::path& path, Certificate& cert) const = 0;

    void setOriginalPath(const std::filesystem::path& path)
    {
        _original_path = path;
    }

  protected:
    const std::filesystem::path& getOriginalPath() const
    {
        return _original_path;
    }

  private:
    std::filesystem::path _original_path;
};
