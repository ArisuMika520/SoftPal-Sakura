#include "json_mini.h"

#include <cctype>
#include <cstring>
#include <sstream>

namespace sp::json
{

    namespace
    {

        class Parser
        {
        public:
            Parser(const std::string &s) : data_(s.data()), end_(s.data() + s.size()) {}

            bool parse_value(Value &out, std::string &err)
            {
                skip_ws();
                if (cur_ == end_)
                {
                    err = "unexpected end of input";
                    return false;
                }
                char c = *cur_;
                if (c == '{')
                    return parse_object(out, err);
                if (c == '[')
                    return parse_array(out, err);
                if (c == '"')
                    return parse_string_value(out, err);
                if (c == 't' || c == 'f')
                    return parse_bool(out, err);
                if (c == 'n')
                    return parse_null(out, err);
                if (c == '-' || (c >= '0' && c <= '9'))
                    return parse_number(out, err);
                err = std::string("unexpected character '") + c + "' at offset " + std::to_string(cur_ - data_);
                return false;
            }

            bool eof_ok()
            {
                skip_ws();
                return cur_ == end_;
            }

        private:
            const char *data_;
            const char *cur_ = nullptr;
            const char *end_;
            bool initialized_ = false;

            void ensure_init()
            {
                if (!initialized_)
                {
                    cur_ = data_;
                    initialized_ = true;
                }
            }

            void skip_ws()
            {
                ensure_init();
                while (cur_ < end_)
                {
                    char c = *cur_;
                    if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
                        ++cur_;
                    else
                        break;
                }
            }

            bool expect(char c, std::string &err)
            {
                if (cur_ >= end_ || *cur_ != c)
                {
                    err = std::string("expected '") + c + "' at offset " + std::to_string(cur_ - data_);
                    return false;
                }
                ++cur_;
                return true;
            }

            bool parse_string_raw(std::string &out, std::string &err)
            {
                if (!expect('"', err))
                    return false;
                out.clear();
                while (cur_ < end_)
                {
                    unsigned char c = static_cast<unsigned char>(*cur_++);
                    if (c == '"')
                        return true;
                    if (c == '\\')
                    {
                        if (cur_ >= end_)
                        {
                            err = "bad escape (truncated)";
                            return false;
                        }
                        char esc = *cur_++;
                        switch (esc)
                        {
                        case '"':
                            out.push_back('"');
                            break;
                        case '\\':
                            out.push_back('\\');
                            break;
                        case '/':
                            out.push_back('/');
                            break;
                        case 'b':
                            out.push_back('\b');
                            break;
                        case 'f':
                            out.push_back('\f');
                            break;
                        case 'n':
                            out.push_back('\n');
                            break;
                        case 'r':
                            out.push_back('\r');
                            break;
                        case 't':
                            out.push_back('\t');
                            break;
                        case 'u':
                        {
                            if (cur_ + 4 > end_)
                            {
                                err = "bad \\u escape";
                                return false;
                            }
                            unsigned code = 0;
                            for (int i = 0; i < 4; ++i)
                            {
                                char h = *cur_++;
                                code <<= 4;
                                if (h >= '0' && h <= '9')
                                    code |= unsigned(h - '0');
                                else if (h >= 'a' && h <= 'f')
                                    code |= unsigned(h - 'a' + 10);
                                else if (h >= 'A' && h <= 'F')
                                    code |= unsigned(h - 'A' + 10);
                                else
                                {
                                    err = "bad hex in \\u escape";
                                    return false;
                                }
                            }
                            // 高位代理项后必须紧跟一对低位代理项才能合成 BMP 外的码点
                            unsigned cp = code;
                            if (code >= 0xD800 && code <= 0xDBFF)
                            {
                                if (cur_ + 6 <= end_ && cur_[0] == '\\' && cur_[1] == 'u')
                                {
                                    unsigned low = 0;
                                    cur_ += 2;
                                    for (int i = 0; i < 4; ++i)
                                    {
                                        char h = *cur_++;
                                        low <<= 4;
                                        if (h >= '0' && h <= '9')
                                            low |= unsigned(h - '0');
                                        else if (h >= 'a' && h <= 'f')
                                            low |= unsigned(h - 'a' + 10);
                                        else if (h >= 'A' && h <= 'F')
                                            low |= unsigned(h - 'A' + 10);
                                        else
                                        {
                                            err = "bad surrogate";
                                            return false;
                                        }
                                    }
                                    cp = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                                }
                            }
                            if (cp < 0x80)
                            {
                                out.push_back(char(cp));
                            }
                            else if (cp < 0x800)
                            {
                                out.push_back(char(0xC0 | (cp >> 6)));
                                out.push_back(char(0x80 | (cp & 0x3F)));
                            }
                            else if (cp < 0x10000)
                            {
                                out.push_back(char(0xE0 | (cp >> 12)));
                                out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
                                out.push_back(char(0x80 | (cp & 0x3F)));
                            }
                            else
                            {
                                out.push_back(char(0xF0 | (cp >> 18)));
                                out.push_back(char(0x80 | ((cp >> 12) & 0x3F)));
                                out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
                                out.push_back(char(0x80 | (cp & 0x3F)));
                            }
                            break;
                        }
                        default:
                            err = std::string("bad escape \\") + esc;
                            return false;
                        }
                    }
                    else
                    {
                        out.push_back(char(c));
                    }
                }
                err = "unterminated string";
                return false;
            }

