/*
 * Copyright (c) 2020 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2024-11-26
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

#pragma once

#include <string>
#include <vector>

#include <maxbase/ccdefs.hh>
#include <maxbase/jansson.h>

namespace maxbase
{

/**
 * @brief Convenience function for dumping JSON into a string
 *
 * @param json  JSON to dump
 * @param flags Optional flags passed to the jansson json_dump function
 *
 * @return The JSON in string format
 */
std::string json_dump(const json_t* json, int flags = 0);

/**
 * @brief Return value at provided JSON Pointer
 *
 * @see https://datatracker.ietf.org/doc/html/rfc6901
 *
 * @param json     JSON object
 * @param json_ptr JSON Pointer to object
 *
 * @return Pointed value or NULL if no value is found
 */
json_t* json_ptr(const json_t* json, const char* json_ptr);

/**
 * Wrapper class for Jansson json-objects.
 */
class Json
{
public:

    enum class Type
    {
        OBJECT,     /**< Json object */
        ARRAY,      /**< Json array */
        JS_NULL,    /**< Json null */
        NONE        /**< No object */
    };

    enum class Format
    {
        NORMAL  = 0,                // JSON on one line
        COMPACT = JSON_COMPACT,     // As compact as possible
        PRETTY  = JSON_INDENT(4),   // Pretty-printed
    };

    /**
     * Construct a new Json wrapper object.
     *
     * @param obj The type of the object to create
     */
    explicit Json(Type type = Type::OBJECT);

    ~Json();

    /**
     * Construct a new Json wrapper object. Increments reference count of obj.
     *
     * @param obj The object to manage
     */
    explicit Json(json_t* obj);

    Json(const Json& rhs);
    Json& operator=(const Json& rhs);

    Json(Json&& rhs) noexcept;
    Json& operator=(Json&& rhs);

    /**
     * Load data from json string. Removes any currently held object.
     *
     * @param source Source string
     *
     * @return True on success
     */
    bool load_string(const std::string& source);

    /**
     * Load data from a file.
     *
     * @param filepath Path to a JSON file that is loaded
     *
     * @return True on success
     */
    bool load(const std::string& filepath);

    /**
     * Save data to a file.
     *
     * @param filepath Path to where the JSON file is stored
     * @param flags    The format to store the file in
     *
     * @return True on success
     */
    bool save(const std::string& filepath, Format format = Format::PRETTY);

    /**
     * Check if object contains a field
     *
     * @param key The name of the field
     *
     * @return True if the object has the field
     */
    bool contains(const std::string& key) const;

    /**
     * Check if a field in a object is a JSON null
     *
     * @param key The name of the field
     *
     * @return True if the field exists and is a JSON null
     */
    bool is_null(const std::string& key) const;

    /**
     * Get JSON object from a field
     *
     * @param key The name of the field
     *
     * @return The JSON object or an empty Json if it doesn't exist (i.e. valid() returns false)
     */
    Json get_object(const char* key) const;
    Json get_object(const std::string& key) const;

    /**
     * Get JSON string from a field
     *
     * @param key The name of the field
     *
     * @return The string if it was found or an empty string if it wasn't. Use try_get_string() if you need to
     *         reliably extract empty strings or strings that aren't guaranteed to exist.
     */
    std::string get_string(const char* key) const;
    std::string get_string(const std::string& key) const;

    /**
     * Get JSON integer from a field
     *
     * @param key The name of the field
     *
     * @return The integer value or 0 if the field did not exist. Use try_get_int() if you need to
     *         reliably extract integers that aren't guaranteed to exist.
     */
    int64_t get_int(const char* key) const;
    int64_t get_int(const std::string& key) const;

    /**
     * Try to get a JSON integer from a field
     *
     * @param key The name of the field
     * @param out The value where the result is stored
     *
     * @return True if the field was found and it was an integer
     */
    bool try_get_int(const std::string& key, int64_t* out) const;

