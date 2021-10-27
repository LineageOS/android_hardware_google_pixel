PRODUCT_COPY_FILES += \
      hardware/google/pixel/common/init.pixel.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.pixel.rc

BOARD_SEPOLICY_DIRS += hardware/google/pixel-sepolicy/common

# Write flags to the vendor space in /misc partition.
PRODUCT_PACKAGES += \
    misc_writer

# Enable atrace hal and tools for pixel devices
PRODUCT_PACKAGES += \
    android.hardware.atrace@1.0-service.pixel \
    dmabuf_dump

# fastbootd
PRODUCT_PACKAGES += \
    fastbootd

# Common ramdump file type.
BOARD_VENDOR_SEPOLICY_DIRS += hardware/google/pixel-sepolicy/ramdump/common

# Pixel Experience
PRODUCT_PACKAGES_DEBUG += wifi_sniffer
BOARD_VENDOR_SEPOLICY_DIRS += hardware/google/pixel-sepolicy/wifi_sniffer

# turbo Adapter
PRODUCT_SOONG_NAMESPACES += vendor/unbundled_google/packages/TurboAdapter
BOARD_SEPOLICY_DIRS += hardware/google/pixel-sepolicy/turbo_adapter
PRODUCT_PACKAGES += TurboAdapter

include hardware/google/pixel/google_battery/google_battery_hal.mk
