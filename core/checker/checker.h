#pragma once

#include <filesystem>

class Certificate;

/// @brief Base interface for all validation engines in FSLint.
class Checker
{
  public:
    /// @brief Default constructor.
    Checker() = default;

    /// @brief Virtual destructor.
    virtual ~Checker() = default;

    // Disable copying and moving for interface class
    Checker(const Checker&) = delete;
    Checker& operator=(const Checker&) = delete;
    Checker(Checker&&) = delete;
    Checker& operator=(Checker&&) = delete;

    /// @brief Validates the model at the specified path.
    /// @param path FMU or SSP path.
    /// @param cert Certificate to record results.
    virtual void validate(const std::filesystem::path& path, Certificate& cert) const = 0;

    /// @brief Sets the original path (before extraction).
    /// @param path Original file path.
    void setOriginalPath(const std::filesystem::path& path)
    {
        _original_path = path;
    }

  protected:
    /// @brief Gets the original file path.
    /// @return Original path.
    [[nodiscard]] const std::filesystem::path& getOriginalPath() const
    {
        return _original_path;
    }

  private:
    std::filesystem::path _original_path;
};
