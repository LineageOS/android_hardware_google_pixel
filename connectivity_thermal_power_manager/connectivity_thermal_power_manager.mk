SYSTEM_EXT_PRIVATE_SEPOLICY_DIRS += hardware/google/pixel-sepolicy/connectivity_thermal_power_manager

$(call soong_config_set,connectivity_thermal_power_manager_config,use_alcedo_modem,$(USES_ALCEDO_MODEM))
ifeq ($(USES_ALCEDO_MODEM),true)
PRODUCT_PACKAGES += ConnectivityThermalPowerManagerNextgen
PRODUCT_PACKAGES_DEBUG += mipc_util
else
PRODUCT_PACKAGES += ConnectivityThermalPowerManager
endif
