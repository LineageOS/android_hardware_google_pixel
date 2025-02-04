# PowerShare
TARGET_POWERSHARE_PATH := /sys/class/power_supply/wireless/device/rtx

PRODUCT_PACKAGES += \
    vendor.lineage.powershare@1.0-service.default

BOARD_SEPOLICY_DIRS += hardware/google/pixel-sepolicy/powershare
