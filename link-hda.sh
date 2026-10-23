#!/bin/sh
set -eu
cd /boot/home/haiku
base=generated/objects/haiku/x86_64/release
out="$base/add-ons/kernel/drivers/audio/hda/hda"
gcc -fno-strict-aliasing -fno-delete-null-pointer-checks \
  -Xlinker --no-undefined -shared -nostdlib -Xlinker --no-undefined \
  -Xlinker -soname=hda -z max-page-size=0x1000 -o "$out" \
  /boot/system/develop/tools/lib/gcc/x86_64-unknown-haiku/13.3.0/crtbeginS.o \
  "$base/system/glue/haiku_version_glue.o" \
  "$base/add-ons/kernel/drivers/audio/hda/driver.o" \
  "$base/add-ons/kernel/drivers/audio/hda/device.o" \
  "$base/add-ons/kernel/drivers/audio/hda/hda_multi_audio.o" \
  "$base/add-ons/kernel/drivers/audio/hda/hda_controller.o" \
  "$base/add-ons/kernel/drivers/audio/hda/hda_codec.o" \
  "$base/system/kernel/kernel.so" \
  generated/build_packages/gcc_syslibs_devel-13.3.0_2023_08_10-6-x86_64/develop/lib/libgcc-kernel.a \
  /boot/system/develop/tools/lib/gcc/x86_64-unknown-haiku/13.3.0/crtendS.o
ls -l "$out"
