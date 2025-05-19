
# Introduction to SDL_ttf with Visual Studio

The easiest way to use SDL_ttf is to include it along with SDL as subprojects in your project.

First, make sure you have downloaded the external dependencies. You can do that by running `Get-GitModules.ps1` in the external directory.

We'll start by creating a simple project to build and run [hello.c](hello.c)

- Create a new project in Visual Studio, using the C++ Empty Project template
- Add hello.c to the Source Files
- Right click the solution, select add an existing project, navigate to the SDL VisualC/SDL directory and add SDL.vcxproj
- Right click the solution, select add an existing project, navigate to the SDL_ttf VisualC directory and add SDL_ttf.vcxproj
- Select your SDL_ttf project and go to Project -> Add Reference and select SDL3
- Select your SDL_ttf project and go to Project -> Properties, set the filter at the top to "All Configurations" and "All Platforms", select VC++ Directories and modify the default SDL path in "Include Directories" to point to your SDL include directories
- Select your main project and go to Project -> Add Reference and select SDL3 and SDL3_ttf
- Select your main project and go to Project -> Properties, set the filter at the top to "All Configurations" and "All Platforms", select VC++ Directories and add the SDL and SDL_ttf include directories to "Include Directories"
- Build and run!

