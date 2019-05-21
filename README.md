# Nebula engine

## Requirements
1. OS: Windows or Linux
2. Compiler with support for C++14.
3. GPU supporting Vulkan
4. [CMake 3.13+](https://cmake.org/download/)
5. [Vulkan SDK](https://vulkan.lunarg.com/sdk/home)
6. [Python 3.5+](https://www.python.org/downloads)
    * Python modules required:
        1. numpy
        2. jedi
        
        As root/admin: `python -m pip install numpy jedi`
    * Python requirements (Windows):
        1. Correct architecture (64-bit if you're building for 64-bit systems)
        2. Installed for all users
        3. Added to PATH
        4. Installed with debugging symbols and binaries

### Mono
To compile with mono you need the runtime installed.
On Windows, you might need to add the `bin` folder of your mono installation to `PATH`.
If CMake is having trouble finding Mono, try adding the root folder of your mono installation to an environment variable and call it `MONO_ROOT`.

## Setup
1. `./fips set config vulkan-win64-vstudio-debug` in your project directory

Remember to run `fips nebula` verb to set work and toolkit directory registry variables.