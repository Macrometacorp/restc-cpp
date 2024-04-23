#pragma once

#include <assert.h>
#include <deque>
#include <iostream>
#include <map>
#include <set>
#include <stack>
#include <type_traits>
#include <optional>

#include "restc-cpp/RapidJsonReader.h"
#include "restc-cpp/RapidJsonWriter.h"
#include "restc-cpp/error.h"
#include "restc-cpp/internals/for_each_member.hpp"
#include "restc-cpp/logging.h"
#include "restc-cpp/restc-cpp.h"
#include "restc-cpp/typename.h"
#include <boost/fusion/adapted/struct/define_struct.hpp>
#include <boost/fusion/algorithm/iteration/for_each.hpp>
#include <boost/fusion/algorithm/transformation/transform.hpp>
#include <boost/fusion/algorithm/transformation/zip.hpp>
#include <boost/fusion/container.hpp>
#include <boost/fusion/include/is_sequence.hpp>
#include <boost/fusion/sequence/intrinsic/at.hpp>
#include <boost/fusion/sequence/intrinsic/at_c.hpp>
#include <boost/fusion/sequence/intrinsic/segments.hpp>
#include <boost/fusion/sequence/intrinsic/size.hpp>
#include <boost/fusion/support/is_sequence.hpp>
#include <boost/iterator/function_input_iterator.hpp>
#include <boost/mpl/contains.hpp>
#include <boost/mpl/range_c.hpp>
#include <boost/mpl/vector.hpp>

#include "rapidjson/writer.h"
#include "rapidjson/istreamwrapper.h"
#include <rapidjson/ostreamwrapper.h>

namespace restc_cpp {

using excluded_names_t = std::set<std::string>;

/*! Mapping between C++ property names and json names.
*
* Normally we will use the same, but in some cases we
* need to appy mapping.
*/
struct JsonFieldMapping {
    struct entry {
        entry() = default;
        entry(const std::string& native, const std::string& json)
        : native_name{native}, json_name{json} {}

        std::string native_name;
        std::string json_name;
    };
    std::vector<entry> entries;

    JsonFieldMapping() = default;
    JsonFieldMapping(JsonFieldMapping&&) = default;
    JsonFieldMapping(const JsonFieldMapping&) = default;
    JsonFieldMapping(const std::initializer_list<entry>& entries)
        : entries{entries}
    {}

    const std::string&
    to_json_name(const std::string& name) const noexcept {
        for(const auto& entry : entries) {
            if (name.compare(entry.native_name) == 0) {
                return entry.json_name;
            }
        }
        return name;
    }

    const std::string&
    to_native_name(const std::string& name) const noexcept {
        for(const auto& entry : entries) {
            if (name.compare(entry.json_name) == 0) {
                return entry.native_name;
            }
        }
        return name;
    }
};

/*! Base class that satisfies the requirements from rapidjson */
class RapidJsonDeserializerBase {
public:
    enum class State { INIT, IN_OBJECT, IN_ARRAY, RECURSED, DONE };

    static std::string to_string(const State& state) {
        const static std::array<std::string, 5> states =
            {"INIT", "IN_OBJECT", "IN_ARRAY", "RECURSED", "DONE"};
        return states.at(static_cast<int>(state));
    }

    RapidJsonDeserializerBase(RapidJsonDeserializerBase *parent)
    : parent_{parent} {}

    virtual ~RapidJsonDeserializerBase()
    {
    }

    virtual void Push(const std::shared_ptr<RapidJsonDeserializerBase>& handler) {
        parent_->Push(handler);
    }

    virtual void Pop() {
        parent_->Pop();
    }

    // Outer interface
        virtual bool Null() = 0;

        virtual bool Bool(bool b) = 0;

        virtual bool Int(int i) = 0;

        virtual bool Uint(unsigned u) = 0;

        virtual bool Int64(int64_t i) = 0;

        virtual bool Uint64(uint64_t u) = 0;

        virtual bool Double(double d) = 0;

        virtual bool String(const char* str, std::size_t Tlength, bool copy) = 0;

        virtual bool RawNumber(const char* str, std::size_t length, bool copy) = 0;

        virtual bool StartObject() = 0;

        virtual bool Key(const char* str, std::size_t length, bool copy) = 0;

        virtual bool EndObject(std::size_t memberCount) = 0;

        virtual bool StartArray() = 0;

        virtual bool EndArray(std::size_t elementCount) = 0;

    virtual void OnChildIsDone() {};
    RapidJsonDeserializerBase& GetParent() {
        assert(parent_ != nullptr);
        return *parent_;
    }
    bool HaveParent() const noexcept {
        return parent_ != nullptr;
    }

private:
    RapidJsonDeserializerBase *parent_;
};


struct SerializeProperties {
    SerializeProperties()
    : max_memory_consumption{GetDefaultMaxMemoryConsumption()}
    , ignore_empty_fileds{true}
    , ignore_unknown_properties{true}
    {}

    explicit SerializeProperties(const bool ignoreEmptyFields)
    : max_memory_consumption{GetDefaultMaxMemoryConsumption()}
    , ignore_empty_fileds{ignoreEmptyFields}
    , ignore_unknown_properties{true}
    {}

    uint64_t max_memory_consumption : 32;
    uint64_t ignore_empty_fileds : 1;
    uint64_t ignore_unknown_properties : 1;

    const std::set<std::string> *excluded_names = nullptr;
    const JsonFieldMapping *name_mapping = nullptr;

    constexpr static uint64_t GetDefaultMaxMemoryConsumption() { return 1024 * 1024; }

    bool is_excluded(const std::string& name) const noexcept {
        return excluded_names
            && excluded_names->find(name) != excluded_names->end();
    }

