// krdap — KRCompiler Debug Adapter Protocol (DAP) stub server.
//
// DAP messages use Content-Length prefixed JSON-RPC over stdio.
//
// This is a stub: it accepts a DAP session on stdio, advertises debugger
// capabilities, and returns success responses to all supported requests.
// It does not actually debug a target. It is the M4 DAP/GUI debug session
// stub (see moving.md §4 and implementation.md M4 checklist).
//
// Supported requests:
//   initialize, launch, setBreakpoints, configurationDone,
//   threads, stackTrace, scopes, variables,
//   continue, next, stepIn, stepOut, pause, disconnect
//
// Notifications (no id) are acknowledged silently.

#include <cstdio>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

namespace krdap {

namespace json {

// A minimal JSON value sufficient for DAP JSON-RPC messages.
struct Value {
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool b = false;
    double n = 0.0;
    std::string s;
    std::vector<Value> a;
    std::map<std::string, Value> o;

    bool is_null() const { return type == Type::Null; }
    bool is_bool() const { return type == Type::Bool; }
    bool is_number() const { return type == Type::Number; }
    bool is_string() const { return type == Type::String; }
    bool is_array() const { return type == Type::Array; }
    bool is_object() const { return type == Type::Object; }

    const Value* find(const std::string& key) const {
        if (type == Type::Object) {
            auto it = o.find(key);
            if (it != o.end()) {
                return &it->second;
            }
        }
        return nullptr;
    }

    double as_number(double def = 0.0) const {
        return type == Type::Number ? n : def;
    }
    int as_int(int def = 0) const {
        return type == Type::Number ? static_cast<int>(n) : def;
    }
    bool as_bool(bool def = false) const {
        return type == Type::Bool ? b : def;
    }
    std::string as_string(const std::string& def = std::string()) const {
        return type == Type::String ? s : def;
    }
};

// ---------- parser ----------

class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    Value parse() {
        skip_ws();
        Value v = parse_value();
        skip_ws();
        return v;
    }

private:
    std::string_view text_;
    size_t pos_ = 0;

    char peek() const {
        return pos_ < text_.size() ? static_cast<char>(text_[pos_]) : '\0';
    }
    char next() {
        return pos_ < text_.size() ? static_cast<char>(text_[pos_++]) : '\0';
    }

    void skip_ws() {
        while (pos_ < text_.size()) {
            char c = static_cast<char>(text_[pos_]);
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++pos_;
            } else {
                break;
            }
        }
    }

    Value parse_value() {
        skip_ws();
        char c = peek();
        switch (c) {
            case '{': return parse_object();
            case '[': return parse_array();
            case '"': return parse_string();
            case 't':
            case 'f': return parse_bool();
            case 'n': return parse_null();
            case '-':
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9': return parse_number();
            default: return Value(); // null on parse error
        }
    }

    Value parse_object() {
        Value v;
        v.type = Value::Type::Object;
        next(); // consume '{'
        skip_ws();
        if (peek() == '}') {
            next();
            return v;
        }
        for (;;) {
            skip_ws();
            if (peek() != '"') {
                break;
            }
            Value key_val = parse_string();
            std::string key = std::move(key_val.s);
            skip_ws();
            if (peek() != ':') {
                break;
            }
            next(); // consume ':'
            Value val = parse_value();
            v.o.emplace(std::move(key), std::move(val));
            skip_ws();
            char c = peek();
            if (c == ',') {
                next();
                continue;
            }
            if (c == '}') {
                next();
                break;
            }
            break;
        }
        return v;
    }

    Value parse_array() {
        Value v;
        v.type = Value::Type::Array;
        next(); // consume '['
        skip_ws();
        if (peek() == ']') {
            next();
            return v;
        }
        for (;;) {
            Value val = parse_value();
            v.a.push_back(std::move(val));
            skip_ws();
            char c = peek();
            if (c == ',') {
                next();
                continue;
            }
            if (c == ']') {
                next();
                break;
            }
            break;
        }
        return v;
    }

