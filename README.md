
[![Build](https://img.shields.io/github/workflow/status/configcat/cpp-sdk/C++%20CI?logo=GitHub&label=windows%20%2F%20macos%20%2F%20linux)](https://github.com/configcat/cpp-sdk/actions/workflows/cpp-ci.yml)
[![codecov](https://codecov.io/gh/configcat/cpp-sdk/branch/main/graph/badge.svg?token=cvUgfof8k7)](https://codecov.io/gh/configcat/cpp-sdk)

# ConfigCat SDK for C++

https://configcat.com

ConfigCat SDK for C++ provides easy integration for your application to ConfigCat.

C++ Standard Minimum Level: `C++17`.

ConfigCat is a feature flag and configuration management service that lets you separate releases from deployments. You can turn your features ON/OFF using [ConfigCat Dashboard](http://app.configcat.com) even after they are deployed. ConfigCat lets you target specific groups of users based on region, email or any other custom user attribute.

ConfigCat is a [hosted feature flag service](http://configcat.com). Manage feature toggles across frontend, backend, mobile, desktop apps. [Alternative to LaunchDarkly](https://configcat.com/launchdarkly-vs-configcat). Management app + feature flag SDKs.

## Getting started

### 1. Install the ConfigCat SDK
With [Vcpkg](https://github.com/microsoft/vcpkg)

- On Windows:
  ```cmd
  git clone https://github.com/microsoft/vcpkg
  .\vcpkg\bootstrap-vcpkg.bat
  .\vcpkg\vcpkg install configcat
  ```

  In order to use vcpkg with Visual Studio,
  run the following command (may require administrator elevation):

  ```cmd
  .\vcpkg\vcpkg integrate install
  ```

  After this, you can create a New non-CMake Project (or open an existing one).
  All installed libraries are immediately ready to be `#include`'d and used
  in your project without additional configuration.


- On Linux/Mac:
  ```bash
  git clone https://github.com/microsoft/vcpkg
  ./vcpkg/bootstrap-vcpkg.sh
  ./vcpkg/vcpkg install configcat
  ```

In order to use vcpkg with CMake, you can use the toolchain file:

```bash
cmake -B [build directory] -S . "-DCMAKE_TOOLCHAIN_FILE=[path to vcpkg]/scripts/buildsystems/vcpkg.cmake"
cmake --build [build directory]
```

We recommend that you use the `vcpkg.json` manifest file to add the dependencies to your project.
You can find an example manifest file [here](example/vcpkg.json).

### 2. Go to the <a href="https://app.configcat.com/sdkkey" target="_blank">ConfigCat Dashboard</a> to get your *SDK Key*
![SDK-KEY](https://raw.githubusercontent.com/ConfigCat/cpp-sdk/master/media/readme02-3.png "SDK-KEY")

### 3. Include *configcat.h* header in your application code
```cpp
#include <configcat/configcat.h>

using namespace configcat;
```

### 4. Create a *ConfigCat* client instance
```cpp
auto client = ConfigCatClient::get("#YOUR-SDK-KEY#");
```

### 5. Get your setting value
```cpp
bool isMyAwesomeFeatureEnabled = client->getValue("isMyAwesomeFeatureEnabled", false);
if (isMyAwesomeFeatureEnabled) {
    doTheNewThing();
} else {
    doTheOldThing();
}
```
### 6. Close the client on application exit
```cpp
ConfigCatClient::closeAll();
```

## Getting user-specific setting values with Targeting
Using this feature, you will be able to get different setting values for different users in your application by passing a `User Object` to the `getValue()` function.

Read more about Targeting [here](https://configcat.com/docs/advanced/targeting/).

## User Object
Percentage and targeted rollouts are calculated by the user object passed to the configuration requests.
The user object must be created with a **mandatory** identifier parameter which uniquely identifies each user:
```cpp
auto user = ConfigCatUser("#USER-IDENTIFIER#");

bool isMyAwesomeFeatureEnabled = client->getValue("isMyAwesomeFeatureEnabled", false, &user);
if (isMyAwesomeFeatureEnabled) {
    doTheNewThing();
} else {
    doTheOldThing();
}
```

## Sample/Demo apps
* [Example Console app](https://github.com/ConfigCat/cpp-sdk/tree/main/example/)

## Polling Modes
The ConfigCat SDK supports three different polling mechanisms to acquire the setting values from ConfigCat. After the latest setting values are downloaded, they are stored in an internal cache . After that, all requests are served from the cache. Read more about Polling Modes and how to use them at [ConfigCat C++ Docs](https://configcat.com/docs/sdk-reference/cpp/).

## Support
If you need help using this SDK, feel free to contact the ConfigCat Staff at [https://configcat.com](https://configcat.com). We're happy to help.

## Contributing
Contributions are welcome. For more info please read the [Contribution Guideline](CONTRIBUTING.md).

## About ConfigCat
- [Official ConfigCat SDKs for other platforms](https://github.com/configcat)
- [Documentation](https://configcat.com/docs)
- [Blog](https://configcat.com/blog)
