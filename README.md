# PianoFromAbove

Welcome to my viz branch. This includes all the changes of upstream's viz branch, but with other hacky tweaks included!

This is currently (much) faster than upstream viz.

## How to build

* Clone this repo
* Download and install Visual Studio 2019
  * Make sure to install the Clang compiler and tools, too
* Download and install Direct X SDK
* Retarget project to your installed Windows SDK version
* Cross fingers
* Build! (Release, x64)

Once that's done, there should be a Release\PFA-1.1.0viz-x86_64.exe that you can run.

Remember to WinMM patch it!
