use clap::Parser;
use fslint::ModelChecker;
use std::path::PathBuf;

#[derive(Parser, Debug)]
#[command(author, version, about, long_about = None)]
struct Args {
    #[arg(index = 1)]
    path: PathBuf,

    #[arg(short, long)]
    json: bool,
}

fn main() {
    let args = Args::parse();

    let mut checker = ModelChecker::new();
    match checker.validate(&args.path) {
        Ok(_) => {
            if args.json {
                println!("{}", checker.cert.to_json());
            } else {
                println!("{}", checker.cert.report);
                println!("\nSummary:");
                println!("  Standard: {}", checker.cert.summary.standard);
                println!("  Model Name: {}", checker.cert.summary.model_name);
                println!("  FMI Version: {}", checker.cert.summary.fmi_version);
                println!("  Overall Status: {:?}", checker.cert.get_overall_status());
            }
        }
        Err(e) => {
            if args.json {
                println!("{}", checker.cert.to_json());
            } else {
                println!("{}", checker.cert.report);
                eprintln!("\x1b[31mError during validation: {}\x1b[0m", e);
            }
            std::process::exit(1);
        }
    }
}
