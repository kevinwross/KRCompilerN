class Error {
public:
    int code;
    Error(int input) { code = input; }
};
int main() {
    try { Error error(42); throw error; }
    catch (const Error& caught) { return caught.code; }
    return 0;
}
