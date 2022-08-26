PRODUCT_SOONG_NAMESPACES += vendor/google_nos/init/dauntless
# Dauntless
PRODUCT_PACKAGES += \
    citadeld \
    citadel_updater \
    android.hardware.weaver@1.0-service.citadel \
    android.hardware.identity@1.0-service.citadel \
    init_citadel

# AIDL
PRODUCT_PACKAGES += \
    android.hardware.security.keymint-service.citadel

PRODUCT_COPY_FILES += \
    hardware/google/pixel/dauntless/android.hardware.strongbox_keystore.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.strongbox_keystore.xml

ifneq ($(wildcard vendor/google_nos/provision),)
PRODUCT_PACKAGES_DEBUG += CitadelProvision
endif

# USERDEBUG ONLY: Install test packages
ifneq (,$(filter userdebug eng,$(TARGET_BUILD_VARIANT)))
PRODUCT_PACKAGES_DEBUG += citadel_integration_tests \
                          pwntest \
                          nugget_targeted_tests
endif
