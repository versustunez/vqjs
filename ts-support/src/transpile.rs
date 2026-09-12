use crate::error::RuntimeError;
use crate::{Logger, log};
use deno_ast::diagnostics::Diagnostic;
use deno_ast::{
    DecoratorsTranspileOption, EmitOptions, ImportsNotUsedAsValues, MediaType, ModuleKind,
    ParsedSource, SourceMapOption, TranspileModuleOptions, TranspileOptions,
};
use deno_lint::diagnostic::LintDiagnostic;
use deno_lint::linter::{LintConfig, LintFileOptions, Linter, LinterOptions};
use deno_lint::rules::get_all_rules;
use deno_lint::rules::recommended_rules;
use std::borrow::Cow;
use std::collections::HashSet;
use std::fs;
use std::fs::write;
use std::path::Path;
use url::Url;

pub(crate) struct Transpile {
    logger: Option<Logger>,
}

impl Transpile {
    pub(crate) fn new(logger: Logger) -> Transpile {
        Transpile {
            logger: Option::from(logger),
        }
    }

    pub(crate) fn transpile(&self, filename: &str, output: &str) -> bool {
        let path = Path::new(&filename);
        if !path.exists() {
            log(
                format!("path does not exists {:?}", path.to_str()),
                &self.logger,
            );
            return false;
        }
        let source_text =
            fs::read_to_string(path).map_err(|_| RuntimeError::new("Failed to read file"));
        if source_text.is_err() {
            log(format!("{:?}", source_text.err().unwrap()), &self.logger);
            return false;
        }
        let result = Lint::lint(path, &source_text.unwrap(), &self.logger);
        if result.is_none() {
            return false;
        }
        let tu = result.unwrap();
        if !tu.1.is_empty() {
            tu.1.iter().for_each(|diag| {
                log(diag.message().to_string(), &self.logger);
            });
            return false;
        }
        let res = tu.0.transpile(
            &TranspileOptions {
                decorators: DecoratorsTranspileOption::LegacyTypeScript {
                    emit_metadata: false,
                },
                verbatim_module_syntax: false,
                imports_not_used_as_values: ImportsNotUsedAsValues::Remove,
                jsx: None,
                var_decl_imports: false,
            },
            &TranspileModuleOptions {
                module_kind: Some(ModuleKind::Esm),
            },
            &EmitOptions {
                source_map: SourceMapOption::None,
                source_map_base: None,
                source_map_file: None,
                inline_sources: false,
                remove_comments: true,
            },
        );
        if res.is_err() {
            log(format!("{:?}", res.unwrap_err()), &self.logger);
            return false;
        }

        let out = write(
            Path::new(output),
            res.unwrap().into_source().text.as_bytes(),
        );
        if out.is_err() {
            log(format!("write error: {:?}", out.unwrap_err()), &self.logger);
            return false;
        }
        true
    }
}

struct Lint {}

impl Lint {
    pub(crate) fn lint(
        path: &Path,
        source_text: &String,
        logger: &Option<Logger>,
    ) -> Option<(ParsedSource, Vec<LintDiagnostic>)> {
        let rules = recommended_rules(get_all_rules());
        let all_rule_codes = Self::get_all_rules_codes();

        let linter = Linter::new(LinterOptions {
            rules,
            all_rule_codes,
            custom_ignore_diagnostic_directive: None,
            custom_ignore_file_directive: None,
        });
        let url =
            Url::parse(format!("file://{}", path.display()).as_str()).expect("Failed to parse URL");
        let x = linter.lint_file(LintFileOptions {
            specifier: url,
            source_code: source_text.to_string(),
            media_type: MediaType::TypeScript,
            config: LintConfig {
                default_jsx_factory: None,
                default_jsx_fragment_factory: None,
            },
            external_linter: None,
        });
        if x.is_err() {
            let error = x.err().unwrap();
            log(format!("{:?}", error.message()), logger);
            return None;
        }

        Some(x.unwrap())
    }

    fn get_all_rules_codes() -> HashSet<Cow<'static, str>> {
        get_all_rules()
            .into_iter()
            .map(|rule| rule.code())
            .map(Cow::from)
            .collect()
    }
}
