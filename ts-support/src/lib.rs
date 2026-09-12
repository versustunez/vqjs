use crate::metadata::{vqjs_parse_file_metadata, vqjs_serialize_parse_data};
use crate::transpile::Transpile;
use std::ffi::{CStr, CString, c_char, c_void};

mod error;
mod metadata;
mod transpile;

mod parse_struct;
mod serialize;

#[repr(C)]
pub struct BinaryBlob {
    pub ptr: *mut u8,
    pub len: u64,
}

#[repr(C)]
pub struct Logger {
    callback: unsafe extern "C" fn(*mut c_void, *const c_char),
    userdata: *mut c_void,
}

pub(crate) fn log(message: String, logger: &Option<Logger>) {
    if let Some(logger) = logger {
        let cstr = CString::new(message).unwrap();
        // c interopt call... sadly
        unsafe {
            (logger.callback)(logger.userdata, cstr.as_ptr());
        }
    } else {
        println!("{}", message);
    }
}

fn to_string(str: *const c_char) -> String {
    let cstr = unsafe { CStr::from_ptr(str) };
    cstr.to_str().expect("works").to_string()
}

#[unsafe(no_mangle)]
pub extern "C" fn vqjs_parse_file(
    file_name: *const c_char,
    output_file: *const c_char,
    logger: Logger,
) -> bool {
    let file_string = to_string(file_name);
    let output_string = to_string(output_file);
    Transpile::new(logger).transpile(file_string.as_str(), output_string.as_str())
}

#[unsafe(no_mangle)]
pub extern "C" fn vqjs_reflect_metadata(file_name: *const c_char, logger: Logger) -> BinaryBlob {
    let res = vqjs_parse_file_metadata(to_string(file_name).as_str());
    if res.is_err() {
        log(format!("{:?}", res.unwrap_err()), &Some(logger));
        return BinaryBlob {
            ptr: 0 as *mut u8,
            len: 0,
        };
    }
    // we need to do our job now
    let res = res.unwrap();
    let mut data = vqjs_serialize_parse_data(res.as_ref());
    let blob = BinaryBlob {
        ptr: data.as_mut_ptr(),
        len: data.len() as u64,
    };
    std::mem::forget(data);
    blob
}

#[unsafe(no_mangle)]
pub extern "C" fn vqjs_reflect_free(ptr: *mut u8, len: usize) {
    unsafe {
        Vec::from_raw_parts(ptr, len, len);
    }
}
