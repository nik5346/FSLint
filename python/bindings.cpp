#include "certificate.h"
#include "model_checker.h"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>

namespace py = pybind11;

PYBIND11_MODULE(fslint, m)
{
    m.doc() = "FSLint Python bindings";

    py::enum_<TestStatus>(m, "TestStatus")
        .value("PASS", TestStatus::PASS)
        .value("FAIL", TestStatus::FAIL)
        .value("WARNING", TestStatus::WARNING)
        .export_values();

    py::class_<TestResult>(m, "TestResult")
        .def_readwrite("test_name", &TestResult::test_name)
        .def_readwrite("status", &TestResult::status)
        .def_readwrite("messages", &TestResult::messages);

    py::class_<NestedModelResult>(m, "NestedModelResult")
        .def_readwrite("name", &NestedModelResult::name)
        .def_readwrite("status", &NestedModelResult::status)
        .def_readwrite("nested_models", &NestedModelResult::nested_models);

    py::class_<Certificate>(m, "Certificate")
        .def(py::init<>())
        .def("set_quiet", &Certificate::setQuiet)
        .def("is_quiet", &Certificate::isQuiet)
        .def("is_failed", &Certificate::isFailed)
        .def("get_overall_status", &Certificate::getOverallStatus)
        .def("get_full_report", &Certificate::getFullReport)
        .def("get_results", &Certificate::getResults)
        .def("get_nested_models", &Certificate::getNestedModels)
        .def("save_to_file", &Certificate::saveToFile);

    py::class_<ModelChecker>(m, "ModelChecker")
        .def(py::init<>())
        .def("validate", &ModelChecker::validate, py::arg("path"), py::arg("quiet") = false)
        .def("add_certificate", &ModelChecker::addCertificate)
        .def("update_certificate", &ModelChecker::updateCertificate)
        .def("remove_certificate", &ModelChecker::removeCertificate)
        .def("display_certificate", &ModelChecker::displayCertificate)
        .def("verify_certificate", &ModelChecker::verifyCertificate)
        .def("is_version_deprecated", &ModelChecker::isVersionDeprecated);
}
