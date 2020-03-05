PRODUCT_COPY_FILES += \
      hardware/google/pixel/common/fstab.firmware:$(TARGET_COPY_OUT_VENDOR)/etc/fstab.firmware \
      hardware/google/pixel/common/fstab.persist:$(TARGET_COPY_OUT_VENDOR)/etc/fstab.persist \
      hardware/google/pixel/common/init.firmware.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.firmware.rc \
      hardware/google/pixel/common/init.insmod.sh:$(TARGET_COPY_OUT_VENDOR)/bin/init.insmod.sh

BOARD_SEPOLICY_DIRS += hardware/google/pixel-sepolicy/common
