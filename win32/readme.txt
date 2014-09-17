patches under win32 (mingw32), NOT android

sdl_patch      --- sdl patch for console output.
ffmpeg_patch   --- need sdl_patch, for examples build.
musicPlayer    --- modified from ffplay & https://github.com/lnmcc/musicPlayer
modSDL-1.2.15  --- mod of musicPlayer & sdl (mini version of SDL modified by me)
videoPlayer    --- modified from ffplay & https://github.com/lnmcc/videoPlayer
picPlayer      --- mod of videoPlayer & sdl 
oglPlayer      --- mod of picPlayer, using opengl and mini version of SDL modified by me

NOTE: The ffmpeg/lib/*.a files are deleted, they are built from ffmpeg_patch 
(copy them from msys\local\lib after executing 'make install')
The glut/include/*.h and glut/lib/*.a files are from dev-c++.