    Value parse_string() {
        Value v;
        v.type = Value::Type::String;
        next(); // consume opening '"'
        while (pos_ < text_.size()) {
            char c = static_cast<char>(text_[pos_]);
            if (c == '"') {
                next();
                break;
            }
            if (c == '\\') {
                next(); // consume backslash
                if (pos_ >= text_.size()) {
                    break;
                }
                char e = static_cast<char>(text_[pos_++]);
                switch (e) {
                    case '"': v.s.push_back('"'); break;
                    case '\\': v.s.push_back('\\'); break;
                    case '/': v.s.push_back('/'); break;
                    case 'b': v.s.push_back('\b'); break;
                    case 'f': v.s.push_back('\f'); break;
                    case 'n': v.s.push_back('\n'); break;
                    case 'r': v.s.push_back('\r'); break;
                    case 't': v.s.push_back('\t'); break;
                    case 'u': {
                        if (pos_ + 4 <= text_.size()) {
                            pos_ += 4;
                        }
                        v.s.push_back('?');
                        break;
                    }
                    default: v.s.push_back(e); break;
                }
            } else {
                next();
                v.s.push_back(c);
            }
        }
        return v;
    }

    Value parse_bool() {
        Value v;
        v.type = Value::Type::Bool;
        if (pos_ + 4 <= text_.size() && text_.compare(pos_, 4, "true") == 0) {
            v.b = true;
            pos_ += 4;
        } else if (pos_ + 5 <= text_.size() &&
                   text_.compare(pos_, 5, "false") == 0) {
            v.b = false;
            pos_ += 5;
        }
        return v;
    }

    Value parse_null() {
        Value v;
        v.type = Value::Type::Null;
        if (pos_ + 4 <= text_.size() && text_.compare(pos_, 4, "null") == 0) {
            pos_ += 4;
        }
        return v;
    }

    Value parse_number() {
        Value v;
        v.type = Value::Type::Number;
        size_t start = pos_;
        if (peek() == '-') {
            next();
        }
        while (pos_ < text_.size()) {
            char c = static_cast<char>(text_[pos_]);
            if (std::isdigit(static_cast<unsigned char>(c)) || c == '.' ||
                c == 'e' || c == 'E' || c == '+' || c == '-') {
                next();
            } else {
                break;
            }
        }
        std::string num_str(text_.substr(start, pos_ - start));
        try {
            v.n = std::stod(num_str);
        } catch (...) {
            v.n = 0.0;
        }
        return v;
    }
};

// ---------- serializer ----------

void escape_string(const std::string& s, std::string& out) {
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<unsigned char>(c));
                    out.append(buf);
                } else {
                    out.push_back(c);
                }
                break;
        }
    }
}

void serialize_value(const Value& v, std::string& out) {
    switch (v.type) {
        case Value::Type::Null:
            out += "null";
            break;
        case Value::Type::Bool:
            out += v.b ? "true" : "false";
            break;
        case Value::Type::Number: {
            char buf[64];
            if (v.n == static_cast<double>(static_cast<long long>(v.n)) &&
                v.n >= -999999999.0 && v.n <= 999999999.0) {
                std::snprintf(buf, sizeof(buf), "%lld",
                              static_cast<long long>(v.n));
            } else {
                std::snprintf(buf, sizeof(buf), "%g", v.n);
            }
            out.append(buf);
            break;
        }
        case Value::Type::String:
            out.push_back('"');
            escape_string(v.s, out);
            out.push_back('"');
            break;
        case Value::Type::Array:
            out.push_back('[');
            for (size_t i = 0; i < v.a.size(); ++i) {
                if (i > 0) {
                    out.push_back(',');
                }
                serialize_value(v.a[i], out);
            }
            out.push_back(']');
            break;
        case Value::Type::Object:
            out.push_back('{');
            {
                bool first = true;
                for (const auto& [key, val] : v.o) {
                    if (!first) {
                        out.push_back(',');
                    }
                    first = false;
                    out.push_back('"');
                    escape_string(key, out);
                    out.push_back('"');
                    out.push_back(':');
                    serialize_value(val, out);
                }
            }
            out.push_back('}');
            break;
    }
}

std::string serialize(const Value& v) {
    std::string out;
    serialize_value(v, out);
    return out;
}

} // namespace json

// ---------- DAP request handling ----------

using json::Value;

// Build a success response with the given id and body.
Value make_response(const Value& id_value, const Value& body) {
    Value r;
    r.type = Value::Type::Object;
    Value type_v;
    type_v.type = Value::Type::String;
    type_v.s = "response";
    r.o.emplace("type", std::move(type_v));
    if (!id_value.is_null()) {
        r.o.emplace("id", id_value);
    }
    Value s;
    s.type = Value::Type::Bool;
    s.b = true;
    r.o.emplace("success", s);
    r.o.emplace("body", body);
    return r;
}

