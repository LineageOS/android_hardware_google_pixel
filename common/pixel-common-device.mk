PRODUCT_COPY_FILES += \
      hardware/google/pixel/common/fstab.firmware:$(TARGET_COPY_OUT_VENDOR)/etc/fstab.firmware \
      hardware/google/pixel/common/fstab.persist:$(TARGET_COPY_OUT_VENDOR)/etc/fstab.persist \
      hardware/google/pixel/common/init.pixel.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.pixel.rc \
      hardware/google/pixel/common/init.insmod.sh:$(TARGET_COPY_OUT_VENDOR)/bin/init.insmod.sh

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
