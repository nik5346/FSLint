#include "fmi2_build_description_checker.h"

#include "certificate.h"

#include <libxml/tree.h>

void Fmi2BuildDescriptionChecker::checkFmiVersion(xmlNodePtr root, Certificate& cert)
{
    TestResult test{"Build Description FMI Version", TestStatus::PASS, {}};
    auto bd_fmi_version = getXmlAttribute(root, "fmiVersion");
    if (!bd_fmi_version)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Missing 'fmiVersion' attribute in 'buildDescription.xml'.");
    }
    else if (*bd_fmi_version != getFmiVersion())
    {
        // For FMI 2.0, it's a backport, we issue a warning if versions don't match.
        test.status = TestStatus::WARNING;
        test.messages.push_back("fmiVersion in 'buildDescription.xml' (" + *bd_fmi_version +
                                ") does not match FMU version (" + getFmiVersion() + ").");
    }
    cert.printTestResult(test);
}