    const std::string& map_name_to_json(const std::string& name) const noexcept {
        if (name_mapping == nullptr)
            return name;
        return name_mapping->to_json_name(name);
    }

    int64_t GetMaxMemoryConsumption() const noexcept {
        return static_cast<int64_t>(max_memory_consumption);
    }

    void SetMaxMemoryConsumption(std::int64_t val) {
        if ((val < 0) || (val > 0xffffffffL)) {
            throw ConstraintException("Memory contraint value is out of limit");
        }
        max_memory_consumption = static_cast<uint32_t>(val);
    }
};

using serialize_properties_t = SerializeProperties;

namespace {

template <typename T>
struct type_conv
{
    using const_field_type_t = const T;
    using native_field_type_t = typename std::remove_const<typename std::remove_reference<const_field_type_t>::type>::type;
    using noconst_ref_type_t = typename std::add_lvalue_reference<native_field_type_t>::type;
};

template <typename T, typename fnT>
struct on_name_and_value
{
    on_name_and_value(fnT fn)
    : fn_{fn}
    {
    }

    template <typename valT>
    void operator () (const char* name, const valT& val) const {
        fn_(name, val);
    }

    void for_each_member(const T& instance) {
        restc_cpp::for_each_member(instance, *this);
    }

private:
    fnT fn_;
};

template <typename T>
size_t get_len(const T& v) {
    if constexpr (std::is_same_v<std::string, T>) {
        const auto ptrlen = sizeof(const char *);
        size_t b = sizeof(v);
        if (v.size() > (sizeof(std::string) + ptrlen)) {
            b += v.size() - ptrlen;
        }
        return b;
    }

    return sizeof(v);
};

template <typename T>
struct is_optional {
    constexpr static const bool value = false;
};

template <typename T>
struct is_optional<std::optional<T> > {
    constexpr static const bool value = true;
};

template <typename T>
constexpr auto has_std_to_string_implementation(int) -> decltype (std::is_same_v<std::string,decltype(::std::to_string(std::declval<T>()))>) {
    return true;
}

template <typename T>
constexpr auto has_std_to_string_implementation(float) -> bool {
    return false;
}

template <typename T, typename U = int>
struct has_empty_method : std::false_type { };

// Specialization for U = int
template <typename T>
struct has_empty_method <T, decltype((void) T::empty(), 0)> : std::true_type { };

template <typename T, typename U = int>
struct has_reset_method : std::false_type { };

// Specialization for U = int
template <typename T>
struct has_reset_method <T, decltype((void) T::reset(), 0)> : std::true_type { };

template <typename T>
bool is_digits_only(const T& vect, bool signedFlag) {
    size_t cnt = 0;
    for(auto v: vect) {
        if (signedFlag && ++cnt == 1 && v == '-') {
            continue;
        }
        if (v < '0' || v > '9') {
            return false;
        }
    }
    return true;
}

template <typename varT, typename valT>
void assign_value(varT& var, const valT& val) {
    if constexpr (std::is_same_v<valT, std::nullptr_t>) {
        if constexpr (is_optional<varT>::value) {
            var.reset();
        } else if constexpr (has_reset_method<varT>()) {
            var.reset();
        } else if constexpr (std::is_pointer_v<varT>) {
            var = nullptr;
        } else if constexpr (std::is_default_constructible_v<valT>) {
            var = {};
        }
        // If none of the above is true, just ignore it...
        RESTC_CPP_LOG_TRACE_("assign_value: " << "Don't know how to assign null-value/empty value to type " << RESTC_CPP_TYPENAME(varT));
    }
    else if constexpr (std::is_assignable_v<varT, valT>) {
        var = val;
    } else if constexpr (std::is_same_v<std::string, varT>) {
        var = std::to_string(val);
    } else if constexpr (std::is_arithmetic<varT>::value
            && std::is_arithmetic<valT>::value) {
        var = static_cast<varT>(val);
    } else if constexpr (std::is_same_v<bool, varT>) {
        if constexpr (std::is_same_v<std::string, valT>) {
            if (val == "true" || val == "yes" || atoi(val.c_str()) > 0) {
                var = true;
            } else if (val.empty() || val == "false" || val == "no" || (val.at(0) == '0' && atoi(val.c_str()) == 0)) {
                var = false;
            } else {
                throw ParseException("assign_value: Invalid data conversion from string to bool");
            }
        } else if constexpr (std::is_integral_v<valT>) {
            var = val != 0;
        } else {
           throw ParseException("assign_value: Invalid data conversion to bool");
        }
    } else if constexpr ((std::is_same_v<varT, int8_t>
            || std::is_same_v<varT, int16_t>
            || std::is_same_v<varT, int16_t>
            || std::is_same_v<varT, int32_t>
            || std::is_same_v<varT, int64_t>
            || std::is_same_v<varT, int>
            ) && std::is_same_v<std::string, valT>) {
        if (is_digits_only(val, true)) {
            var = static_cast<varT>(std::stoll(val));
        } else {
            throw ParseException("assign_value: Invalid data conversion from string to int*_t");
        }
    } else if constexpr ((std::is_same_v<varT, uint8_t>
            || std::is_same_v<varT, uint16_t>
            || std::is_same_v<varT, uint32_t>
            || std::is_same_v<varT, uint64_t>
            || std::is_same_v<varT, size_t>
            || std::is_same_v<varT, unsigned int>
            ) && std::is_same_v<std::string, valT>) {
        if (is_digits_only(val, false)) {
            var = static_cast<varT>(std::stoull(val));
        } else {
            throw ParseException("assign_value: Invalid data conversion from string to uint*_t");
        }
    } else {
        throw ParseException("assign_value: Invalid data conversion");
    }
}

template <typename T>
struct is_map {
    constexpr static const bool value = false;
};

template <typename T, typename CompareT, typename AllocT>
struct is_map<std::map<std::string, T, CompareT, AllocT> > {
    constexpr static const bool value = true;
};

template <typename T>
struct is_container {
    constexpr static const bool value = false;
};

template <typename T,typename Alloc>
struct is_container<std::vector<T,Alloc> > {
    constexpr static const bool value = true;
};

template <typename T,typename Alloc>
struct is_container<std::list<T,Alloc> > {
    constexpr static const bool value = true;
    using data_t = T;
};

template <typename T,typename Alloc>
struct is_container<std::deque<T,Alloc> > {
    constexpr static const bool value = true;
    using data_t = T;
};

class RapidJsonSkipObject :  public RapidJsonDeserializerBase {
public:

