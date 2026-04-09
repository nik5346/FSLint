#include "fmi3_build_description_checker.h"
#include <format>

#include "certificate.h"

#include <libxml/tree.h>

void Fmi3BuildDescriptionChecker::checkFmiVersion(xmlNodePtr root, Certificate& cert) const
{
    TestResult test{"Version", TestStatus::PASS, {}};
    auto bd_fmi_version = getXmlAttribute(root, "fmiVersion");
    if (!bd_fmi_version)
    {
        test.setStatus(TestStatus::FAIL);
        test.getMessages().emplace_back("Missing 'fmiVersion' attribute in 'buildDescription.xml'.");
    }
    else if (*bd_fmi_version != getFmiVersion())
    {
        test.setStatus(TestStatus::FAIL);
        test.getMessages().emplace_back(
            std::format("fmiVersion in 'buildDescription.xml' ({}) does not match FMU version ({}).", *bd_fmi_version,
                        getFmiVersion()));
    }
    cert.printTestResult(test);
}