Value make_error_response(const Value& id_value, int code,
                          std::string message) {
    Value r;
    r.type = Value::Type::Object;
    Value type_v;
    type_v.type = Value::Type::String;
    type_v.s = "response";
    r.o.emplace("type", std::move(type_v));
    if (!id_value.is_null()) {
        r.o.emplace("id", id_value);
    }
    Value s;
    s.type = Value::Type::Bool;
    s.b = false;
    r.o.emplace("success", s);
    Value err;
    err.type = Value::Type::Object;
    Value code_v;
    code_v.type = Value::Type::Number;
    code_v.n = static_cast<double>(code);
    err.o.emplace("code", code_v);
    Value msg_v;
    msg_v.type = Value::Type::String;
    msg_v.s = std::move(message);
    err.o.emplace("message", msg_v);
    r.o.emplace("error", err);
    return r;
}

// Handlers return the response "body" Value.

Value handle_initialize(const Value&) {
    Value caps;
    caps.type = Value::Type::Object;
    auto add_bool = [&caps](const char* name, bool val) {
        Value b;
        b.type = Value::Type::Bool;
        b.b = val;
        caps.o.emplace(name, b);
    };
    add_bool("supportsConfigurationDoneRequest", true);
    add_bool("supportsBreakpoints", true);
    add_bool("supportsConditionalBreakpoints", false);
    add_bool("supportsFunctionBreakpoints", false);
    add_bool("supportsDataBreakpoints", false);
    add_bool("supportsDelayedStackTraceLoading", false);
    add_bool("supportsSetVariable", false);
    add_bool("supportsRestartRequest", false);
    add_bool("supportsExceptionInfoRequest", false);
    add_bool("supportsTerminateDebuggee", true);
    add_bool("exceptionBreakpointFilters", false); // placeholder, not an array
    add_bool("supportsCompletionsRequest", false);
    add_bool("supportsModulesRequest", false);
    add_bool("supportsStepBack", false);
    add_bool("supportsStepInTargetsRequest", false);
    add_bool("supportsGotoTargetsRequest", false);
    add_bool("supportsStackTraceRequest", true);
    add_bool("supportsScopesRequest", true);
    add_bool("supportsVariablesRequest", true);
    add_bool("supportsContinueRequest", true);
    add_bool("supportsPauseRequest", true);
    add_bool("supportsTerminateRequest", true);
    add_bool("supportsExceptionOptions", false);
    add_bool("supportsDataBreakpoints", false);
    add_bool("supportsRestartFrame", false);
    add_bool("supportsGotoStackFrameRequest", false);

    Value result;
    result.type = Value::Type::Object;
    result.o.emplace("capabilities", caps);
    return result;
}

Value handle_launch(const Value&) {
    Value result;
    result.type = Value::Type::Object;
    return result;
}

Value handle_set_breakpoints(const Value&) {
    Value bps;
    bps.type = Value::Type::Array;
    Value result;
    result.type = Value::Type::Object;
    result.o.emplace("breakpoints", bps);
    return result;
}

Value handle_configuration_done(const Value&) {
    Value result;
    result.type = Value::Type::Object;
    return result;
}

Value handle_threads(const Value&) {
    Value threads;
    threads.type = Value::Type::Array;
    Value t;
    t.type = Value::Type::Object;
    Value id_v;
    id_v.type = Value::Type::Number;
    id_v.n = 1.0;
    t.o.emplace("id", id_v);
    Value name_v;
    name_v.type = Value::Type::String;
    name_v.s = "main thread";
    t.o.emplace("name", name_v);
    threads.a.push_back(std::move(t));
    Value result;
    result.type = Value::Type::Object;
    result.o.emplace("threads", threads);
    return result;
}

Value handle_stack_trace(const Value&) {
    Value frames;
    frames.type = Value::Type::Array;
    Value result;
    result.type = Value::Type::Object;
    result.o.emplace("stackFrames", frames);
    Value tf;
    tf.type = Value::Type::Number;
    tf.n = 0.0;
    result.o.emplace("totalFrames", tf);
    return result;
}

Value handle_scopes(const Value&) {
    Value scopes;
    scopes.type = Value::Type::Array;
    Value result;
    result.type = Value::Type::Object;
    result.o.emplace("scopes", scopes);
    return result;
}

Value handle_variables(const Value&) {
    Value vars;
    vars.type = Value::Type::Array;
    Value result;
    result.type = Value::Type::Object;
    result.o.emplace("variables", vars);
    return result;
}