    RapidJsonSkipObject(RapidJsonDeserializerBase *parent)
    : RapidJsonDeserializerBase(parent)
    {
    }

    bool Null() override {
        return true;
    }

    bool Bool(bool b) override {
        return true;
    }

    bool Int(int) override {
        return true;
    }

    bool Uint(unsigned) override {
        return true;
    }

    bool Int64(int64_t) override {
        return true;
    }

    bool Uint64(uint64_t) override {
        return true;
    }

    bool Double(double) override {
        return true;
    }

    bool String(const char*, std::size_t Tlength, bool) override {
        return true;
    }

    bool RawNumber(const char*, std::size_t, bool) override {
        return true;
    }

    bool StartObject() override {
#ifdef RESTC_CPP_LOG_JSON_SERIALIZATION
        RESTC_CPP_LOG_TRACE_("   Skipping json: StartObject()");
#endif
        ++recursion_;
        return true;
    }

    bool Key(const char* str, std::size_t length, bool copy) override {
#ifdef RESTC_CPP_LOG_JSON_SERIALIZATION
        RESTC_CPP_LOG_TRACE_("   Skipping json key: "
            << boost::string_ref(str, length));
#endif
        return true;
    }

    bool EndObject(std::size_t memberCount) override {
#ifdef RESTC_CPP_LOG_JSON_SERIALIZATION
        RESTC_CPP_LOG_TRACE_("   Skipping json: EndObject()");
#endif
        if (--recursion_ <= 0) {
            if (HaveParent()) {
                GetParent().OnChildIsDone();
            }
        }
        return true;
    }

    bool StartArray() override {
#ifdef RESTC_CPP_LOG_JSON_SERIALIZATION
        RESTC_CPP_LOG_TRACE_("   Skipping json: StartArray()");
#endif
        ++recursion_;
        return true;
    }

    bool EndArray(std::size_t elementCount) override {
#ifdef RESTC_CPP_LOG_JSON_SERIALIZATION
        RESTC_CPP_LOG_TRACE_("   Skipping json: EndArray()");
#endif
        if (--recursion_ <= 0) {
            if (HaveParent()) {
                GetParent().OnChildIsDone();
            }
        }
        return true;
    }

private:
    int recursion_ = 0;
};

//template <typename T, typename U = int>
//struct has_value_type : std::false_type { };

//// Specialization for U = int
//template <typename T>
//struct has_value_type <T, decltype((void) T::value_type, 0)> : std::true_type { };

} // anonymous namespace

template <typename T>
class RapidJsonDeserializer : public RapidJsonDeserializerBase {
public:
    using data_t = typename std::remove_const<typename std::remove_reference<T>::type>::type;
    static constexpr int const default_mem_limit = 1024 * 1024 * 1024;

    RapidJsonDeserializer(data_t& object)
    : RapidJsonDeserializerBase(nullptr)
    , object_{object}
    , properties_buffer_{std::make_unique<serialize_properties_t>()}
    , properties_{*properties_buffer_}
    , bytes_buffer_{properties_.GetMaxMemoryConsumption()}
    , bytes_{bytes_buffer_ ? &bytes_buffer_ : nullptr}
    {}

    explicit RapidJsonDeserializer(data_t& object, const serialize_properties_t& properties)
    : RapidJsonDeserializerBase(nullptr)
    , object_{object}
    , properties_{properties}
    , bytes_buffer_{properties_.GetMaxMemoryConsumption()}
    , bytes_{bytes_buffer_ ? &bytes_buffer_ : nullptr}
    {}

    explicit RapidJsonDeserializer(data_t& object,
                        RapidJsonDeserializerBase *parent,
                        const serialize_properties_t& properties,
                        std::int64_t *maxBytes)
    : RapidJsonDeserializerBase(parent)
    , object_{object}
    , properties_{properties}
    , bytes_{maxBytes}
    {
        assert(parent != nullptr);
    }

    bool Null() override {
        assert(((state_ == State::RECURSED) && recursed_to_) || !recursed_to_);
        return recursed_to_ ? recursed_to_->Null() : DoNull();
    }

    bool Bool(bool b) override {
        assert(((state_ == State::RECURSED) && recursed_to_) || !recursed_to_);
        return recursed_to_ ? recursed_to_->Bool(b) : DoBool(b);
    }

    bool Int(int i) override {
        assert(((state_ == State::RECURSED) && recursed_to_) || !recursed_to_);
        return recursed_to_ ? recursed_to_->Int(i) : DoInt(i);
    }

    bool Uint(unsigned u) override {
        assert(((state_ == State::RECURSED) && recursed_to_) || !recursed_to_);
        return recursed_to_ ? recursed_to_->Uint(u) : DoUint(u);
    }

    bool Int64(int64_t i) override {
        assert(((state_ == State::RECURSED) && recursed_to_) || !recursed_to_);
        return recursed_to_ ? recursed_to_->Int64(i) : DoInt64(i);
    }

