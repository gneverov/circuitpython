// For debugging mbedtls - also set
// Debug level (0-4) 1=warning, 2=info, 3=debug, 4=verbose
// #define MODUSSL_MBEDTLS_DEBUG_LEVEL 1

// #define MICROPY_HW_PIN_EXT_COUNT    CYW43_WL_GPIO_COUNT

#define MICROPY_HW_PIN_RESERVED(i) ((i) == CYW43_PIN_WL_HOST_WAKE || (i) == CYW43_PIN_WL_REG_ON)
