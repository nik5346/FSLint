#include "fmi3_build_description_checker.h"
#include "certificate.h"
#include <regex>

void Fmi3BuildDescriptionChecker::checkFmiVersion(xmlNodePtr root, Certificate& cert)
{
    TestResult test{"Build Description FMI Version", TestStatus::PASS, {}};
    auto bd_fmi_version = getXmlAttribute(root, "fmiVersion");
    if (!bd_fmi_version)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Missing 'fmiVersion' attribute in 'buildDescription.xml'.");
    }
    else
    {
        // FMI 3.0 regex from XSD: 3[.](0|[1-9][0-9]*)([.](0|[1-9][0-9]*))?(-.+)?
        std::regex fmi3_pattern(R"(^3\.(0|[1-9][0-9]*)(\.(0|[1-9][0-9]*))?(-.+)?$)");
        if (!std::regex_match(*bd_fmi_version, fmi3_pattern))
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("fmiVersion in 'buildDescription.xml' (" + *bd_fmi_version +
                                    ") must match FMI 3.0 format.");
        }

        if (*bd_fmi_version != _fmi_version)
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("fmiVersion in 'buildDescription.xml' (" + *bd_fmi_version +
                                    ") does not match FMU version (" + _fmi_version + ").");
        }
    }
    cert.printTestResult(test);
}