    bool Uint64(uint64_t u) override {
        assert(((state_ == State::RECURSED) && recursed_to_) || !recursed_to_);
        return recursed_to_ ? recursed_to_->Uint64(u) : DoUint64(u);
    }

    bool Double(double d) override {
        assert(((state_ == State::RECURSED) && recursed_to_) || !recursed_to_);
        return recursed_to_ ? recursed_to_->Double(d) : DoDouble(d);
    }

    bool String(const char* str, std::size_t length, bool copy) override {
        assert(((state_ == State::RECURSED) && recursed_to_) || !recursed_to_);
        return recursed_to_
            ? recursed_to_->String(str, length, copy)
            : DoString(str, length, copy);
    }

    bool RawNumber(const char* str, std::size_t length, bool copy) override {
        assert(((state_ == State::RECURSED) && recursed_to_) || !recursed_to_);
        assert(false);
        return true;
    }

    bool StartObject() override {
        assert(((state_ == State::RECURSED) && recursed_to_) || !recursed_to_);
        return recursed_to_ ? recursed_to_->StartObject() : DoStartObject();
    }

    bool Key(const char* str, std::size_t length, bool copy) override {
        assert(((state_ == State::RECURSED) && recursed_to_) || !recursed_to_);
        return recursed_to_
            ? recursed_to_->Key(str, length, copy)
            : DoKey(str, length, copy);
    }

    bool EndObject(std::size_t memberCount) override {
        assert(((state_ == State::RECURSED) && recursed_to_) || !recursed_to_);
        return recursed_to_
            ? recursed_to_->EndObject(memberCount)
            : DoEndObject(memberCount);
    }

    bool StartArray() override {
        assert(((state_ == State::RECURSED) && recursed_to_) || !recursed_to_);
        return recursed_to_ ? recursed_to_->StartArray() : DoStartArray();
    }

    bool EndArray(std::size_t elementCount) override {
        assert(((state_ == State::RECURSED) && recursed_to_) || !recursed_to_);
        return recursed_to_
            ? recursed_to_->EndArray(elementCount)
            : DoEndArray(elementCount);
    }

private:
    template <typename typeT, typename itemT>
    void DoRecurseToMember(itemT& item) {
        using native_field_type_t = typename std::remove_const<typename std::remove_reference<itemT>::type>::type;
        if constexpr (is_optional<native_field_type_t>::value) {
            using val_t = typename std::remove_const<typename native_field_type_t::value_type>::type;
            const_cast<native_field_type_t &>(item) = val_t{};
            //DoRecurseToMember<native_field_type_t>(item.value());
            DoRecurseToMember<typename std::remove_const<typename std::remove_reference<decltype(item.value())>::type>::type>(item.value());
        } else if constexpr (boost::fusion::traits::is_sequence<typeT>::value
                      || is_container<typeT>::value
                      || is_map<typeT>::value) {
             //using const_field_type_t = decltype(item);
             //using native_field_type_t = typename std::remove_const<typename std::remove_reference<const_field_type_t>::type>::type;
             using field_type_t = typename std::add_lvalue_reference<native_field_type_t>::type;

             auto& value = const_cast<field_type_t&>(item);

             recursed_to_ = std::make_unique<RapidJsonDeserializer<field_type_t>>(
                 value, this, properties_, bytes_);
        } else {
             throw ParseException(std::string{"DoRecurseToMember: Unexpected type: "} +
                                  RESTC_CPP_TYPENAME(itemT) + " to " + current_name_);
        }
    }

    template <typename dataT>
    void RecurseToContainerValue() {
        if constexpr (is_container<dataT>::value) {
             if constexpr (boost::fusion::traits::is_sequence<typename dataT::value_type>::value
                           || is_container<typename dataT::value_type>::value) {
                 object_.push_back({});

                 using native_type_t = typename std::remove_const<
                     typename std::remove_reference<typename dataT::value_type>::type>::type;
                 recursed_to_ = std::make_unique<RapidJsonDeserializer<native_type_t>>(
                     object_.back(), this, properties_, bytes_);
                 saved_state_.push(state_);
                 state_ = State::RECURSED;
                 return;
            }

            // Do nothing. We will push_back() the values as they arrive
        }

        throw ParseException(std::string{"RecurseToContainerValue: Unexpected type: "} +
                             RESTC_CPP_TYPENAME(dataT));
    }


    template <typename dataT>
    void RecurseToMember() {
        assert(!recursed_to_);

        if constexpr (boost::fusion::traits::is_sequence<dataT>::value
                   && !is_map<dataT>::value) {
             assert(!current_name_.empty());
             bool found = false;
             auto fn = [&](const char *name, auto& val) {
                 if (found) {
                     return;
                 }

                 if (strcmp(name, current_name_.c_str()) == 0) {
                     using const_field_type_t = decltype(val);
                     using native_field_type_t = typename std::remove_const<typename std::remove_reference<const_field_type_t>::type>::type;
                     DoRecurseToMember<native_field_type_t>(val);
                     found = true;
                 }
             };

             on_name_and_value<decltype(object_), decltype(fn)> handler(fn);
             handler.for_each_member(object_);

             if (!recursed_to_) {
                 assert(!found);
                 RESTC_CPP_LOG_TRACE_("RecurseToMember(): Failed to find property-name '"
                     << current_name_
                     << "' in C++ class '" << RESTC_CPP_TYPENAME(dataT)
                     << "' when serializing from JSON.");

                 if (!properties_.ignore_unknown_properties) {
                     throw UnknownPropertyException(current_name_);
                 } else {
                     recursed_to_ = std::make_unique<RapidJsonSkipObject>(this);
                 }
             } else {
                 assert(found);
             }

             assert(recursed_to_);
             saved_state_.push(state_);
             state_ = State::RECURSED;
             current_name_.clear();
        } else if constexpr (is_map<dataT>::value) {
             using native_type_t = typename std::remove_const<
                 typename std::remove_reference<typename dataT::mapped_type>::type>::type;
             recursed_to_ = std::make_unique<RapidJsonDeserializer<native_type_t>>(
                 object_[current_name_], this, properties_, bytes_);
             saved_state_.push(state_);
             state_ = State::RECURSED;
             current_name_.clear();
        } else {
             throw ParseException(std::string{"RecurseToMember: Unexpected type: "} +
                                  RESTC_CPP_TYPENAME(dataT) + " to " + current_name_);
        }
    }

