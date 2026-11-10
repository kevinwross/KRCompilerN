int destroyed = 0;
class Error {
public:
    ~Error() { destroyed = 1; }
};
int main() {
    int caught = 0;
    try { Error error; throw error; }
    catch (const Error& caught_error) { caught = 1; }
    return destroyed == 1 && caught == 1 ? 42 : 0;
}
