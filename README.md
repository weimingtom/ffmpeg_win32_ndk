ffmpeg_win32_ndk
================

ffmpeg win32 ndk build tool and test projects  



(1) Build Tools
* Windows XP  
* ossbuild msys (MinGW)  
	https://code.google.com/p/ossbuild/  
	https://code.google.com/p/ossbuild/downloads/detail?name=msys_v11.7z  
* JDK6  
* android-ndk-r9c  
	https://developer.android.com/tools/sdk/ndk/index.html  
* android-sdk
	https://developer.android.com/sdk/index.html  
* apache-ant-1.8.1  
	http://ant.apache.org/  

(2) Build Environment variables (edit manually):  
* ffmpeg\1.2.4\002_unzip_ffmpeg_src_and_patch.bat  
* ffmpeg\1.2.4\003_android_sdk_ndk_env.bat  
* projects\VideoProject\002_android_sdk_ndk_env.bat  
* test\001_android_sdk_ndk_env.bat  

(3) Build APK  
* Build JNI code  
	see ffmpeg\1.2.4\003_android_sdk_ndk_env.bat  
* Build Java code and APK  
	see projects\VideoProject\002_android_sdk_ndk_env.bat  

(4) Test  
* Copy test video file to device  
	see test\001_android_sdk_ndk_env.bat  
	see ffmpeg\1.2.4\jni\native.c  
* Run VideoDemo App  

(5) Prebuild:  
* see script\ffmpeg-1.2.4.7_build_noasm_mini  
* I use ossbuild msys (MinGW) to execute build-config.sh  

(6) References:  
* http://www.ffmpeg.org/  
* https://github.com/clzhan/ffmpeg-ffmpeg-1.2  
* https://github.com/weikunlu/VideoProject  

(7) Problems:  
* I use SYSROOT_INC in Android.mk to pass compiling libavutil/parseutils.c (because of the file time.h in the same directory)
* No asm and optimization, ONLY FOR ffmpeg study on Android  

(8) LICENSE:  
* see ffmpeg source (ffmpeg\1.2.4\ffmpeg-1.2.4.tar.gz)
