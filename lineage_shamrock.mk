#
# Copyright (C) 2022 The LineageOS Project
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
#

# Inherit from those products. Most specific first.
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base_telephony.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/product_launched_with_m.mk)

# Inherit some common lineage stuff
$(call inherit-product, vendor/lineage/config/common_full_phone.mk)

# Inherit from shamrock device
$(call inherit-product, $(LOCAL_PATH)/device.mk)

PRODUCT_BRAND := GM
PRODUCT_DEVICE := shamrock
PRODUCT_MANUFACTURER := General Mobile
PRODUCT_NAME := lineage_shamrock
PRODUCT_MODEL := GM 5 Plus

PRODUCT_GMS_CLIENTID_BASE := android-gm
TARGET_VENDOR := GM
TARGET_VENDOR_PRODUCT_NAME := shamrock
PRODUCT_BUILD_PROP_OVERRIDES += PRIVATE_BUILD_DESC="shamrock-user 8.0.0 OSR18O 572 release-keys"

# Set BUILD_FINGERPRINT variable to be picked up by both system and vendor build.prop
BUILD_FINGERPRINT := gm/shamrock/shamrock:8.0.0/OSR18O/572:user/test-keys