Value handle_continue(const Value&) {
    Value r;
    r.type = Value::Type::Bool;
    r.b = true;
    Value result;
    result.type = Value::Type::Object;
    result.o.emplace("allThreadsContinued", r);
    return result;
}

Value handle_next(const Value&) { return handle_continue(Value()); }
Value handle_step_in(const Value&) { return handle_continue(Value()); }
Value handle_step_out(const Value&) { return handle_continue(Value()); }

Value handle_pause(const Value&) {
    Value result;
    result.type = Value::Type::Object;
    return result;
}

Value handle_disconnect(const Value&) {
    Value result;
    result.type = Value::Type::Object;
    return result;
}

Value handle_request(const std::string& method, const Value& params,
                     bool& should_exit) {
    should_exit = false;
    if (method == "initialize") {
        return handle_initialize(params);
    }
    if (method == "launch") {
        return handle_launch(params);
    }
    if (method == "setBreakpoints") {
        return handle_set_breakpoints(params);
    }
    if (method == "configurationDone") {
        return handle_configuration_done(params);
    }
    if (method == "threads") {
        return handle_threads(params);
    }
    if (method == "stackTrace") {
        return handle_stack_trace(params);
    }
    if (method == "scopes") {
        return handle_scopes(params);
    }
    if (method == "variables") {
        return handle_variables(params);
    }
    if (method == "continue") {
        return handle_continue(params);
    }
    if (method == "next") {
        return handle_next(params);
    }
    if (method == "stepIn") {
        return handle_step_in(params);
    }
    if (method == "stepOut") {
        return handle_step_out(params);
    }
    if (method == "pause") {
        return handle_pause(params);
    }
    if (method == "disconnect") {
        should_exit = true;
        return handle_disconnect(params);
    }
    // Unknown method: error response.
    return make_error_response(Value(), -32601,
                               "Method not found: " + method);
}

// ---------- stdio message transport ----------

bool read_line(std::string& line) {
    char buf[4096];
    if (std::fgets(buf, sizeof(buf), stdin) == nullptr) {
        return false; // EOF
    }
    line = buf;
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
        line.pop_back();
    }
    return true;
}

std::string read_body(size_t length) {
    std::string body;
    body.resize(length);
    size_t n = std::fread(body.data(), 1, length, stdin);
    body.resize(n);
    return body;
}

void write_message(const std::string& body) {
    std::string header =
        "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";
    std::fwrite(header.data(), 1, header.size(), stdout);
    std::fwrite(body.data(), 1, body.size(), stdout);
    std::fflush(stdout);
}

} // namespace krdap

int main() {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    using krdap::json::Value;
    using krdap::json::Parser;

    for (;;) {
        // Read header lines until an empty line.
        std::size_t content_length = 0;
        bool got_headers = false;

        for (;;) {
            std::string line;
            if (!krdap::read_line(line)) {
                got_headers = false;
                break;
            }
            if (line.empty()) {
                got_headers = true;
                break;
            }
            if (line.compare(0, 15, "Content-Length:") == 0) {
                std::string_view rest(line.data() + 16);
                while (!rest.empty() &&
                       (rest.front() == ' ' || rest.front() == '\t')) {
                    rest.remove_prefix(1);
                }
                try {
                    content_length = std::stoull(std::string(rest));
                } catch (...) {
                    content_length = 0;
                }
            }
        }

        if (!got_headers) {
            break;
        }

        // Read the JSON body.
        std::string body = krdap::read_body(content_length);
        if (body.empty() && content_length > 0) {
            break;
        }

        // Parse the JSON-RPC message.
        Value msg = Parser(body).parse();

        const Value* id_val = msg.find("id");
        const Value* method_val = msg.find("method");
        const Value* params_val = msg.find("params");

        Value params;
        if (params_val && params_val->is_object()) {
            params = *params_val;
        }

        std::string method;
        if (method_val && method_val->is_string()) {
            method = method_val->as_string();
        }

        if (method.empty()) {
            // Not a recognizable request; skip.
            continue;
        }

        bool is_request = id_val && !id_val->is_null();
        if (is_request) {
            bool should_exit = false;
            Value result =
                krdap::handle_request(method, params, should_exit);
            Value response = krdap::make_response(*id_val, result);
            krdap::write_message(krdap::json::serialize(response));
            if (should_exit) {
                break;
            }
        }
        // Notifications have no id: no response is sent.
    }

    return 0;
}
