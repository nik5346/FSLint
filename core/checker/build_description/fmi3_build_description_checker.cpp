#include "fmi3_build_description_checker.h"

#include "certificate.h"

#include <libxml/tree.h>

void Fmi3BuildDescriptionChecker::checkFmiVersion(xmlNodePtr root, Certificate& cert) const
{
    TestResult test{"Version", TestStatus::PASS, {}};
    auto bd_fmi_version = getXmlAttribute(root, "fmi_version");
    if (!bd_fmi_version)
    {
        test.setStatus(TestStatus::FAIL);
        test.getMessages().emplace_back("Missing 'fmi_version' attribute in 'buildDescription.xml'.");
    }
    else if (*bd_fmi_version != getFmiVersion())
    {
        test.setStatus(TestStatus::FAIL);
        test.getMessages().emplace_back("fmi_version in 'buildDescription.xml' (" + *bd_fmi_version +
                                        ") does not match FMU version (" + getFmiVersion() + ").");
    }
    cert.printTestResult(test);
}
