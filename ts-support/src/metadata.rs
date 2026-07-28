use crate::error::RuntimeError;
use crate::parse_struct::{Classes, Member, ParseData, Type};
use crate::serialize::BinaryWriter;
use oxc_allocator::Allocator;
use oxc_ast::ast::{Class, ClassElement, SourceType, TSType};
use oxc_ast_visit::Visit;
use oxc_parser::{ParseOptions, Parser};
use std::fs;
use std::path::Path;

struct MetaVisitor {
    data: Box<ParseData>,
}

impl<'a> Visit<'a> for MetaVisitor {
    fn visit_class(&mut self, it: &Class<'a>) {
        if !it.is_declaration() {
            return;
        }
        let name = it
            .id
            .as_ref()
            .map(|id| id.name.to_string())
            .unwrap_or_default();
        // we don't support anonymous classes in the metadata collector
        if name.is_empty() {
            return;
        }

        let mut super_class = String::new();
        if it.super_class.is_some() {
            let sc = it.super_class.as_ref().expect("has value");
            if sc.is_identifier_reference() {
                let ir = &sc
                    .get_identifier_reference()
                    .expect("failed to get IdentifierReference");
                super_class = ir.name.to_string();
            }
        }

        let mut written_element = Classes {
            name,
            super_class,
            members: vec![],
        };
        it.body.body.iter().for_each(|element| match element {
            ClassElement::PropertyDefinition(value) => {
                let mut typ = Type::from("unknown");
                if value.type_annotation.is_some() {
                    typ = Self::get_ts_type_to_vqjs_type(
                        &value.type_annotation.as_ref().unwrap().type_annotation,
                    );
                }
                let name = element.static_name().unwrap_or_default().to_string();
                written_element.members.push(Member { name, typ });
            }
            _default => {}
        });

        self.data.total += 1;
        self.data.classes.push(written_element)
    }
}

impl MetaVisitor {
    fn get_ts_type_to_vqjs_type(value: &TSType) -> Type {
        match value {
            TSType::TSAnyKeyword(_) => Type::from("any"),
            TSType::TSBigIntKeyword(_) => Type::from("bigint"),
            TSType::TSBooleanKeyword(_) => Type::from("boolean"),
            TSType::TSIntrinsicKeyword(_) => Type::from("intrinsic"),
            TSType::TSNeverKeyword(_) => Type::from("never"),
            TSType::TSNullKeyword(_) => Type::from("null"),
            TSType::TSNumberKeyword(_) => Type::from("number"),
            TSType::TSObjectKeyword(_) => Type::from("object"),
            TSType::TSStringKeyword(_) => Type::from("string"),
            TSType::TSSymbolKeyword(_) => Type::from("symbol"),
            TSType::TSUndefinedKeyword(_) => Type::from("undefined"),
            TSType::TSUnknownKeyword(_) => Type::from("unknown"),
            TSType::TSVoidKeyword(_) => Type::from("void"),
            TSType::TSArrayType(arr) => Type::from_as_array(
                Self::get_ts_type_to_vqjs_type(&arr.element_type)
                    .name
                    .as_str(),
            ),
            // We have to maybe support this? but right now v3d does not care about cases like this.
            TSType::TSConditionalType(_) => Type::from("unknown"),
            TSType::TSConstructorType(_) => Type::from("constructor"),
            TSType::TSFunctionType(_) => Type::from("function"),
            TSType::TSImportType(_) => Type::from("import"),
            TSType::TSIndexedAccessType(element) => Type::new(
                format!(
                    "{}::{}",
                    Self::get_ts_type_to_vqjs_type(&element.object_type).name,
                    Self::get_ts_type_to_vqjs_type(&element.index_type).name
                ),
                false,
                true,
            ),
            TSType::TSInferType(_) => Type::from("infer"),
            TSType::TSIntersectionType(_) => Type::from("intersection"),
            TSType::TSLiteralType(_) => Type::from("literal"),
            TSType::TSMappedType(_) => Type::from("mapped"),
            TSType::TSNamedTupleMember(_) => Type::from("named-tuple"),
            TSType::TSTemplateLiteralType(_) => Type::from("template-literal"),
            TSType::TSThisType(_) => Type::from("this"),
            TSType::TSTupleType(_) => Type::from("tuple"),
            TSType::TSTypeLiteral(_) => Type::from("literal"),
            TSType::TSTypeOperatorType(_) => Type::from("operator"),
            TSType::TSTypePredicate(_) => Type::from("predicate"),
            TSType::TSTypeQuery(_) => Type::from("query"),
            TSType::TSTypeReference(_) => Type::from("reference"),
            TSType::TSUnionType(element) => Type::from(
                element
                    .types
                    .iter()
                    .map(|e| Self::get_ts_type_to_vqjs_type(e).name)
                    .collect::<Vec<String>>()
                    .join(",")
                    .as_str(),
            ),
            TSType::TSParenthesizedType(_) => Type::from("parent"),
            TSType::JSDocNullableType(_) => Type::from("nullable"),
            TSType::JSDocNonNullableType(_) => Type::from("non-nullable"),
            TSType::JSDocUnknownType(_) => Type::from("unknown"),
        }
    }
}

