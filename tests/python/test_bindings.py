import os
import sys
import unittest

# Add the build directory to the path so we can import fslint
build_dir = os.path.join(os.getcwd(), 'build')
sys.path.append(build_dir)

import fslint

class TestFSLintBindings(unittest.TestCase):
    def test_model_checker_validate(self):
        checker = fslint.ModelChecker()

        # Path to a test FMU (using a directory as an FMU)
        test_fmu_path = os.path.join('tests', 'data', 'fmi2', 'pass', 'type_usage')

        cert = checker.validate(test_fmu_path, quiet=True)

        self.assertIsInstance(cert, fslint.Certificate)

        # Print results if failed for debugging
        if cert.is_failed():
            print("\nValidation failed. Results:")
            for res in cert.get_results():
                print(f"  [{res.status}] {res.test_name}")
                for msg in res.messages:
                    print(f"    - {msg}")

        self.assertFalse(cert.is_failed())
        # It's okay if overall status is WARNING as long as it's not FAIL
        self.assertIn(cert.get_overall_status(), [fslint.TestStatus.PASS, fslint.TestStatus.WARNING])

        results = cert.get_results()
        self.assertGreater(len(results), 0)

        for result in results:
            self.assertIsInstance(result, fslint.TestResult)
            self.assertNotEqual(result.status, fslint.TestStatus.FAIL)

    def test_test_status_enum(self):
        self.assertEqual(int(fslint.TestStatus.PASS), 0)
        self.assertEqual(int(fslint.TestStatus.FAIL), 1)
        self.assertEqual(int(fslint.TestStatus.WARNING), 2)

if __name__ == '__main__':
    unittest.main()
