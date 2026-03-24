#include "fmi2_build_description_checker.h"

#include "certificate.h"

#include <libxml/tree.h>

void Fmi2BuildDescriptionChecker::checkFmiVersion(xmlNodePtr root, Certificate& cert) const
{
    TestResult test{"Version", TestStatus::PASS, {}};
    auto bd_fmi_version = getXmlAttribute(root, "fmiVersion");
    if (!bd_fmi_version)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Missing 'fmiVersion' attribute in 'buildDescription.xml'.");
    }
    else if (*bd_fmi_version != "3.0")
    {
        // For FMI 2.x, it's a backport from FMI 3.0 and the fmiVersion attribute is fixed to "3.0".
        test.status = TestStatus::FAIL;
        test.messages.push_back("fmiVersion in 'buildDescription.xml' must be '3.0' for FMI 2.x FMUs (found '" +
                                *bd_fmi_version + "').");
    }
    cert.printTestResult(test);
}
