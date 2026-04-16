#include "RC_Controller_Header.h"
#include <stdio.h>

static unsigned char data[DATA_SIZE];

#if RECEIVER_OR_TRANSMITTER
// ADC libraries =====================
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
// ===================================

#define ADC1_CHAN0 ADC_CHANNEL_4
#define ADC1_CHAN1 ADC_CHANNEL_5
#define ADC1_CHAN2 ADC_CHANNEL_7

#define ADC_ATTEN ADC_ATTEN_DB_12

static bool calibrated[3];
static int adc_raw[3];
static int voltage[3];


static adc_oneshot_unit_handle_t adc1_handle;

static adc_oneshot_unit_init_cfg_t init_config1 = {
    .unit_id = ADC_UNIT_1,
};

static adc_oneshot_chan_cfg_t config = {
    .bitwidth = ADC_BITWIDTH_DEFAULT,
    .atten = ADC_ATTEN,
};
    
static adc_cali_handle_t adc1_cali_chan0_handle = NULL;
static adc_cali_handle_t adc1_cali_chan1_handle = NULL;
static adc_cali_handle_t adc1_cali_chan2_handle = NULL;

static void init_adc();
static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);
#endif


void init_rc_controller(void){
    configure_transceiver(); 

    #if RECEIVER_OR_TRANSMITTER
    init_adc();


    while (1){
        adc_oneshot_read(adc1_handle, ADC1_CHAN0, &adc_raw[0]);
        if (calibrated[0]) {
            adc_cali_raw_to_voltage(adc1_cali_chan0_handle, adc_raw[0], &voltage[0]);
        }

        adc_oneshot_read(adc1_handle, ADC1_CHAN1, &adc_raw[1]);
        if (calibrated[1]) {
            adc_cali_raw_to_voltage(adc1_cali_chan1_handle, adc_raw[1], &voltage[1]);
        }

        adc_oneshot_read(adc1_handle, ADC1_CHAN2, &adc_raw[2]);
        if (calibrated[2]) {
            adc_cali_raw_to_voltage(adc1_cali_chan2_handle, adc_raw[2], &voltage[2]);
        }
               
        //printf("x1 = %d\t y1 = %d\t y2 = %d\n", voltage[0], voltage[1], voltage[2]);
        data[0] = (voltage[0] >> 8) & 0xFF;
        data[1] = (voltage[1] >> 8) & 0xFF;
        data[2] = (voltage[2] >> 8) & 0xFF;

        //printf("x1 = %d\t y1 = %d\t y2 = %d\n", data[0], data[1], data[2]);
                
        nrf_send(data);
    }

    #else
    /* 
    while (1){
        nrf_read(data);
        //printf("x1 = %d\t y1 = %d\t y2 = %d\t\n", data[0], data[1], data[2]);
    }*/
    #endif 
}


#if RECEIVER_OR_TRANSMITTER
static void init_adc(){
    adc_oneshot_new_unit(&init_config1, &adc1_handle);
    
    adc_oneshot_config_channel(adc1_handle, ADC1_CHAN0, &config);
    adc_oneshot_config_channel(adc1_handle, ADC1_CHAN1, &config);
    adc_oneshot_config_channel(adc1_handle, ADC1_CHAN2, &config);
    
    calibrated[0] = example_adc_calibration_init(ADC_UNIT_1, ADC1_CHAN0, ADC_ATTEN, &adc1_cali_chan0_handle);
    calibrated[1] = example_adc_calibration_init(ADC_UNIT_1, ADC1_CHAN1, ADC_ATTEN, &adc1_cali_chan1_handle);
    calibrated[2] = example_adc_calibration_init(ADC_UNIT_1, ADC1_CHAN2, ADC_ATTEN, &adc1_cali_chan2_handle);
}

static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        //ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;

    return calibrated;
}
#else

unsigned char * get_data(void){
    nrf_read(data);
    return data;
}

#endif
