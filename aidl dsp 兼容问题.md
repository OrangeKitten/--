 **版本** GAC_T75_IDC_SA_260402_1831D_D

 dsp 接口名称 vendor.idc.hardware.display-V1-ndk.so

## 现象1
system 目录下存在 vendor.idc.hardware.dsp-V1-ndk.so
![[Pasted image 20260407135706.png|591]]
vendor 目录下不存在
![[Pasted image 20260407135735.png]]

## 现象2
system/etc/vintf
![[Pasted image 20260407141811.png]]
vendor/etc/vintf
![[Pasted image 20260407141901.png]]

#  /vendor/./etc/vintf/manifest/android.hardware.dsp@1.0-manifest.xml .  板子里面的
 
![[Pasted image 20260407150929.png|1378]]


# 恢复操作
1.把system的vendor.idc.hardware.dsp-V1-ndk.so 拷贝到vendor区
2.把/vendor/./etc/vintf/manifest/android.hardware.dsp@1.0-manifest.xml  修改如下
```
<!-- Input: vendor/desaysv/common/hardware/interfaces/dsp/1.0/default/android.hardware.dsp@1.0-manifest.xml -->
 <manifest version="9.0" type="device"> 
 <hal format="aidl">
  <name>vendor.idc.hardware.dsp</name> 
  <fqname>IAudioControl/default</fqname>
   </hal> </manifest>
```