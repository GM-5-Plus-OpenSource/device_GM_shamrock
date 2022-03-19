# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


def IncrementalOTA_InstallEnd(info):
  ImportMainBootloaderFirmware(info)

def FullOTA_InstallEnd(info):
  ImportMainBootloaderFirmware(info)

def ImportMainBootloaderFirmware(info):
  info.script.AppendExtra('ui_print("Flashing sbl1...");')
  info.script.AppendExtra('package_extract_file("install/firmware-update/sbl1.img", "/dev/block/bootdevice/by-name/sbl1");')
  info.script.AppendExtra('package_extract_file("install/firmware-update/sbl1.img", "/dev/block/bootdevice/by-name/sbl1bak");')
  info.script.AppendExtra('ui_print("Flashing aboot...");')
  info.script.AppendExtra('package_extract_file("install/firmware-update/emmc_appsboot.mbn", "/dev/block/bootdevice/by-name/aboot");')
  info.script.AppendExtra('package_extract_file("install/firmware-update/emmc_appsboot.mbn", "/dev/block/bootdevice/by-name/abootbak");')
  info.script.AppendExtra('ui_print("Flashing rpm...");')
  info.script.AppendExtra('package_extract_file("install/firmware-update/rpm.img", "/dev/block/bootdevice/by-name/rpm");')
  info.script.AppendExtra('package_extract_file("install/firmware-update/rpm.img", "/dev/block/bootdevice/by-name/rpmbak");')
  info.script.AppendExtra('ui_print("Flashing tz...");')
  info.script.AppendExtra('package_extract_file("install/firmware-update/tz.img", "/dev/block/bootdevice/by-name/tz");')
  info.script.AppendExtra('package_extract_file("install/firmware-update/tz.img", "/dev/block/bootdevice/by-name/tzbak");')
  info.script.AppendExtra('ui_print("Flashing hyp...");')
  info.script.AppendExtra('package_extract_file("install/firmware-update/hyp.mbn", "/dev/block/bootdevice/by-name/hyp");')
  info.script.AppendExtra('package_extract_file("install/firmware-update/hyp.mbn", "/dev/block/bootdevice/by-name/hypbak");')
  info.script.AppendExtra('ui_print("Flashing cmnlib...");')
  info.script.AppendExtra('package_extract_file("install/firmware-update/cmnlib.mbn", "/dev/block/bootdevice/by-name/cmnlib");')
  info.script.AppendExtra('package_extract_file("install/firmware-update/cmnlib.mbn", "/dev/block/bootdevice/by-name/cmnlibbak");')
  info.script.AppendExtra('ui_print("Flashing keymaster...");')
  info.script.AppendExtra('package_extract_file("install/firmware-update/keymaster.mbn", "/dev/block/bootdevice/by-name/keymaster");')
  info.script.AppendExtra('package_extract_file("install/firmware-update/keymaster.mbn", "/dev/block/bootdevice/by-name/keymasterbak");')
  info.script.AppendExtra('ui_print("Flashing other images...");')
  info.script.AppendExtra('package_extract_file("install/firmware-update/mdtp.img", "/dev/block/bootdevice/by-name/mdtp");')
  info.script.AppendExtra('package_extract_file("install/firmware-update/dsp.img", "/dev/block/bootdevice/by-name/dsp");')
  info.script.AppendExtra('package_extract_file("install/firmware-update/devinfo.bin", "/dev/block/bootdevice/by-name/devinfo");')
  info.script.AppendExtra('package_extract_file("install/firmware-update/splash.img", "/dev/block/bootdevice/by-name/splash");')
  info.script.AppendExtra('package_extract_file("install/firmware-update/NON-HLOS.bin", "/dev/block/bootdevice/by-name/modem");')
  info.script.AppendExtra('ui_print("Firmware flashing done.");')
