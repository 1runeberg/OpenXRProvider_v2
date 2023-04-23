# OpenXR Provider Library v2

Provides a simpler C++ interface to the OpenXR api. Also includes an optional render module introduced in Demo 5 (Hello PBR).

OpenXR Provider Library is now feature complete - demo tutorial vids soon. 

The optional render module is currently an active work in progress with testing done in **Android (Meta Quest)**, **Win64** and **Linux64**

### Current backlog prior to full release:

| OpenXR Provider Library           | XRVK - Optional Vulkan Render Module                             |  
|-----------------------------------|------------------------------------------------------------------|
| ~~Input subsystem~~               | MSAA support                                                     | 
| ~~Flexible extension subsystem~~  | Mesh Instancing                                                  |
|                                   | ~~Custom graphics pipelines and shaders~~                        |
|                                   | Multithreading (~~Async loading~~, Double buffer, subcommands)   | 
|                                   | Improve async asset loading (add async rendering)                |
|                                   | Animation support and demos (~~transform~~, skel anims)          |
|                                   | XML-style header documentation and resulting refactors (if any)  |


### Current demos and tutorials:

| Platform prerequisite     | Description                                               | YouTube Tutorial  |
|---------------------------|-----------------------------------------------------------|-------------------|
| Windows & Android         | Windows prerequisites and tooling installation & setup    | [![Windows prerequisites and tooling installation & setup](http://img.youtube.com/vi/WWuKZ78hBH8/0.jpg)](http://www.youtube.com/watch?v=WWuKZ78hBH8 "Windows prerequisites and tooling installation & setup") |
| Linux                     | Linux prerequisites and tooling installation & setup      | TBD               |

| Building                  | Description                                               | YouTube Tutorial  |
|---------------------------|-----------------------------------------------------------|-------------------|
| Windows                   | Building for PC, OpenXR app overview and Basic Troubleshooting    | [![Building for PC, OpenXR app overview and Basic Troubleshooting](http://img.youtube.com/vi/xyEkCr7gK3Y/0.jpg)](http://www.youtube.com/watch?v=xyEkCr7gK3Y "Building for PC, OpenXR app overview and Basic Troubleshooting") |
| Android (Meta Quest)      | Building for Android Devices (Meta Quest)      | [![Building for Android Devices (Meta Quest)](http://img.youtube.com/vi/uyDXCbpT9Iw/0.jpg)](http://www.youtube.com/watch?v=uyDXCbpT9Iw "Building for Android Devices (Meta Quest)") |
https://youtu.be/uyDXCbpT9Iw |

| Application               | Description                                               | YouTube Tutorial  |
|---------------------------|-----------------------------------------------------------|-------------------|
| sample_01_hello           | Runtime querying and instance creation                    | TBD               |
| sample_02_hello_exts      | Extensions handling                                       | TBD               |
| sample_03_hello_session   | Session creation                                          | TBD               |
| sample_04_hello_cube      | Event Polling and Session Lifecycle                       | TBD               |
| sample_05_hello_pbr       | Complex rendering and intro xrvk module                   | TBD               |
| sample_06_finger_painting | Multi-and-single vendor extensions                        | TBD               |
| sample_07_finger_painting2| Intro input system, wider device and platform support     | TBD               |
| sample_08_finger_painting3| Basic animation demo, display refresh rate ext impl.      | TBD               |
| sample_09_hidden_worlds   | Eye tracking demo, custom vulkan graphics pipelines       | TBD               |

## I. Pre-built binaries

You can find pre-built library files as well as Android (Meta Quest) and Win64 builds of all demo applications in the `bin_prebuilt` directory. 

The library and demos *should* also build in Linux and Linux ARM, but no testing has been done on this yet.

From demo 4 onwards - PC VR users note that the renderer doesn't render/blit to a desktop mirror. You'll need to use the OpenXR runtime's built-in mirror if you want a desktop view. 

- SteamVR: Click on the hamburger/system icon beside the version number and select "Display VR View"
- Meta: https://developer.oculus.com/documentation/native/pc/dg-compositor-mirror/ 

## II. Building Pre-requisites

- Visual Studio 2019 with C++ modules (https://learn.microsoft.com/en-us/cpp/build/vscpp-step-0-installation?view=msvc-170)
- CMake 3.14.4 and above (https://cmake.org/download/) - Install with include in system path option
- Python 3 (required by the OpenXR SDK submodule build. In Windows, I found it easier to disable Python in "Manage App Execution Aliases" and install it manually from here: https://www.python.org/downloads/)
- Vulkan SDK (https://vulkan.lunarg.com/)
- Git/Git toolset (I use GitKraken in the tutorials: https://www.gitkraken.com/ but feel free to use your preferred git tooling of choice)
- Git LFS (https://git-lfs.com/) - some of the asset files and corresponding pre-built apks can be quite large

### For Android (Meta Quest) builds:

- Quest Device Setup (https://developer.oculus.com/documentation/native/android/mobile-device-setup/)
- Android Studio with Android SDK and NDK (https://developer.android.com/studio)
- Download the Oculus OpenXR Mobile SDK from https://developer.oculus.com/downloads/package/oculus-openxr-mobile-sdk/ and extract in
    `{Drive}:\{OpenXR Provider Library Repo Folder}\third_party\openxr_meta`

## III. Building

## For PC (Win64):
 1. Initialize git lfs `git lfs install` if you haven't already - this ensures you have all the raw binary files in the repository (source art assets, executables, etc)
 2. Clone the repository and initialize submodules: `git clone --recurse-submodules https://github.com/1runeberg/OpenXRProvider_v2.git`
 3. From the root directory of the repository, create a build directory (mkdir build)
 4. Go to the build directory (`cd build`). Create one if it's not present, this is where all the cmake files will be generated.
 5. Run CMake (`cmake ..` or for VS 2017 `cmake -G "Visual Studio 15 2017 Win64" ..`) A *OPENXR_PROVIDER.sln* file is generated in the build directory which you can then open and use in Visual Studio 2019
 6. To build the library and all samples from the commandline: `cmake --build .` or open OPENXR_PROVIDER.sln file in Visual Studio and build from VS

## For Android (Meta Quest):
 1. Clone the repository and initialize submodules: `git clone --recurse-submodules https://github.com/1runeberg/OpenXRProvider_v2.git`
 2. Open Android Studio
 3. Click File > Open File or Project
 4. Select the root folder of the project you want to build (e.g. sample_05_hello_pbr)
 5. Once the project loads, select your connected Quest Device in the top toolbar and hit the Play button
 
 ## For Linux:

1. All pre-requisite install instructions:
- Get Vulkan SDK pacakge from https://vulkan.lunarg.com/sdk/home#linux

    - e.g for Ubuntu Focal Fossa (20.04): 
    - `wget -qO - https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo apt-key add -
sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-focal.list https://packages.lunarg.com/vulkan/lunarg-vulkan-focal.list`

- run `sudo apt-get update`

- run `sudo apt-get install vulkan-sdk`

- run `sudo apt-get install git-all curl libxcb-glx0-dev libsdl2-dev`

- Get git-lfs package from https://packagecloud.io/github/git-lfs/install

    - e.g for Ubuntu Focal Fossa (20.04): 
    - `curl -s https://packagecloud.io/install/repositories/github/git-lfs/script.deb.sh | sudo bash`
    - `sudo apt-get install git-lfs`

2. Fork repository `git fork https://github.com/1runeberg/OpenXRProvider_v2.git`

3. `cd OpenXRProvider_v2`

4. Initialize submodules `git submodule update --init --recursive`

5. Initialize git lfs `git lfs install`

6. `mkdir build`

7. `cd build`

8. `cmake ..`

9. `cmake --build .` or open root folder of git repository in Visual Studio Code and run automated build tasks or run  `cmake --build .` in VSCode's built-in terminal
