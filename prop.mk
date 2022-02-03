#
# system.prop for shamrock
#

# Audio
PRODUCT_PROPERTY_OVERRIDES += \
af.fast_track_multiplier=1 \
audio.deep_buffer.media=true \
audio.offload.min.duration.secs=30 \
audio.offload.video=true \
persist.vendor.btstack.enable.splita2dp=false \
persist.vendor.audio.fluence.speaker=true \
persist.vendor.audio.fluence.voicecall=true \
persist.vendor.audio.fluence.voicerec=false \
persist.vendor.audio.hw.binder.size_kbyte=1024 \
persist.vendor.audio.speaker.prot.enable=false \
ro.vendor.audio.sdk.fluencetype=fluence \
ro.vendor.audio.sdk.ssr=false \
vendor.audio.dolby.ds2.enabled=false \
vendor.audio.dolby.ds2.hardbypass=false \
vendor.audio.flac.sw.decoder.24bit=true \
vendor.audio.hw.aac.encoder=true \
vendor.audio.offload.buffer.size.kb=64 \
vendor.audio.offload.gapless.enabled=true \
vendor.audio.offload.multiaac.enable=true \
vendor.audio.offload.multiple.enabled=false \
vendor.audio.offload.passthrough=false \
vendor.audio.offload.track.enable=true \
vendor.audio.parser.ip.buffer.size=262144 \
vendor.audio.playback.mch.downsample=true \
vendor.audio.pp.asphere.enabled=false \
vendor.audio.safx.pbe.enabled=true \
vendor.audio.tunnel.encode=false \
vendor.audio.use.sw.alac.decoder=true \
vendor.audio.use.sw.ape.decoder=true \
vendor.audio_hal.period_size=192 \
vendor.voice.conc.fallbackpath=deep-buffer \
vendor.voice.path.for.pcm.voip=true \
vendor.voice.playback.conc.disabled=true \
vendor.voice.record.conc.disabled=false \
vendor.voice.voip.conc.disabled=true

# Bluetooth
PRODUCT_PROPERTY_OVERRIDES += \
bluetooth.hfp.client=1 \
qcom.bluetooth.soc=smd \
ro.bluetooth.hfp.ver=1.7 \
ro.qualcomm.bt.hci_transport=smd

# Camera
PRODUCT_PROPERTY_OVERRIDES += \
camera.display.lmax=1280x720 \
camera.display.umax=1920x1080 \
camera.hal1.packagelist=com.skype.raider,com.google.android.talk \
camera.lowpower.record.enable=1 \
media.camera.ts.monotonic=1 \
persist.camera.gyro.disable=0 \
persist.camera.isp.clock.optmz=0 \
persist.camera.stats.test=5 \
persist.vendor.qti.telephony.vt_cam_interface=2 \
vidc.enc.dcvs.extra-buff-count=2

# Cne/Dpm
PRODUCT_PROPERTY_OVERRIDES += \
persist.vendor.cne.feature=1 \
persist.dpm.feature=1 \
persist.vendor.sys.cnd.iwlan=1

# Display
PRODUCT_PROPERTY_OVERRIDES += \
debug.hwui.use_buffer_age=false \
debug.sf.disable_backpressure=1 \
debug.sf.enable_hwc_vds=1 \
debug.sf.hw=0 \
debug.sf.latch_unsignaled=1 \
persist.demo.hdmirotationlock=false \
ro.surface_flinger.force_hwc_copy_for_virtual_displays=true \
ro.surface_flinger.max_virtual_display_dimension=4096 \
ro.hardware.egl=adreno \
ro.hardware.vulkan=adreno \
ro.opengles.version=196608 \
vendor.display.disable_skip_validate=1 \
ro.vendor.display.cabl=0 \
sdm.debug.disable_skip_validate=1 \
vendor.display.disable_skip_validate=1 \
vendor.display.enable_default_color_mode=1 \
vendor.gralloc.enable_fb_ubwc=1

# Memory optimizations
PRODUCT_PROPERTY_OVERRIDES += \
ro.vendor.qti.sys.fw.bservice_enable=true \
ro.config.fha_enable=true \
ro.sys.fw.bg_apps_limit=32 \
ro.config.dha_cached_max=16 \
ro.config.dha_empty_max=42 \
ro.config.dha_empty_init=32 \
ro.config.dha_lmk_scale=0.545 \
ro.config.dha_th_rate=2.3 \
ro.config.sdha_apps_bg_max=64 \
ro.config.sdha_apps_bg_min=8

# Perf Ux IOPrefetcher
PRODUCT_PROPERTY_OVERRIDES += \
vendor.enable_prefetch=1 \
vendor.iop.enable_uxe=1 \
vendor.iop.enable_prefetch_ofr=1 \
vendor.perf.iop_v3.enable=1 \
persist.vendor.qti.games.gt.prof=1

# DRM
PRODUCT_PROPERTY_OVERRIDES += \
drm.service.enabled=true

# GPS
PRODUCT_PROPERTY_OVERRIDES += \
persist.gps.qc_nlp_in_use=1 \
persist.loc.nlp_name=com.qualcomm.location \
ro.gps.agps_provider=1

