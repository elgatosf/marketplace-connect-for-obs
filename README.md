# OBS Plugin Template

## RICHARD- HEADER STUFF HERE

## Build Instructions
Currently the Elgato Marketplace Connect plugin can only be built on Windows. The plugin uses the OBS plugin template build system, and more detailed build instructions can be found on the [template project's wiki](https://github.com/obsproject/obs-plugintemplate/wiki).

### Build system requirements
Two sets of development tools are required. Ensure both are installed prior to configuring and building the plugin:

* Visual Studio 17 2022
* CMake 3.30.5

### Building the project
After making sure the required development tools are installed, building the plugin should be a straightforawrd process.

1. Configuration: in a terminal window, navigate to the root plugin folder, and configure the project files with: `cmake --preset windows-x64`. This will download any build dependencies and set up a Visual Studio project file in a new project directory called `build_x64`.
2. Inside the new `build_x64` directory added to the projects root, you will find a `elgato-marketplace-connect.sln` file. Open this file in Visual Studio to edit and build the project.
3. Alternatively, cmake can build the project directly with the command: `cmake --build --preset windows-x64`.

If you wish to debug the plugin within a running OBS, the (Project Wiki)[https://github.com/obsproject/obs-plugintemplate/wiki/How-To-Debug-Your-Plugin#debugging-plugins-with-visual-studio] has some great instructions. We recommend using the second variant (Adding the plugin to an OBS Studio Project), and when doing so, you will need to add the plugin project, plugin-support project, and the elgato-marketplace-connect-loader project to the solution explorer (step 2 in the instructions there).