            bool parse_string_value(Value &out, std::string &err)
            {
                std::string s;
                if (!parse_string_raw(s, err))
                    return false;
                out = Value(std::move(s));
                return true;
            }

            bool parse_bool(Value &out, std::string &err)
            {
                if (cur_ + 4 <= end_ && std::memcmp(cur_, "true", 4) == 0)
                {
                    cur_ += 4;
                    out = Value(true);
                    return true;
                }
                if (cur_ + 5 <= end_ && std::memcmp(cur_, "false", 5) == 0)
                {
                    cur_ += 5;
                    out = Value(false);
                    return true;
                }
                err = "bad bool";
                return false;
            }

            bool parse_null(Value &out, std::string &err)
            {
                if (cur_ + 4 <= end_ && std::memcmp(cur_, "null", 4) == 0)
                {
                    cur_ += 4;
                    out = Value(nullptr);
                    return true;
                }
                err = "bad null";
                return false;
            }

            bool parse_number(Value &out, std::string &err)
            {
                const char *start = cur_;
                if (*cur_ == '-')
                    ++cur_;
                bool is_float = false;
                while (cur_ < end_)
                {
                    char c = *cur_;
                    if ((c >= '0' && c <= '9'))
                    {
                        ++cur_;
                    }
                    else if (c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-')
                    {
                        is_float = true;
                        ++cur_;
                    }
                    else
                        break;
                }
                std::string token(start, cur_ - start);
                try
                {
                    if (is_float)
                        out = Value(std::stod(token));
                    else
                        out = Value(int64_t(std::stoll(token)));
                }
                catch (...)
                {
                    err = "bad number: " + token;
                    return false;
                }
                return true;
            }

            bool parse_array(Value &out, std::string &err)
            {
                if (!expect('[', err))
                    return false;
                Array arr;
                skip_ws();
                if (cur_ < end_ && *cur_ == ']')
                {
                    ++cur_;
                    out = Value(std::move(arr));
                    return true;
                }
                while (true)
                {
                    skip_ws();
                    Value item;
                    if (!parse_value(item, err))
                        return false;
                    arr.push_back(std::move(item));
                    skip_ws();
                    if (cur_ >= end_)
                    {
                        err = "unterminated array";
                        return false;
                    }
                    if (*cur_ == ',')
                    {
                        ++cur_;
                        continue;
                    }
                    if (*cur_ == ']')
                    {
                        ++cur_;
                        out = Value(std::move(arr));
                        return true;
                    }
                    err = "expected ',' or ']'";
                    return false;
                }
            }

            bool parse_object(Value &out, std::string &err)
            {
                if (!expect('{', err))
                    return false;
                Object obj;
                skip_ws();
                if (cur_ < end_ && *cur_ == '}')
                {
                    ++cur_;
                    out = Value(std::move(obj));
                    return true;
                }
                while (true)
                {
                    skip_ws();
                    std::string key;
                    if (!parse_string_raw(key, err))
                        return false;
                    skip_ws();
                    if (!expect(':', err))
                        return false;
                    Value v;
                    if (!parse_value(v, err))
                        return false;
                    obj.emplace_back(std::move(key), std::move(v));
                    skip_ws();
                    if (cur_ >= end_)
                    {
                        err = "unterminated object";
                        return false;
                    }
                    if (*cur_ == ',')
                    {
                        ++cur_;
                        continue;
                    }
                    if (*cur_ == '}')
                    {
                        ++cur_;
                        out = Value(std::move(obj));
                        return true;
                    }
                    err = "expected ',' or '}'";
                    return false;
                }
            }
        };