# LMKD
PRODUCT_PRODUCT_PROPERTIES += \
ro.lmk.low=1001 \
ro.lmk.medium=0 \
ro.lmk.critical=0 \
ro.lmk.critical_upgrade=false \
ro.lmk.upgrade_pressure=100 \
ro.lmk.downgrade_pressure=100 \
ro.lmk.kill_heaviest_task=true \
ro.lmk.kill_timeout_ms=100 \
ro.lmk.use_minfree_levels=true \
ro.lmk.log_stats=true

# Media
PRODUCT_PROPERTY_OVERRIDES += \
av.debug.disable.pers.cache=1 \
media.msm8956hw=1 \
mm.enable.sec.smoothstreaming=true \
mmp.enable.3g2=true \
media.aac_51_output_enabled=true \
mm.enable.qcom_parser=4194303

# Perf
PRODUCT_PROPERTY_OVERRIDES += \
dalvik.vm.boot-dex2oat-threads=8 \
ro.sys.fw.dex2oat_thread_count=8 \
dalvik.vm.bg-dex2oat-threads=2 \
dalvik.vm.dex2oat-threads=6 \
ro.vendor.qti.am.reschedule_service=true \
ro.vendor.qti.core_ctl_min_cpu=1 \
ro.vendor.qti.core_ctl_max_cpu=4 \
ro.vendor.at_library=libqti-at.so \
ro.vendor.extension_library=libqti-perfd-client.so \
ro.vendor.gt_library=libqti-gt.so \
vendor.perf.gestureflingboost.enable=true

# RAM optimizations
PRODUCT_PROPERTY_OVERRIDES += \
ro.vendor.qti.sys.fw.bservice_enable=true \
ro.vendor.qti.sys.fw.bservice_limit=5 \
ro.vendor.qti.sys.fw.bservice_age=5000 \
ro.vendor.qti.sys.fw.use_trim_settings=true \
ro.vendor.qti.sys.fw.empty_app_percent=50 \
ro.vendor.qti.sys.fw.trim_empty_percent=100 \
ro.vendor.qti.sys.fw.trim_cache_percent=100 \
ro.vendor.qti.sys.fw.trim_enable_memory=2147483648

# Netmgrd
PRODUCT_PROPERTY_OVERRIDES += \
ro.vendor.use_data_netmgrd=true \
persist.data.netmgrd.qos.enable=true \
persist.vendor.data.mode=concurrent

# Nitz
PRODUCT_PROPERTY_OVERRIDES += \
persist.rild.nitz_plmn="" \
persist.rild.nitz_long_ons_0="" \
persist.rild.nitz_long_ons_1="" \
persist.rild.nitz_long_ons_2="" \
persist.rild.nitz_long_ons_3="" \
persist.rild.nitz_short_ons_0="" \
persist.rild.nitz_short_ons_1="" \
persist.rild.nitz_short_ons_2="" \
persist.rild.nitz_short_ons_3=""

# Qualcomm
PRODUCT_PROPERTY_OVERRIDES += \
ro.vendor.qti.va_aosp.support=1 \
ro.vendor.qti.va_odm.support=1

# Radio
PRODUCT_PROPERTY_OVERRIDES += \
DEVICE_PROVISIONED=1 \
persist.dbg.volte_avail_ovr=1 \
persist.dbg.vt_avail_ovr=1 \
persist.dbg.wfc_avail_ovr=1 \
persist.sys.fflag.override.settings_network_and_internet_v2=true \
persist.vendor.radio.apm_sim_not_pwdn=1 \
persist.radio.aosp_usr_pref_sel=true \
persist.radio.multisim.config=dsds \
persist.vendor.radio.custom_ecc=1 \
persist.vendor.radio.procedure_bytes=SKIP \
persist.vendor.radio.rat_on=combine \
persist.vendor.radio.sib16_support=1 \
persist.vendor.data.iwlan.enable=true \
ril.subscription.types=NV,RUIM \
rild.libargs=-d/dev/smd0 \
rild.libpath=/vendor/lib64/libril-qc-qmi-1.so \
ro.telephony.call_ring.multiple=false \
ro.telephony.default_network=22,22 \
ro.telephony.iwlan_operation_mode=legacy \
service.qti.ims.enabled=1

# Time Services
PRODUCT_PROPERTY_OVERRIDES += \
persist.timed.enable=true

# SurfaceFlinger
PRODUCT_PROPERTY_OVERRIDES += \
ro.surface_flinger.protected_contents=true \
ro.surface_flinger.max_frame_buffer_acquired_buffers=3 \
debug.sf.early_phase_offset_ns=1500000 \
debug.sf.early_app_phase_offset_ns=1500000 \
debug.sf.early_gl_phase_offset_ns=3000000 \
debug.sf.early_gl_app_phase_offset_ns=15000000

# UX
PRODUCT_PROPERTY_OVERRIDES += \
sys.use_fifo_ui=1

# Tcp
PRODUCT_PROPERTY_OVERRIDES += \
net.tcp.2g_init_rwnd=10

# Wifi
PRODUCT_PROPERTY_OVERRIDES += \
wifi.interface=wlan0
