#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include "timeutils.h"

namespace configcat {

// An object containing attributes to properly identify a given user for rollout evaluation.
class ConfigCatUser {
public:
    struct AttributeValue : public std::variant<std::string, double, date_time_t, std::vector<std::string>> {
    private:
        using _Base = std::variant<std::string, double, date_time_t, std::vector<std::string>>;
    public:
        AttributeValue(const char* v) : _Base(std::string(v)) {}
        // CLang number type conversion to variant<double> fix
        AttributeValue(double value) : _Base(value) {}

        // Disable the implicit conversion from pointer to bool: https://stackoverflow.com/a/59372958/8656352
        template<typename T>
        AttributeValue(T*) = delete;

        using _Base::_Base;
        using _Base::operator=;
    };

    static constexpr char kIdentifierAttribute[] = "Identifier";
    static constexpr char kEmailAttribute[] = "Email";
    static constexpr char kCountryAttribute[] = "Country";

    /**
     * Creates a new instance of the [ConfigCatUser] class.
     *
     * Parameter [id]: the unique identifier of the user or session (e.g. email address, primary key, session ID, etc.)
     * Parameter [email]: email address of the user.
     * Parameter [country]: country of the user.
     * Parameter [custom]: custom attributes of the user for advanced targeting rule definitions (e.g. user role, subscription type, etc.)
     *
     * All comparators support `std::string` values as User Object attribute (in some cases they need to be provided in a specific format though, see below),
     * but some of them also support other types of values. It depends on the comparator how the values will be handled. The following rules apply:
     *
     * **Text-based comparators** (EQUALS, IS ONE OF, etc.)
     * * accept `std::string` values,
     * * all other values are automatically converted to `std::string` (a warning will be logged but evaluation will continue as normal).
     *
     * **SemVer-based comparators** (IS ONE OF, &lt;, &gt;=, etc.)
     * * accept `std::string` values containing a properly formatted, valid semver value,
     * * all other values are considered invalid (a warning will be logged and the currently evaluated targeting rule will be skipped).
     *
     * **Number-based comparators** (=, &lt;, &gt;=, etc.)
     * * accept `double` values,
     * * accept `std::string` values containing a properly formatted, valid `double` value,
     * * all other values are considered invalid (a warning will be logged and the currently evaluated targeting rule will be skipped).
     *
     * **Date time-based comparators** (BEFORE / AFTER)
     * * accept `configcat::date_time_t` (`std::chrono::system_clock::time_point`) values,
         which are automatically converted to a second-based Unix timestamp,
     * * accept `double` values representing a second-based Unix timestamp,
     * * accept `std::string` values containing a properly formatted, valid `double` value,
     * * all other values are considered invalid (a warning will be logged and the currently evaluated targeting rule will be skipped).
     *
     * **String array-based comparators** (ARRAY CONTAINS ANY OF / ARRAY NOT CONTAINS ANY OF)
     * * accept lists of `std::string` (i.e. `std::vector<std::string>`),
     * * accept `std::string` values containing a valid JSON string which can be deserialized to a list of `std::string`,
     * * all other values are considered invalid (a warning will be logged and the currently evaluated targeting rule will be skipped).
     */
    ConfigCatUser(const std::string& id,
        const std::optional<std::string>& email = std::nullopt,
        const std::optional<std::string>& country = std::nullopt,
        const std::unordered_map<std::string, ConfigCatUser::AttributeValue>& custom = {})
        : identifier(id)
        , email(email)
        , country(country)
        , custom(custom) {}

    static inline std::shared_ptr<ConfigCatUser> create(const std::string& id,
        const std::optional<std::string>& email = std::nullopt,
        const std::optional<std::string>& country = std::nullopt,
        const std::unordered_map<std::string, ConfigCatUser::AttributeValue>& custom = {}) {
        return std::make_shared<ConfigCatUser>(id, email, country, custom);
    }

    inline const std::string& getIdentifier() const { return std::get<std::string>(this->identifier); }
    inline const ConfigCatUser::AttributeValue& getIdentifierAttribute() const { return this->identifier; }
    const ConfigCatUser::AttributeValue* getAttribute(const std::string& key) const;
    std::string toJson() const;

private:
    ConfigCatUser::AttributeValue identifier;
    std::optional<ConfigCatUser::AttributeValue> email;
    std::optional<ConfigCatUser::AttributeValue> country;
    std::unordered_map<std::string, AttributeValue> custom;
};

} // namespace configcat
