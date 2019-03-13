# PianoFromAbove

Welcome to the viz branch. This version removes all learning and scoring logic. It plays midis. This simplifies rendering such that performance optimizations and graphics upgrades should be a lot easier to implement going forward.

As of right now, this is not faster nor slower than master.

## How to build

Almost the same as the main branch, except boost is no longer used.

* clone this repo
* Download and install VisualStudio 2010
* Download and install Direct X SDK
* Download and extract Google Protocol Buffers 2.5
  * Build libprotobuf-lite.vcproj
* Open the .sln and edit the VC++ Directories from the project properties so that the Include Directories and Library Directories point to the location of your protocol buffers download
* Cross fingers
* Build! (Release, x64)

Once that's done, there should be a Release\PFA-1.1.0viz-x86_64.exe that you can run.
