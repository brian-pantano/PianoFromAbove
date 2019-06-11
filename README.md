# PianoFromAbove

Welcome to my viz branch. This includes all the changes of upstream's viz branch, but with other hacky tweaks included!

This is currently faster than upstream viz.

## How to build

* clone this repo
* Download and install Visual Studio 2019
* Download and install Direct X SDK
* Install protobuf:x64-windows through vcpkg
  * Copy `\buildtrees\protobuf\x64-windows-rel\libprotobuf-lite.lib` to `\installed\x64-windows\lib\`
  * Copy  `\buildtrees\protobuf\x64-windows-rel\libprotobuf-lite.dll` to `\installed\x64-windows\bin\`
* Cross fingers
* Build! (Release, x64)

Once that's done, there should be a Release\PFA-1.1.0viz-x86_64.exe that you can run.
