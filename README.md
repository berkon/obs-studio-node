# libobs via node bindings
This library intends to provide bindings to obs-studio's internal library, named libobs accordingly, for the purpose of using it from a node runtime.
Currently, only Windows is supported.

## Why CMake?
CMake offers better compatibility with existing projects than node-gyp and comparable solutions. It's also capable of generating solution files for multiple different IDEs and compilers, which makes it ideal for a native module. Personally, I don't like gyp syntax or the build system surrounding it or the fact it requires you to install python.

# Building

## Prerequisites
You will need to have the following installed:

* Git
* [Node.js](https://nodejs.org/en/)
* [Yarn](https://yarnpkg.com/en/docs/install#windows-stable)
* [CMake](https://cmake.org/)

### Windows
Building on windows requires additional software:

* [Visual Studio 2019, 2017 or 2015](https://visualstudio.microsoft.com/)
* [Windows SDK](https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk) (may be installed with Visual Studio 2017 Installer)

### Example Build
We use a flexible cmake script to be as broad and generic as possible in order to prevent the need to constantly manage the cmake script for custom uses, while also providing sane defaults. It follows a pretty standard cmake layout and you may execute it however you want.

Example:
```
yarn install
git submodule update --init --recursive
mkdir build
cd build
cmake .. -G"Visual Studio 16 2019" -A x64
cmake --build . --config Release
cpack -G ZIP
```

This will will download any required dependencies, build the module, and then place it in an archive compatible with npm or yarn that you may specify in a given package.json.

### Custom OBS Build
By default, we download a pre-built version of libobs if none is specified. However, this pre-built version may not be what you want to use or maybe you're testing a new obs feature.

You may specify a custom archive of your own. However, some changes need to be made to obs-studio's default configuration before building:

* `ENABLE_SCRIPTING` must be set to `false`
* `ENABLE_UI` must be set to `false`
* `QTDIR` should *not* be specified.

If you don't know how to build obs-studio from source, you may find instructions [here](https://github.com/obsproject/obs-studio/wiki/Install-Instructions#windows-build-directions).

Example (from root of obs-studio repository clone):
```
mkdir build
cd build
cmake .. -DENABLE_UI=false -DDepsPath="C:\Users\computerquip\Projectslibobs-deps\win64" -DENABLE_SCRIPTING=false -G"Visual Studio 15 2017" -A x64
cmake --build .
cpack -G ZIP
```

This will create an archive that's compatible with obs-studio-node. The destination of the archive will appear after cpack is finished executing.

Example:

> CPack: Create package using ZIP
>
> CPack: Install projects
>
> CPack: - Install project: obs-studio
>
> CPack: Create package
>
> CPack: - package: C:/Users/computerquip/Projects/obs-studio/build/obs-studio-x64-22.0.3-sl-7-13-g208cb2f5.zip generated.

This archive may then be specified as a cmake variable when building obs-studio-node like so:
```
cmake .. -G"Visual Studio 15 2017" -A x64 -DOSN_LIBOBS_URL="C:/Users/computerquip/Projects/obs-studio/build/obs-studio-x64-22.0.3-sl-7-13-g208cb2f5.zip"
cmake --build .
cpack -G ZIP
```

### Further Building
I don't specify every possible combination of variables. Here's a list of actively maintained variables that control how obs-studio-node is built:

* All configurable node-cmake variables found [here](https://github.com/cjntaylor/node-cmake/blob/dev/docs/NodeJSCmakeManual.md).
* `OSN_LIBOBS_URL` - Controls where to fetch the libobs archive. May be a directory, any compressed archive that cpack supports, or a URI of various types including FTP or HTTP/S.

If you find yourself unable to configure something about our build script or have any questions, please file a github issue!

define `EXTENDED_DEBUG_LOG` controls logging of ipc requests. 

### Static code analyzis 

#### cppcheck 

Install cppcheck from http://cppcheck.sourceforge.net/ and add cppcheck folder to PATH 
To run check from console:  
```
cd build 
cmake --build . --target CPPCHECK
```

Also target can be built from Visula Studio. 
Report output format set as compatible and navigation to file:line posiible from build results panel.  

Some warnings suppressed in files `obs-studio-client/cppcheck_suppressions_list.txt` and `obs-studio-server/cppcheck_suppressions_list.txt`.

#### Clang Analyzer 

`Ninja` and `LLVM` have to be installed in system. Warning: depot_tool have broken ninja.  
To make build open `cmd.exe`. 


```
mkdir build_clang
cd build_clang

"c:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\amd64\vcvars64.bat"
 
set CCC_CC=clang-cl
set CCC_CXX=clang-cl
set CC=ccc-analyzer.bat
set CXX=c++-analyzer.bat
#set CCC_ANALYZER_VERBOSE=1

#make ninja project 
cmake  -G "Ninja" -DCLANG_ANALYZE_CONFIG=1 -DCMAKE_INSTALL_PREFIX:PATH=""  -DCMAKE_LINKER=lld-link -DCMAKE_BUILD_TYPE="Debug"   -DCMAKE_SYSTEM_NAME="Generic" -DCMAKE_MAKE_PROGRAM=ninja.exe ..

#try to build and "fix" errors 
ninja.exe 

#clean build to scan 
ninja.exe clean 

scan-build --keep-empty -internal-stats -stats -v -v -v -o check ninja.exe
```
Step with `"fixing"` errors is important as code base and especially third-party code are not ready to be build with clang. And files which failed to compile will not be scanned for errors.

### Tests

The tests for obs studio node are written in Typescript and use Mocha as test framework, with electron-mocha pacakage to make Mocha run in Electron, and Chai as assertion framework.

You need to build obs-studio-node in order to run the tests. You can build it any way you want, just be sure to use `CMAKE_INSTALL_PREFIX` to install obs-studio-node in a folder of your choosing. The tests use this variable to know where the obs-studio-node module is. Since we use our own fork of Electron, you also need to create an environment variable called `ELECTRON_PATH` pointing to where the Electron binary is in the node_modules folder after you run `yarn install`. Below are three different ways to build obs-studio-node:

#### Terminal commands
In obs-studio-node root folder:
1. `yarn install`
2. `git submodule update --init --recursive --force`
3. `mkdir build`
4. `cmake -Bbuild -H. -G"Visual Studio 15 2017" -A x64 -DCMAKE_INSTALL_PREFIX="path_of_your_choosing"`
5. `cmake --build build --target install`

#### Terminal using package.json scripts
In obs-studio-node root folder:
1. `mkdir build`
2. `yarn local:config`
3. `yarn local:build`
4. Optional: To clean build folder to repeat the steps 2 to 3 again do `yarn local:clean`

#### CMake GUI
1. `yarn install`
2. Create a build folder in obs-studio-node root
3. Open CMake GUI
4. Put obs-studio-node project path in `Where is the source code:` box
5. Put path to build folder in `Where to build the binaries:` box
6. Click `Configure`
7. Change CMAKE_INSTALL_PREFIX to a folder path of your choosing
8. Click `Generate`
9. Click `Open Project` to open Visual Studio and build the project there

#### Running tests
Some tests interact with Twitch and we use a user pool service to get users but in case we are not able to fetch a user from it, we use the stream key provided by an environment variable. Create an environment variable called SLOBS_BE_STREAMKEY with the stream key of a Twitch account of your choosing.

* To run all the tests do `yarn run test` 
* To run only run one test do `yarn run test --grep describe_name_value` where `describe_name_value` is the name of the test passed to the describe call in each test file. Example: `yarn run test --grep nodeobs_api`
