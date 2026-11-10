struct Byte { char value; };
struct Short { short value; };
Byte make_byte() { Byte object; object.value = 7; return object; }
Short make_short() { Short object; object.value = 35; return object; }
int read_byte(Byte object) { return object.value; }
int read_short(Short object) { return object.value; }
int main() {
    Byte byte = make_byte();
    Short small = make_short();
    return read_byte(byte) + read_short(small);
}
