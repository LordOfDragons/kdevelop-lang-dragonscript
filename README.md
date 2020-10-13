# kdevelop-lang-dragonscript
DragonScript Language Plugin for KDevelop

# Compile
Required packages (Ubuntu):
- cmake
- extra-cmake-modules
- flex
- bison
- gettext
- kdevelop-dev
- kdevelop-pg-qt
- libkf5itemmodels-dev

Run (as root)
> mkdir build
> cmake ..
> make install

# Run-Time Requirements
For general use no additional runtime files are required.

For use with Drag\[en]gine Game Engine (https://github.com/LordOfDragons/dragengine)
you need to install the game engine libraries (package __libdragengine1__) of at
least version 1.3.1 for the plugin to be able to load the game engine scripts.