        void escape_string(std::string &out, const std::string &s)
        {
            out.push_back('"');
            for (size_t i = 0; i < s.size(); ++i)
            {
                unsigned char c = static_cast<unsigned char>(s[i]);
                switch (c)
                {
                case '"':
                    out.append("\\\"");
                    break;
                case '\\':
                    out.append("\\\\");
                    break;
                case '\b':
                    out.append("\\b");
                    break;
                case '\f':
                    out.append("\\f");
                    break;
                case '\n':
                    out.append("\\n");
                    break;
                case '\r':
                    out.append("\\r");
                    break;
                case '\t':
                    out.append("\\t");
                    break;
                default:
                    if (c < 0x20)
                    {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                        out.append(buf);
                    }
                    else
                    {
                        out.push_back(char(c));
                    }
                }
            }
            out.push_back('"');
        }

        void write_value(std::string &out, const Value &v, int indent, int depth)
        {
            auto put_indent = [&](int d)
            {
                if (indent <= 0)
                    return;
                out.push_back('\n');
                out.append(size_t(d * indent), ' ');
            };

            switch (v.type())
            {
            case Type::Null:
                out.append("null");
                break;
            case Type::Bool:
                out.append(v.as_int() ? "true" : "false");
                break;
            case Type::Int:
                out.append(std::to_string(v.as_int()));
                break;
            case Type::Double:
            {
                std::ostringstream os;
                os << v.as_int();
                out.append(os.str());
                break;
            }
            case Type::String:
                escape_string(out, v.as_string());
                break;
            case Type::Array_:
            {
                const Array &a = v.as_array();
                if (a.empty())
                {
                    out.append("[]");
                    break;
                }
                out.push_back('[');
                for (size_t i = 0; i < a.size(); ++i)
                {
                    put_indent(depth + 1);
                    write_value(out, a[i], indent, depth + 1);
                    if (i + 1 < a.size())
                        out.push_back(',');
                }
                put_indent(depth);
                out.push_back(']');
                break;
            }
            case Type::Object_:
            {
                const Object &o = v.as_object();
                if (o.empty())
                {
                    out.append("{}");
                    break;
                }
                out.push_back('{');
                for (size_t i = 0; i < o.size(); ++i)
                {
                    put_indent(depth + 1);
                    escape_string(out, o[i].first);
                    out.append(indent > 0 ? ": " : ":");
                    write_value(out, o[i].second, indent, depth + 1);
                    if (i + 1 < o.size())
                        out.push_back(',');
                }
                put_indent(depth);
                out.push_back('}');
                break;
            }
            }
        }

    } // namespace

    bool parse(const std::string &utf8, Value &out, std::string &err)
    {
        Parser p(utf8);
        if (!p.parse_value(out, err))
            return false;
        if (!p.eof_ok())
        {
            err = "trailing content after JSON value";
            return false;
        }
        return true;
    }

    std::string dump(const Value &v, int indent)
    {
        std::string out;
        out.reserve(1024);
        write_value(out, v, indent, 0);
        return out;
    }

} // namespace sp::json
