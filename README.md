# PianoFromAbove

This is the software that drives some of the Impossible MIDI videos on youtube:

<p align="center">
  <a href="https://www.youtube.com/watch?v=p_c6uQHlhZ0" target="_blank">
    <img src="https://img.youtube.com/vi/p_c6uQHlhZ0/hqdefault.jpg"/>
  </a>
</p>

Synthesia is the main alternate with way more market share and much more active development, but appaerently PFA is more performant, so some prefer it. Yay.

The original inspiration:

<p align="center">
  <a href="https://www.youtube.com/watch?v=mTS16klgqMU" target="_blank">
    <img src="https://img.youtube.com/vi/mTS16klgqMU/hqdefault.jpg"/>
  </a>
</p>

And so I made it happen:


<p align="center">
  <a href="https://www.youtube.com/watch?v=PWQj61p6D5s" target="_blank">
    <img src="https://img.youtube.com/vi/PWQj61p6D5s/hqdefault.jpg"/>
  </a>
</p>

## Binaries

https://github.com/brian-pantano/PianoFromAbove/releases

## Viz branch

There's now a viz branch which will house graphics and performance updates going forward (if there is a forward).

## How to build

This is unfortunately very tricky. Hopefully I will simplify this in the future.

* clone this repo
* Download and install VisualStudio 2010
* Download and install Direct X SDK
* Download and extract Google Protocol Buffers 2.5
  * Build libprotobuf-lite.vcproj
* Download and extract Boost 1.55
* Open the .sln and edit the VC++ Directories from the project properties so that the Include Directories and Library Directories point to the location of your boost and protocol buffers downloads
* Cross fingers
* Build! (Release, x64)

Once that's done, there should be a Release\PFA-1.1.0-x86_64.exe that you can run.

There's an optional .nsi script that you can run if you want to build an installer.

The code probably isn't the best, and it probably goes against all sorts of best practices but it is fairly snappy. I'm not very good at writing UI or UX, but I am fairly good at writing datastructures and writing minimal and fast code. Good luck reading it! 