    template<typename dataT, typename argT>
    bool SetValueOnMember(const argT& val) {

        if constexpr (boost::fusion::traits::is_sequence<dataT>::value) {
             assert(!current_name_.empty());

             bool found = false;

             auto fn = [&](const char *name, auto& v) {
                 if (found) {
                     return;
                 }

                 if (strcmp(name, current_name_.c_str()) == 0) {

                     const auto& new_value_ = val;

                     using const_field_type_t = decltype(v);
                     using native_field_type_t = typename std::remove_const<
                         typename std::remove_reference<const_field_type_t>::type>::type;
                     using field_type_t = typename std::add_lvalue_reference<native_field_type_t>::type;

                     auto& value = const_cast<field_type_t&>(v);

                     this->AddBytes(get_len(new_value_));
                     assign_value(value, new_value_);
                     found = true;
                 }
             };

             on_name_and_value<dataT, decltype(fn)> handler(fn);
             handler.for_each_member(object_);

             if (!found) {
                 assert(!found);
                 RESTC_CPP_LOG_TRACE_("SetValueOnMember(): Failed to find property-name '"
                     << current_name_
                     << "' in C++ class '" << RESTC_CPP_TYPENAME(dataT)
                     << "' when serializing from JSON.");

                 if (!properties_.ignore_unknown_properties) {
                     throw UnknownPropertyException(current_name_);
                 }
             }

             current_name_.clear();
             return true;
        }

        if constexpr (is_map<dataT>::value) {
             assert(!current_name_.empty());
             AddBytes(
                 get_len(val)
                 + sizeof(std::string)
                 + current_name_.size()
                 + sizeof(size_t) * 6 // FIXME: Find approximate average overhead for map
             );

             assign_value(object_[current_name_], val);
             current_name_.clear();
             return true;
        }

        throw ParseException(std::string{"SetValueOnMember: Unexpected type: "} +
                          RESTC_CPP_TYPENAME(dataT) + " on " + current_name_);
    }

    template<typename dataT, typename argT>
    void SetValueInArray(const argT& val) {
        if constexpr (is_container<dataT>::value) {
             if constexpr (!boost::fusion::traits::is_sequence<typename dataT::value_type>::value) {
                 AddBytes(
                     get_len(val)
                     + sizeof(size_t) * 3 // Approximate average overhead for container
                 );

                 object_.push_back({});
                 assign_value<decltype(object_.back()), decltype(val)>(object_.back(), val);
                 } else {
                 // We should always recurse into structs
                 assert(false);
            }
        } else {
             throw ParseException(std::string{"SetValueInArray: Unexpected type: "} +
                               RESTC_CPP_TYPENAME(argT) + " on " + current_name_);
        }
    }

    template<typename argT>
    bool SetValue(argT val) {
#ifdef RESTC_CPP_LOG_JSON_SERIALIZATION
        RESTC_CPP_LOG_TRACE_(RESTC_CPP_TYPENAME(data_t)
            << " SetValue: " << current_name_
            << " State: " << to_string(state_));
#endif
        if (state_ == State::IN_OBJECT) {
            return SetValueOnMember<data_t>(val);
        } else if (state_ == State::IN_ARRAY) {
            SetValueInArray<data_t>(val);
            return true;
        }
        RESTC_CPP_LOG_ERROR_(RESTC_CPP_TYPENAME(data_t)
            << " SetValue: " << current_name_
            << " State: " << to_string(state_)
            << " Value type" << RESTC_CPP_TYPENAME(argT));

        assert(false && "Invalid state for setting a value");
        return true;
    }

    bool DoNull() {
        return SetValue(nullptr);
    }

    bool DoBool(bool b) {
        return SetValue(b);
    }

    bool DoInt(int i) {
        return SetValue(i);
    }

    bool DoUint(unsigned u) {
        return SetValue(u);
    }

    bool DoInt64(int64_t i) {
        return SetValue(i);
    }

    bool DoUint64(uint64_t u) {
        return SetValue(u);
    }

    bool DoDouble(double d) {
        return SetValue(d);
    }

    bool DoString(const char* str, std::size_t length, bool copy) {
        return SetValue(std::string(str, length));
    }

    bool DoRawNumber(const char* str, std::size_t length, bool copy) {
        assert(false);
        return false;
    }

    bool DoStartObject() {
#ifdef RESTC_CPP_LOG_JSON_SERIALIZATION
        const bool i_am_a_map = is_map<data_t>::value;
        RESTC_CPP_LOG_TRACE_(RESTC_CPP_TYPENAME(data_t)
            << " DoStartObject: " << current_name_
            << " i_am_a_map: " << i_am_a_map);
#endif
        switch (state_) {

            case State::INIT:
                state_ = State::IN_OBJECT;
                break;
            case State::IN_OBJECT:
                RecurseToMember<data_t>();
                recursed_to_->StartObject();
                break;
            case State::IN_ARRAY:
                RecurseToContainerValue<data_t>();

                // If this fails, you probably need to declare the data-type in the array.
                // with BOOST_FUSION_ADAPT_STRUCT or friends.
                assert(recursed_to_);
                recursed_to_->StartObject();
                break;
            case State::DONE:
                RESTC_CPP_LOG_TRACE_("Re-using instance of RapidJsonDeserializer");
                state_ = State::IN_OBJECT;
                if (bytes_) {
                    *bytes_ = properties_.GetMaxMemoryConsumption();
                }
                break;
            default:
                assert(false && "Unexpected state");

        }
        return true;
    }

