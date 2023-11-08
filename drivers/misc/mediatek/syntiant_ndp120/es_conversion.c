#include <linux/types.h>
#include <syntiant_ilib/syntiant_ndp.h>
#include <syntiant_ilib/syntiant_ndp10x.h>

const char *
syntiant_ndp10x_config_dnn_input_s(enum syntiant_ndp10x_config_dnn_input_e e)
{
    switch(e) {
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_NONE:       return "none";
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM0:       return "pdm0";
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM1:       return "pdm1";
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM_SUM:    return "pdm sum";
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_LEFT:   return "i2s left";
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_RIGHT:  return "i2s right";
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_SUM:    return "i2s sum";
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_MONO:   return "i2s mono";
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_DIRECT: return "i2s direct";
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_SPI:        return "spi";
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_SPI_DIRECT: return "spi direct";
        default: return "error";
    }
}

const char *
syntiant_ndp10x_config_tank_input_s(enum syntiant_ndp10x_config_tank_input_e e)
{
    switch(e) {
        case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_NONE:        return "none";
        case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM0:        return "pdm0";
        case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM1:        return "pdm1";
        case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM_SUM:     return "pdm sum";
        case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM_BOTH:    return "pdm both";
        case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM0_RAW:    return "pdm0 raw";
        case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM1_RAW:    return "pdm1 raw";
        case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM_BOTH_RAW:
                                                      return "pdm both raw";
        case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_LEFT:
                                                      return "i2s left";
        case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_RIGHT:
                                                      return "i2s right";
        case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_MONO:
                                                      return "i2s mono";
        case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_SUM:     return "i2s sum";
        case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_BOTH:
                                                      return "i2s both";
        case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_SPI:         return "spi";
        case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_FILTER_BANK:
                                                      return "filter bank";
        case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_DNN:         return "dnn";
        default: return "error";
    }
}

const char *
syntiant_ndp10x_memory_s(enum syntiant_ndp10x_config_memory_e e)
{
    switch (e) {
        case SYNTIANT_NDP10X_CONFIG_MEMORY_BOOTROM:     return "bootrom";
        case SYNTIANT_NDP10X_CONFIG_MEMORY_RAM:         return "ram";
        case SYNTIANT_NDP10X_CONFIG_MEMORY_BOOTRAM:     return "bootram";
        case SYNTIANT_NDP10X_CONFIG_MEMORY_DNNFEATURE:  return "dnnfeature";
        case SYNTIANT_NDP10X_CONFIG_MEMORY_DNN00:       return "dnn00";
        case SYNTIANT_NDP10X_CONFIG_MEMORY_DNN01:       return "dnn01";
        case SYNTIANT_NDP10X_CONFIG_MEMORY_DNN02:       return "dnn02";
        case SYNTIANT_NDP10X_CONFIG_MEMORY_DNN03:       return "dnn03";
        case SYNTIANT_NDP10X_CONFIG_MEMORY_DNN04:       return "dnn04";
        case SYNTIANT_NDP10X_CONFIG_MEMORY_DNN05:       return "dnn05";
        case SYNTIANT_NDP10X_CONFIG_MEMORY_DNN06:       return "dnn06";
        case SYNTIANT_NDP10X_CONFIG_MEMORY_DNN07:       return "dnn07";
        case SYNTIANT_NDP10X_CONFIG_MEMORY_DNN08:       return "dnn08";
        case SYNTIANT_NDP10X_CONFIG_MEMORY_DNN09:       return "dnn09";
        case SYNTIANT_NDP10X_CONFIG_MEMORY_DNN10:       return "dnn10";
        case SYNTIANT_NDP10X_CONFIG_MEMORY_DNN11:       return "dnn11";
        case SYNTIANT_NDP10X_CONFIG_MEMORY_DNN12:       return "dnn12";
        case SYNTIANT_NDP10X_CONFIG_MEMORY_DNN13:       return "dnn13";
        case SYNTIANT_NDP10X_CONFIG_MEMORY_DNN14:       return "dnn14";
        case SYNTIANT_NDP10X_CONFIG_MEMORY_DNN15:       return "dnn15";
        case SYNTIANT_NDP10X_CONFIG_MEMORY_DNN16:       return "dnn16";
        case SYNTIANT_NDP10X_CONFIG_MEMORY_DNN17:       return "dnn17";
        default: return "error";
    }
}

const char *
syntiant_ndp10x_memory_power_s(enum syntiant_ndp10x_config_memory_power_e e)
{
    switch(e) {
        case SYNTIANT_NDP10X_CONFIG_MEMORY_POWER_NO_CHANGE: return "no change";
        case SYNTIANT_NDP10X_CONFIG_MEMORY_POWER_OFF:       return "off";
        case SYNTIANT_NDP10X_CONFIG_MEMORY_POWER_RETAIN:    return "retain";
        case SYNTIANT_NDP10X_CONFIG_MEMORY_POWER_ON:        return "on";
        default: return "error";
    }
}
