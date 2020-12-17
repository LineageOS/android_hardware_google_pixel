# Dauntless
PRODUCT_PACKAGES += \
    citadeld \
    citadel_updater \
    android.hardware.weaver@1.0-service.citadel \
    android.hardware.keymaster@4.1-service.citadel \
    android.hardware.identity@1.0-service.citadel \
    android.hardware.rebootescrow-service.citadel \
    wait_for_strongbox

# init scripts (won't be in AOSP)
-include vendor/google_nos/init/dauntless/init.mk

ifneq ($(wildcard vendor/google_nos/provision),)
PRODUCT_PACKAGES_DEBUG += CitadelProvision
endif