    bool DoKey(const char* str, std::size_t length, bool copy) {
        assert(current_name_.empty());

        if (properties_.name_mapping == nullptr) {
            current_name_.assign(str, length);
        } else {
            std::string name{str, length};
            current_name_ = properties_.name_mapping->to_native_name(name);
        }
#ifdef RESTC_CPP_LOG_JSON_SERIALIZATION
        RESTC_CPP_LOG_TRACE_(RESTC_CPP_TYPENAME(data_t)
            << " DoKey: " << current_name_);
#endif
        return true;
    }

    bool DoEndObject(std::size_t memberCount) {
#ifdef RESTC_CPP_LOG_JSON_SERIALIZATION
        RESTC_CPP_LOG_TRACE_(RESTC_CPP_TYPENAME(data_t)
            << " DoEndObject: " << current_name_);
#endif
        current_name_.clear();

        switch (state_) {
            case State::IN_OBJECT:
                state_ = State::DONE;
                break;
            case State::IN_ARRAY:
                assert(false); // FIXME?
                break;
            default:
                assert(false && "Unexpected state");

        }

        if (state_ == State::DONE) {
            if (HaveParent()) {
                GetParent().OnChildIsDone();
            }
        }
        return true;
    }

    bool DoStartArray() {
#ifdef RESTC_CPP_LOG_JSON_SERIALIZATION
        RESTC_CPP_LOG_TRACE_(RESTC_CPP_TYPENAME(data_t)
            << " DoStartArray: " << current_name_);
#endif
        switch (state_) {
            case State::INIT:
                state_ = State::IN_ARRAY;
                break;
            case State::IN_ARRAY:
                RecurseToContainerValue<data_t>();
                recursed_to_->StartArray();
                break;
            case State::IN_OBJECT:
                RecurseToMember<data_t>();
                recursed_to_->StartArray();
                break;
            default:
                assert(false && "Unexpected state");
        }

        return true;
    }

    bool DoEndArray(std::size_t elementCount) {
#ifdef RESTC_CPP_LOG_JSON_SERIALIZATION
        RESTC_CPP_LOG_TRACE_(RESTC_CPP_TYPENAME(data_t)
            << " DoEndArray: " << current_name_);
#endif
        current_name_.clear();

        switch (state_) {
            case State::IN_OBJECT:
                assert(false); // FIXME?
                break;
            case State::IN_ARRAY:
                state_ = State::DONE;
                break;
            default:
                assert(false && "Unexpected state");

        }

        if (state_ == State::DONE) {
            if (HaveParent()) {
                GetParent().OnChildIsDone();
            }
        }
        return true;
    }

    void OnChildIsDone() override {
#ifdef RESTC_CPP_LOG_JSON_SERIALIZATION
        RESTC_CPP_LOG_TRACE_(RESTC_CPP_TYPENAME(data_t)
            << " OnChildIsDone");
#endif
        assert(state_ == State::RECURSED);
        assert(!saved_state_.empty());

        state_ = saved_state_.top();
        saved_state_.pop();
        recursed_to_.reset();
    }

    void AddBytes(size_t bytes) {
        if (!bytes_) {
            return;
        }
        static const std::string oom{"Exceed memory usage constraint"};
        *bytes_ -= bytes;
        if (*bytes_ <= 0) {
            throw ConstraintException(oom);
        }
    }

private:
    data_t& object_;

    // The root objects owns the counter.
    // Child objects gets the bytes_ pointer in the constructor.
    std::unique_ptr<serialize_properties_t> properties_buffer_;
    const serialize_properties_t& properties_;
    std::int64_t bytes_buffer_ = {};
    std::int64_t *bytes_ = &bytes_buffer_;

