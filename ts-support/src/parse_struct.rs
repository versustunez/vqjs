#[derive(Debug)]
pub enum TypeValue {
    BooleanLiteral(bool),
    NumberLiteral(f64),
    StringLiteral(String),
}
#[derive(Debug)]
pub struct Type {
    pub name: String,
    pub array: bool,
    pub map: bool,
    pub value: Option<TypeValue>,
    pub arguments: Vec<Type>,
}
#[derive(Debug)]
pub struct Member {
    pub name: String,
    pub typ: Type,
}

#[derive(Debug)]
pub struct Classes {
    pub name: String,
    pub super_class: String,
    pub members: Vec<Member>,
}

#[derive(Debug)]
pub struct ParseData {
    pub classes: Vec<Classes>,
    pub total: u64,
}

impl Type {
    pub fn from(str: &str) -> Self {
        Self {
            name: str.to_string(),
            array: false,
            map: false,
            arguments: Vec::new(),
            value: None,
        }
    }
    pub fn from_as_array(str: &str) -> Self {
        Self {
            name: str.to_string(),
            array: true,
            map: false,
            arguments: Vec::new(),
            value: None,
        }
    }

    pub fn new(name: String, array: bool, map: bool) -> Self {
        Self {
            name,
            array,
            map,
            arguments: Vec::new(),
            value: None,
        }
    }
    
    pub fn with_value(mut self, value: TypeValue) -> Self {
        self.value = Some(value);
        self
    }
}
