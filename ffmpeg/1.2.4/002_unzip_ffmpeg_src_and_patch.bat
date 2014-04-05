@set path=D:\android_rtmp\ffmpeg\msys\bin;%path%
mkdir jni\ffmpeg-1.2.4
tar xzf ffmpeg-1.2.4.tar.gz
xcopy /e /y /i ffmpeg-1.2.4 jni\ffmpeg-1.2.4
rmdir /S /Q ffmpeg-1.2.4
xcopy /e /y /i ffmpeg-1.2.4_patch jni\ffmpeg-1.2.4
pause
