pub mod archive;
pub mod binary_parser;
pub mod certificate;
pub mod checker;
pub mod file_utils;
pub mod iso8601;

pub use certificate::Certificate;
pub use model_checker::ModelChecker;

pub mod model_checker;

use wasm_bindgen::prelude::*;

#[wasm_bindgen]
pub fn run_validation(path: &str) -> String {
    let mut checker = ModelChecker::new();
    match checker.validate(path) {
        Ok(_) => checker.cert.to_json(),
        Err(e) => {
            checker.cert.add_test_result(
                "Validation Error",
                certificate::TestStatus::FAIL,
                vec![format!("Error during validation: {}", e)],
            );
            checker.cert.to_json()
        }
    }
}
