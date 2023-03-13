# Steps to deploy
## Preparation
1. Run tests
   ```bash
   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=[path to vcpkg]/scripts/buildsystems/vcpkg.cmake
   cmake --build build
   cd build && ctest
   ```
2. Increase the `CONFIGCAT_VERSION` in [src/version.h](src/version.h).
3. Commit & Push
## Publish
Use the **same version** for the git tag as in [src/version.h](src/version.h).
- Via git tag
    1. Create a new version tag.
       ```bash
       git tag v[MAJOR].[MINOR].[PATCH]
       ```
       > Example: `git tag v2.5.5`
    2. Push the tag.
       ```bash
       git push origin --tags
       ```
- Via Github release 

  Create a new [Github release](https://github.com/configcat/cpp-sdk/releases) with a new version tag and release notes.

## Vcpkg Package
- Fork the [vcpkg](https://github.com/microsoft/vcpkg) repo on GitHub.
- In the repo directory run `./bootstrap-vcpkg.sh`.
- Calculate `SHA512` of the released `.tar.gz` file.  
   - Download `.tar.gz` file
      ```bash
      wget https://github.com/configcat/cpp-sdk/archive/refs/tags/[GIT_TAG].tar.gz
      ```
   - Calculate `SHA512`
      ```bash
      ./vcpkg hash [GIT_TAG].tar.gz
      ``` 
- Update the git tag and the `SHA512` hash in the fork repo's `ports/configcat/portfile.cmake` file.
  ```cmake
  vcpkg_from_github(
      OUT_SOURCE_PATH SOURCE_PATH
      REPO configcat/cpp-sdk
      REF [GIT_TAG]
      SHA512 [HASH]
      HEAD_REF master
  )
  ```
- Update the version in the fork repo's `ports/configcat/vcpkg.json` file.
  ```
  {
      "name": "configcat",
      "version": "[CONFIGCAT_VERSION]",
      ...
  }
  ```
- Commit & Push with commit message like `[configcat] Update to version 2.5.5`.
- Run `./vcpkg x-add-version --all`
- Commit & Push with commit message like `[configcat] Update to version 2.5.5`.
- Create a PR from the fork to [vcpkg](https://github.com/microsoft/vcpkg) master branch. PR's title should be like `[configcat] Update to version 2.5.5`.
- Make sure the PR is merged by the vcpkg team.
