PRODUCT_COPY_FILES += \
      hardware/google/pixel/common/fstab.firmware:$(TARGET_COPY_OUT_VENDOR)/etc/fstab.firmware \
      hardware/google/pixel/common/fstab.persist:$(TARGET_COPY_OUT_VENDOR)/etc/fstab.persist \
      hardware/google/pixel/common/init.firmware.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.firmware.rc

BOARD_SEPOLICY_DIRS += hardware/google/pixel-sepolicy/common
