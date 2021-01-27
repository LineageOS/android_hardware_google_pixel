# Dauntless
PRODUCT_PACKAGES += \
    citadeld \
    citadel_updater

# init scripts (won't be in AOSP)
-include vendor/google_nos/init/dauntless/init.mk

ifneq ($(wildcard vendor/google_nos/provision),)
PRODUCT_PACKAGES_DEBUG += CitadelProvision
endif
