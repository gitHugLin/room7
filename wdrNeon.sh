#！/bin/bash
ndk-build
adb root
adb remount
adb shell input keyevent 3
# adb push /Users/linqi/WorkDir/NormalPro/wdr-neonMode/obj/local/armeabi-v7a/test-wdr /system/bin
adb push /Users/linqi/WorkDir/NormalPro/wdr-neonMode/obj/local/armeabi-v7a/librkwdr.so /system/lib
# adb shell chmod 777 /system/bin/test-wdr
# adb shell /system/bin/test-wdr
adb shell stop media
adb shell start media
sleep 1
adb shell am start -n com.android.camera2/com.android.camera.CameraLauncher
# adb pull /sdcard/grayImage.jpg ./pictures/
#adb pull /data/local/srcImage.jpg ./
#adb pull /data/local/avgLum.jpg ./
#adb pull /data/local/nonLum.jpg ./
#adb pull /data/local/maxLum.jpg ./
#adb logcat *:E
adb logcat -s wdr
adb logcat -c
