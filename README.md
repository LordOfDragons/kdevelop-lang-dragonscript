# kdevelop-lang-dragonscript
DragonScript Language Plugin for KDevelop

- Website: https://dragondreams.ch/?page_id=152
- Source Code Repository: https://github.com/LordOfDragons/kdevelop-lang-dragonscript

# Features
Shows Declarations, Uses and Class Outlining information
![image1](https://dragondreams.ch/wp-content/uploads/2020/10/kdevlangds1.png)

Supports Code Completion with generating function calls and code block calls.
![image1](https://dragondreams.ch/wp-content/uploads/2020/10/kdevlangds4.gif)

Supports Code Generation for example easily overriding functions.
![image1](https://dragondreams.ch/wp-content/uploads/2020/10/kdevlangds2.png)

More features to come in the future.

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