    //const JsonFieldMapping *name_mapping_ = nullptr;
    std::string current_name_;
    State state_ = State::INIT;
    std::stack<State> saved_state_;
    std::unique_ptr<RapidJsonDeserializerBase> recursed_to_;
};


namespace {


template <typename T>
constexpr bool is_empty_field_(const T& value) {
    if constexpr (std::is_integral<T>::value || std::is_floating_point<T>::value) {
        return value == T{};
    }

    if constexpr (is_optional<T>::value) {
        return !value.has_value();
    }

    // Could produce unexpected results for types that implement empty() with
    // another meaning than the C++ standard library
    if constexpr //(has_empty_method<T>()) {
        (std::is_same_v<std::string, T>
            || is_map<T>::value
            || is_container<T>::value) {
        return value.empty();
    }

    return false;
}

template <typename T>
constexpr bool is_empty_field(T&& value) {
    using data_type = typename std::remove_const<typename std::remove_reference<T>::type>::type;
    return is_empty_field_<data_type>(value);
}

template <typename T, typename S>
void do_serialize_integral(const T& v, S& serializer) {
    assert(false);
}

template <typename S>
void do_serialize_integral(const bool& v, S& serializer) {
    serializer.Bool(v);
}

template <typename S>
void do_serialize_integral(const char& v, S& serializer) {
    serializer.Int(v);
}

template <typename S>
void do_serialize_integral(const int& v, S& serializer) {
    serializer.Int(v);
}

template <typename S>
void do_serialize_integral(const unsigned char& v, S& serializer) {
    serializer.Uint(v);
}


template <typename S>
void do_serialize_integral(const unsigned int& v, S& serializer) {
    serializer.Uint(v);
}

template <typename S>
void do_serialize_integral(const double& v, S& serializer) {
    serializer.Double(v);
}

template <typename S>
void do_serialize_integral(const std::int64_t& v, S& serializer) {
    serializer.Int64(v);
}

template <typename S>
void do_serialize_integral(const std::uint64_t& v, S& serializer) {
    serializer.Uint64(v);
}

template <typename dataT, typename serializerT>
void do_serialize(const dataT& object, serializerT& serializer,
                  const serialize_properties_t& properties) {

    if constexpr (is_optional<dataT>::value) {
        if (object.has_value()) {
            do_serialize(object.value(), serializer, properties);
        } else {
            serializer.Null();
        }
    } else if constexpr (std::is_integral<dataT>::value
            || std::is_floating_point<dataT>::value) {
        do_serialize_integral(object, serializer);
    } else if constexpr (std::is_same<dataT, std::string>::value) {
        serializer.String(object.c_str(),
            static_cast<rapidjson::SizeType>(object.size()),
            true);
    }  else if constexpr (boost::fusion::traits::is_sequence<dataT>::value) {
        serializer.StartObject();
#ifdef RESTC_CPP_LOG_JSON_SERIALIZATION
        RESTC_CPP_LOG_TRACE_(RESTC_CPP_TYPENAME(dataT)
            << " StartObject: ");
#endif
        auto fn = [&](const char *name, auto& val) {
#ifdef RESTC_CPP_LOG_JSON_SERIALIZATION
            RESTC_CPP_LOG_TRACE_(RESTC_CPP_TYPENAME(dataT)
                << " Key: " << name);
#endif
            if (properties.ignore_empty_fileds) {
                if (is_empty_field(val)) {
#ifdef RESTC_CPP_LOG_JSON_SERIALIZATION
            RESTC_CPP_LOG_TRACE_(RESTC_CPP_TYPENAME(dataT)
                << " ignoring empty field.");
#endif
                    return;
                }
            }

            if (properties.excluded_names
                && properties.is_excluded(name)) {
#ifdef RESTC_CPP_LOG_JSON_SERIALIZATION
                RESTC_CPP_LOG_TRACE_(RESTC_CPP_TYPENAME(dataT)
                    << " ignoring excluded field.");
#endif
                return;
            }

            serializer.Key(properties.map_name_to_json(name).c_str());

            using const_field_type_t = decltype(val);
            using native_field_type_t = typename std::remove_const<typename std::remove_reference<const_field_type_t>::type>::type;
            using field_type_t = typename std::add_lvalue_reference<native_field_type_t>::type;

            auto& const_value = val;
            auto& value = const_cast<field_type_t&>(const_value);

            do_serialize<native_field_type_t>(value, serializer, properties);

        };

        on_name_and_value<dataT, decltype(fn)> handler(fn);
        handler.for_each_member(object);
#ifdef RESTC_CPP_LOG_JSON_SERIALIZATION
        RESTC_CPP_LOG_TRACE_(RESTC_CPP_TYPENAME(dataT)
            << " EndObject: ");
#endif
        serializer.EndObject();
    } else if constexpr (is_container<dataT>::value) {
#ifdef RESTC_CPP_LOG_JSON_SERIALIZATION
        RESTC_CPP_LOG_TRACE_(RESTC_CPP_TYPENAME(dataT)
            << " StartArray: ");
#endif
        serializer.StartArray();

        for(const auto& v: object) {

            using native_field_type_t = typename std::remove_const<
                typename std::remove_reference<decltype(v)>::type>::type;

            do_serialize<native_field_type_t>(v, serializer, properties);
        }
#ifdef RESTC_CPP_LOG_JSON_SERIALIZATION
        RESTC_CPP_LOG_TRACE_(RESTC_CPP_TYPENAME(dataT)
            << " EndArray: ");
#endif
        serializer.EndArray();
    } else if constexpr (is_map<dataT>::value) {
        static const serialize_properties_t map_name_properties{false};

#ifdef RESTC_CPP_LOG_JSON_SERIALIZATION
        RESTC_CPP_LOG_TRACE_(RESTC_CPP_TYPENAME(dataT)
            << " StartMap: ");
#endif
        serializer.StartObject();

        for(const auto& v: object) {

            using native_field_type_t = typename std::remove_const<
                typename std::remove_reference<typename dataT::mapped_type>::type>::type;

            do_serialize<std::string>(v.first, serializer, map_name_properties);
            do_serialize<native_field_type_t>(v.second, serializer, properties);
        }
#ifdef RESTC_CPP_LOG_JSON_SERIALIZATION
        RESTC_CPP_LOG_TRACE_(RESTC_CPP_TYPENAME(dataT)
            << " EndMap: ");
#endif
        serializer.EndObject();
    } else if constexpr (has_std_to_string_implementation<dataT>(0)) {
        const auto s = ::std::to_string(object);
        serializer.String(s.c_str(),
            static_cast<rapidjson::SizeType>(s.size()),
            true);
    }

    else {
//        static_assert (false, "do_serialize: Unexpected type. Don't know how to convert it");
        throw ParseException(std::string{"do_serialize: Unexpected type: "} +
                             RESTC_CPP_TYPENAME(dataT));
    }
}

} // namespace

/*! Recursively serialize the C++ object to the json serializer */
template <typename objectT, typename serializerT>
class RapidJsonSerializer
{
public:
    using data_t = typename std::remove_const<typename std::remove_reference<objectT>::type>::type;

    RapidJsonSerializer(const data_t& object, serializerT& serializer)
    : object_{object}, serializer_{serializer}
    {
    }

    // See https://github.com/miloyip/rapidjson/blob/master/doc/sax.md#writer-writer
    void Serialize() {
        do_serialize<data_t>(object_, serializer_, properties_);
    }

