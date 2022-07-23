#pragma once

namespace configcat {

class OverrideDataSource;

// Describes how the overrides should behave.
enum OverrideBehaviour {
    // When evaluating values, the SDK will not use feature flags & settings from the ConfigCat CDN, but it will use
    // all feature flags & settings that are loaded from local-override sources.
    LocalOnly,

    // When evaluating values, the SDK will use all feature flags & settings that are downloaded from the ConfigCat CDN,
    // plus all feature flags & settings that are loaded from local-override sources. If a feature flag or a setting is
    // defined both in the fetched and the local-override source then the local-override version will take precedence.
    LocalOverRemote,

    // When evaluating values, the SDK will use all feature flags & settings that are downloaded from the ConfigCat CDN,
    // plus all feature flags & settings that are loaded from local-override sources. If a feature flag or a setting is
    // defined both in the fetched and the local-override source then the fetched version will take precedence.
    RemoteOverLocal
};

/**
 * Describes feature flag & setting overrides.
 *
 * [dataSource] contains the flag values in a key-value map.
 * [behaviour] can be used to set preference on whether the local values should
 * override the remote values, or use local values only when a remote value doesn't exist,
 * or use it for local only mode.
 */
class FlagOverrides {
public:
    FlagOverrides(std::shared_ptr<OverrideDataSource> dataSource, OverrideBehaviour behaviour):
        dataSource(dataSource),
        behaviour(behaviour) {
    }

    std::shared_ptr<OverrideDataSource> dataSource;
    OverrideBehaviour behaviour;
};

} // namespace configcat
