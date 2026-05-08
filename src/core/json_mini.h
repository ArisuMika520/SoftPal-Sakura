// 仅覆盖脚本导入/导出所需的最小 JSON 实现，不追求完整规范一致
#ifndef SOFTPAL_CORE_JSON_MINI_H
#define SOFTPAL_CORE_JSON_MINI_H

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace sp::json
{

    class Value;
    // 顺序保留型对象，与 Python 3.7+ dict 行为一致；脚本导出依赖键的稳定顺序
    using Object = std::vector<std::pair<std::string, Value>>;
    using Array = std::vector<Value>;

    enum class Type
    {
        Null,
        Bool,
        Int,
        Double,
        String,
        Array_,
        Object_
    };

    class Value
    {
    public:
        Value() : type_(Type::Null) {}
        Value(std::nullptr_t) : type_(Type::Null) {}
        Value(bool b) : type_(Type::Bool), bool_(b) {}
        Value(int v) : type_(Type::Int), int_(v) {}
        Value(int64_t v) : type_(Type::Int), int_(v) {}
        Value(uint32_t v) : type_(Type::Int), int_(v) {}
        Value(double v) : type_(Type::Double), dbl_(v) {}
        Value(std::string s) : type_(Type::String), str_(std::move(s)) {}
        Value(const char *s) : type_(Type::String), str_(s ? s : "") {}
        Value(Array a) : type_(Type::Array_), arr_(std::make_shared<Array>(std::move(a))) {}
        Value(Object o) : type_(Type::Object_), obj_(std::make_shared<Object>(std::move(o))) {}

        Type type() const { return type_; }
        bool is_null() const { return type_ == Type::Null; }
        bool is_int() const { return type_ == Type::Int; }
        bool is_string() const { return type_ == Type::String; }
        bool is_array() const { return type_ == Type::Array_; }
        bool is_object() const { return type_ == Type::Object_; }

        int64_t as_int() const { return int_; }
        const std::string &as_string() const { return str_; }
        const Array &as_array() const { return *arr_; }
        Array &as_array() { return *arr_; }
        const Object &as_object() const { return *obj_; }
        Object &as_object() { return *obj_; }

        const Value *find(const std::string &key) const
        {
            if (type_ != Type::Object_)
                return nullptr;
            for (const auto &kv : *obj_)
            {
                if (kv.first == key)
                    return &kv.second;
            }
            return nullptr;
        }

    private:
        Type type_;
        bool bool_ = false;
        int64_t int_ = 0;
        double dbl_ = 0;
        std::string str_;
        std::shared_ptr<Array> arr_;
        std::shared_ptr<Object> obj_;
    };

    bool parse(const std::string &utf8, Value &out, std::string &err);

    // indent=0 输出紧凑无空白，>0 为每级缩进的空格数
    std::string dump(const Value &v, int indent = 0);

} // namespace sp::json

#endif // SOFTPAL_CORE_JSON_MINI_H
