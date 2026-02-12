#include "fmi3_build_description_checker.h"
#include "certificate.h"
#include "fmi_version_utils.h"

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
        if (!FmiVersionUtils::isValidFmi3Version(*bd_fmi_version))
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("fmiVersion in 'buildDescription.xml' (" + *bd_fmi_version +
                                    ") must match FMI 3.0 format.");
        }

        if (*bd_fmi_version != getFmiVersion())
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("fmiVersion in 'buildDescription.xml' (" + *bd_fmi_version +
                                    ") does not match FMU version (" + getFmiVersion() + ").");
        }
    }
    cert.printTestResult(test);
}
