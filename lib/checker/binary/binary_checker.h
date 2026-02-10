#pragma once

#include "checker.h"
#include <libxml/tree.h>
#include <map>
#include <optional>
#include <set>
#include <string>

class BinaryChecker : public Checker
{
  public:
    struct Capabilities
    {
        bool canGetAndSetFMUstate = false;
        bool canSerializeFMUstate = false;
        bool providesDirectionalDerivative = false;
        bool providesAdjointDerivative = false;
    };

    void validate(const std::filesystem::path& path, Certificate& cert) override;

  private:
    std::vector<std::string> getExpectedFunctions(const std::string& version, const std::set<std::string>& interfaces,
                                                  const Capabilities& caps);
    static std::optional<std::string> getXmlAttribute(xmlNodePtr node, const std::string& attr_name);
};
