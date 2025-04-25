# PowerShare
$(call soong_config_set,lineage_powershare,powershare_path,/sys/class/power_supply/wireless/device/rtx)

PRODUCT_PACKAGES += \
    vendor.lineage.powershare-service.default

BOARD_SEPOLICY_DIRS += hardware/google/pixel-sepolicy/powershare
