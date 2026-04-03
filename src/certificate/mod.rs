use serde::{Deserialize, Serialize};
use std::fmt;
use std::fs;
use std::path::Path;

#[derive(Debug, Clone, Copy, PartialEq, Serialize, Deserialize)]
pub enum TestStatus {
    PASS,
    FAIL,
    WARNING,
}

impl fmt::Display for TestStatus {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            TestStatus::PASS => write!(f, "PASS"),
            TestStatus::FAIL => write!(f, "FAIL"),
            TestStatus::WARNING => write!(f, "WARNING"),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TestResult {
    pub test_name: String,
    pub status: TestStatus,
    pub messages: Vec<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NestedModelResult {
    pub name: String,
    pub status: TestStatus,
    pub nested_models: Vec<NestedModelResult>,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct ModelSummary {
    pub standard: String,
    pub model_name: String,
    pub fmi_version: String,
    pub model_version: String,
    pub guid: String,
    pub generation_tool: String,
    pub generation_date_and_time: String,
    pub author: String,
    pub copyright: String,
    pub license: String,
    pub description: String,
    pub platforms: Vec<String>,
    pub interfaces: Vec<String>,
    pub layered_standards: Vec<String>,
    pub has_icon: bool,
    pub fmu_types: Vec<String>,
    pub source_language: String,
    pub total_size: u64,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct Certificate {
    pub results: Vec<TestResult>,
    pub nested_models: Vec<NestedModelResult>,
    pub summary: ModelSummary,
    pub report: String,
    #[serde(skip)]
    pub quiet: bool,
}

impl Certificate {
    pub fn new() -> Self {
        Self {
            results: Vec::new(),
            nested_models: Vec::new(),
            summary: ModelSummary::default(),
            report: String::new(),
            quiet: false,
        }
    }

    pub fn add_test_result(&mut self, test_name: &str, status: TestStatus, messages: Vec<String>) {
        let result = TestResult {
            test_name: test_name.to_string(),
            status,
            messages: messages.clone(),
        };

        if !self.quiet {
            let status_str = match status {
                TestStatus::PASS => "\x1b[32mPASS\x1b[0m",
                TestStatus::FAIL => "\x1b[31mFAIL\x1b[0m",
                TestStatus::WARNING => "\x1b[33mWARN\x1b[0m",
            };
            self.report
                .push_str(&format!("[{}] {}\n", status_str, test_name));
            for msg in &messages {
                self.report.push_str(&format!("  - {}\n", msg));
            }
        }

        self.results.push(result);
    }

    pub fn log(&mut self, message: &str) {
        if !self.quiet {
            self.report.push_str(message);
            self.report.push('\n');
        }
    }

    pub fn set_summary(&mut self, summary: ModelSummary) {
        self.summary = summary;
    }

    pub fn get_overall_status(&self) -> TestStatus {
        if self.results.iter().any(|r| r.status == TestStatus::FAIL) {
            TestStatus::FAIL
        } else if self.results.iter().any(|r| r.status == TestStatus::WARNING) {
            TestStatus::WARNING
        } else {
            TestStatus::PASS
        }
    }

    pub fn to_json(&self) -> String {
        serde_json::to_string_pretty(self).unwrap_or_default()
    }

    pub fn save_to_file<P: AsRef<Path>>(&self, path: P) -> anyhow::Result<()> {
        let json = self.to_json();
        fs::write(path, json)?;
        Ok(())
    }
}