    void IgnoreEmptyMembers(bool ignore = true) {
        properties_.ignore_empty_fileds = ignore;
    }

    // Set to nullptr to disable lookup
    void ExcludeNames(excluded_names_t *names) {
        properties_.excluded_names = names;
    }

    void SetNameMapping(const JsonFieldMapping *mapping) {
        properties_.name_mapping = mapping;
    }

private:

    const data_t& object_;
    serializerT& serializer_;
    serialize_properties_t properties_;
};

/*! Serialize an object or a list of objects of type T to the wire
 *
*/
template <typename T>
class RapidJsonInserter
{
public:
    using stream_t = RapidJsonWriter<char>;
    using writer_t = rapidjson::Writer<stream_t>;

    /*! Constructor
     *
     * \param writer Output DataWriter
     * \param isList True if we want to serialize a Json list of objects.
     *      If false, we can only Add() one object.
     */
    RapidJsonInserter(DataWriter& writer, bool isList = false)
    : is_list_{isList}, stream_{writer}, writer_{stream_} {}

    RapidJsonInserter(DataWriter& writer, bool isList,
         const serialize_properties_t& properties)
    : is_list_{isList}, stream_{writer}, writer_{stream_}
    , properties_{properties} {}

    ~RapidJsonInserter() {
        Done();
    }

    /*! Serialize one object
     *
     * If the constructor was called with isList = false,
     * it can only be called once.
     */
    void Add(const T& v) {
        if (state_ == State::DONE) {
            throw RestcCppException("Object is DONE. Cannot Add more data.");
        }

        if (state_ == State::PRE) {
            if (is_list_) {
                writer_.StartArray();
            }
            state_ = State::ITERATING;
        }

        do_serialize<T>(v, writer_, properties_);
    }

    /*! Mark the serialization as complete */
    void Done() {
        if (state_ == State::ITERATING) {
            if (is_list_) {
                writer_.EndArray();
            }
        }
        state_ = State::DONE;
    }

    void IgnoreEmptyMembers(bool ignore = true) {
        properties_.ignore_empty_fileds = ignore;
    }

    // Set to nullptr to disable lookup
    void ExcludeNames(const excluded_names_t *names) {
        properties_.excluded_names = names;
    }

    void SetNameMapping(const JsonFieldMapping *mapping) {
        properties_.name_mapping = mapping;
    }

private:
    enum class State { PRE, ITERATING, DONE };

    State state_ = State::PRE;
    const bool is_list_;
    stream_t stream_;
    writer_t writer_;
    serialize_properties_t properties_;
};

/*! Serialize a std::istream to a C++ class instance */
template <typename dataT>
void SerializeFromJson(dataT& rootData,
    std::istream& stream, const serialize_properties_t& properties) {

    RapidJsonDeserializer<dataT> handler(rootData, properties);
    rapidjson::IStreamWrapper input_stream_reader(stream);
    rapidjson::Reader json_reader;
    json_reader.Parse(input_stream_reader, handler);
}

/*! Serialize a std::istream to a C++ class instance */
template <typename dataT>
void SerializeFromJson(dataT& rootData,
    std::istream& stream) {

    serialize_properties_t properties;
    SerializeFromJson(rootData, stream, properties);
}

/*! Serialize json in a std::string to a C++ class instance */
template <typename dataT>
void SerializeFromJson(dataT& rootData,
                       const std::string& data, const serialize_properties_t& properties = {}) {

    std::istringstream stream{data};
    SerializeFromJson(rootData, stream, properties);
}

/*! Serialize a reply to a C++ class instance */
template <typename dataT>
void SerializeFromJson(dataT& rootData, Reply& reply,
                       const serialize_properties_t& properties) {

    RapidJsonDeserializer<dataT> handler(rootData, properties);
    RapidJsonReader reply_stream(reply);
    rapidjson::Reader json_reader;
    json_reader.Parse(reply_stream, handler);
}

/*! Serialize a reply to a C++ class instance */
template <typename dataT>
void SerializeFromJson(dataT& rootData, Reply& reply) {
    serialize_properties_t properties;
    SerializeFromJson(rootData, reply, properties);
}

/*! Serialize a reply to a C++ class instance */
template <typename dataT>
void SerializeFromJson(dataT& rootData, std::unique_ptr<Reply>&& reply) {
    serialize_properties_t properties;
    SerializeFromJson(rootData, *reply, properties);
}

/*! Serialize a reply to a C++ class instance */
template <typename dataT>
void SerializeFromJson(dataT& rootData,
    Reply& reply,
    const JsonFieldMapping *nameMapper,
    std::int64_t maxBytes = RapidJsonDeserializer<dataT>::default_mem_limit) {

    serialize_properties_t properties;
    properties.max_memory_consumption = maxBytes;
    properties.name_mapping = nameMapper;

    SerializeFromJson(rootData, reply, properties);
}

/*! Serialize a reply to a C++ class instance */
template <typename dataT>
void SerializeFromJson(dataT& rootData,
    std::unique_ptr<Reply>&& reply,
    const JsonFieldMapping *nameMapper,
    std::int64_t maxBytes = RapidJsonDeserializer<dataT>::default_mem_limit) {

    serialize_properties_t properties;
    properties.SetMaxMemoryConsumption(maxBytes);
    properties.name_mapping = nameMapper;

    SerializeFromJson(rootData, *reply, properties);
}

/*! Serialize a C++ object to a std::ostream */
template <typename dataT>
void SerializeToJson(dataT& rootData,
    std::ostream& ostream, const serialize_properties_t& properties = {}) {

    rapidjson::OStreamWrapper osw(ostream);
    rapidjson::Writer<rapidjson::OStreamWrapper> writer(osw);
    do_serialize<dataT>(rootData, writer, properties);
}


} // namespace

