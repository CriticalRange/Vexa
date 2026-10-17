adb -s TSORX4OB45L779WO shell am force-stop com.critical.vexaemulator
sleep 1
adb -s TSORX4OB45L779WO shell am start -n com.critical.vexaemulator/.GameActivity --es programId ets2
