class Counter {
private:
    static int secret;
public:
    static int get() { return secret; }
};
int Counter::secret = 42;
int main() { return Counter::get(); }
