#!/system/bin/sh
i=1
while [ $i -le 10000000 ]
do
let i+=1
am start -n com.android.camera2/com.android.camera.CameraLauncher
sleep 5
input keyevent 3
done