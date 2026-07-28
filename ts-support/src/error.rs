use std::error::Error;
use std::fmt::Display;
use std::str::Utf8Error;

#[derive(Debug)]
pub(crate) struct RuntimeError {
    message: String,
}

impl RuntimeError {
    pub(crate) fn new(message: &str) -> RuntimeError {
        RuntimeError {
            message: message.to_string(),
        }
    }
}

impl From<Utf8Error> for RuntimeError {
    fn from(value: Utf8Error) -> Self {
        RuntimeError {
            message: value.to_string(),
        }
    }
}

impl From<&str> for RuntimeError {
    fn from(value: &str) -> Self {
        RuntimeError::new(value)
    }
}

impl Display for RuntimeError {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "{}", self.message)
    }
}

impl Error for RuntimeError {}