pub(crate) fn vqjs_parse_file_metadata(file_name: &str) -> Result<Box<ParseData>, RuntimeError> {
    let file_string = file_name;
    let path = Path::new(&file_string);
    if !path.exists() {
        return Err(RuntimeError::new("File does not exists"));
    }
    let source_text =
        fs::read_to_string(path).map_err(|_| RuntimeError::new("Failed to read file"))?;
    let source_type = SourceType::from_path(path)
        .map_err(|_| RuntimeError::new("Failed to parse source type"))?;
    parse_metadata(&source_text, source_type)
}

fn parse_metadata(
    source_text: &String,
    source_type: SourceType,
) -> Result<Box<ParseData>, RuntimeError> {
    let allocator = Allocator::default();
    let ret = Parser::new(&allocator, &source_text, source_type)
        .with_options(ParseOptions {
            parse_regular_expression: true,
            ..ParseOptions::default()
        })
        .parse();

    let program = ret.program;

    if !ret.diagnostics.is_empty() {
        for error in ret.diagnostics {
            let error = error.with_source_code(source_text.clone());
            println!("{error:?}");
        }
        return Err(RuntimeError::new("Parsed with Errors"));
    }

    let parse_data = Box::new(ParseData {
        classes: vec![],
        total: 0,
    });

    let mut visitor = MetaVisitor { data: parse_data };
    visitor.visit_program(&program);

    Ok(visitor.data)
}

static MAGIC: u32 = 0x53545156;
static VERSION: u32 = 0x1;

pub(crate) fn vqjs_serialize_parse_data(data: &ParseData) -> Vec<u8> {
    let mut writer = BinaryWriter::new();
    writer.write_u32(MAGIC);
    writer.write_u32(VERSION);
    writer.write_u64(data.total);
    data.classes.iter().for_each(|class| {
        writer.write_string(&class.name);
        writer.write_string(&class.super_class);
        writer.write_u64(class.members.len() as u64);
        class.members.iter().for_each(|member| {
            writer.write_string(&member.name);
            writer.write_string(&member.typ.name);
            let mut flag: u16 = 0;
            if member.typ.array {
                flag |= 1 << 0;
            }
            if member.typ.map {
                flag |= 1 << 1;
            }
            writer.write_u16(flag)
        })
    });
    writer.into_vec()
}

#[cfg(test)]
mod tests {
    use crate::metadata::parse_metadata;
    use oxc_ast::ast::SourceType;

    #[test]
    fn test_class_found() {
        let test_code = "class Foo { test: number = 0 }".to_string();
        let x = parse_metadata(&test_code, SourceType::ts());
        assert!(x.is_ok());
        assert!(x.unwrap().total > 0);
    }
}