    /**
     * Try to get a JSON string from a field
     *
     * @param key The name of the field
     * @param out The value where the result is stored
     *
     * @return True if the field was found and it was a string
     */
    bool try_get_string(const std::string& key, std::string* out) const;

    /**
     * Get JSON array elements
     *
     * @param key The name of the field
     *
     * @return A vector of mxb::Json objects. If the field is not an array, an empty vector is returned.
     */
    std::vector<Json> get_array_elems(const std::string& key) const;

    /**
     * Get value at JSON Pointer
     *
     * @param ptr The JSON Pointer to use
     *
     * @return The value at the pointer or an empty object if no value is found
     */
    Json at(const char* ptr) const;

    /**
     * Get latest error message
     *
     * @return The latest error message or an empty string if no errors have occurreed
     */
    const std::string& error_msg() const;

    /**
     * Check if this mxb::Json is valid
     *
     * @return True if this instance is managing an object
     */
    bool valid() const;

    /**
     * Store a JSON object in a field
     *
     * @param key   The name of the field to store the value in
     * @param value The value to store
     */
    void set_object(const char* key, const Json& value);
    void set_object(const char* key, Json&& value);

    /**
     * Store a JSON string in a field
     *
     * @param key   The name of the field to store the value in
     * @param value The value to store
     */
    void set_string(const char* key, const char* value);
    void set_string(const char* key, const std::string& value);

    /**
     * Store a JSON integer in a field
     *
     * Note that JavaScript does not have a concept of integers and only has floating point numbers. Jansson
     * does support integers in JSON and handles their conversion correctly. The values stored with this
     * aren't the same as the they would be if stored in JavaScript but this is only a problem with large
     * numbers.
     *
     * @param key   The name of the field to store the value in
     * @param value The value to store
     */
    void set_int(const char* key, int64_t value);

    /**
     * Store a JSON number in a field
     *
     * @param key   The name of the field to store the value in
     * @param value The value to store
     */
    void set_float(const char* key, double value);

    /**
     * Store a JSON boolean in a field
     *
     * @param key   The name of the field to store the value in
     * @param value The value to store
     */
    void set_bool(const char* key, bool value);

    /**
     * Store a JSON null in a field
     *
     * @param key   The name of the field to store the value in
     */
    void set_null(const char* key);

    /**
     * Apend an element to an array
     *
     * @param value The value to append
     */
    void add_array_elem(const Json& elem);
    void add_array_elem(Json&& elem);

    /**
     * Remove a field from a JSON object
     *
     * @param key The field to remove
     */
    void erase(const char* key);
    void erase(const std::string& key);

    /**
     * Check if the object is OK
     *
     * @return True if there have been no errors. This does not mean the contents are valid if it was
     *         constructed with Type::NONE.
     */
    bool ok() const;

    /**
     * Get underlying JSON pointer
     *
     * @return Pointer to the managed JSON object
     */
    json_t* get_json() const;

    /**
     * Return contents as a string
     *
     * @param format The format to print the JSON in
     *
     * @return The JSON as a string
     */
    std::string to_string(Format format = Format::PRETTY) const;

    /**
     * Replace the current JSON object
     *
     * @param obj Object to use. The reference is stolen.
     */
    void reset(json_t* obj = nullptr);

    /**
     * Check if two JSON values are equal
     *
     * @return True if values compare equal
     */
    bool equal(const Json& other) const;

private:
    json_t*             m_obj{nullptr}; /**< Managed json-object */
    mutable std::string m_errormsg;     /**< Error message container */

    void swap(Json& rhs) noexcept;
};

/**
 * Compares two JSON values for equality
 *
 * @see https://jansson.readthedocs.io/en/2.13/apiref.html#equality
 *
 * @return True if the values compare equal
 */
static inline bool operator==(const Json& lhs, const Json& rhs)
{
    return lhs.equal(rhs);
}

/**
 * Compares two JSON values for inequality
 *
 * @return True if the values compare inequal
 */
static inline bool operator!=(const Json& lhs, const Json& rhs)
{
    return !(lhs == rhs);
}
}
