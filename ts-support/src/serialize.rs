pub struct BinaryWriter {
    data: Vec<u8>,
}

impl BinaryWriter {
    pub fn new() -> Self {
        Self { data: Vec::new() }
    }

    pub fn into_vec(self) -> Vec<u8> {
        self.data
    }

    pub fn write_u16(&mut self, value: u16) {
        self.data.extend_from_slice(&value.to_le_bytes());
    }

    pub fn write_u32(&mut self, value: u32) {
        self.data.extend_from_slice(&value.to_le_bytes());
    }

    pub fn write_u64(&mut self, value: u64) {
        self.data.extend_from_slice(&value.to_le_bytes());
    }

    pub fn write_string(&mut self, value: &str) {
        self.write_u32(value.len() as u32);
        self.data.extend_from_slice(value.as_bytes());
    }
}
