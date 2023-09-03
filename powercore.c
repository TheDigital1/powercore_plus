#include "pico/stdlib.h"
#include <stdlib.h>
#include <stdio.h>
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "pico/time.h"
#include "tusb.h"
#include <math.h>

// ### System Defines ###
// Maximum allowable temperature of power resistor board)
#define maximum_allowable_temperature_of_power_resistor 80.0 // Celsius
// Maximum allowable temperature of power resistor board
#define maximum_allowable_temperature_of_power_MOSFET 80.0 // Celsius
// Minimum allowable temperature used to ensure temp probe connection or sensor isnt faulty
#define minimum_allowable_temperature 0 // Celsius
// The beta coefficient of the NTC 3950 thermister, which is a specification of the thermister used to calculate temperature
#define NTC_3950_thermister_beta_coefficient 3950
// Nominal resistance of the NTC 3950 thermister
#define NTC_3950_thermister_normal_resistance 100000 // ohms
// The pullup resistor for the thermisters on the Powercore motherboard
#define thermister_pullup_resistor_value 10000 // ohms
#define measured_3v3 3.255 // For better ADC accuracy measure your 3v3 rail and enter the value here
#define NTC_Thermister_Number_Of_Samples 2

// ### Pins Definition ###
#define HIGH_VOLTAGE_MOSFET_PIN 17
#define USER_LED_PIN 25
#define P12 12
#define SHORT_ALERT_MOSFET_PIN 20
#define DATA_LENGTH 256

// ### DMA configuration ###
uint DMA_CHANNEL = 0;

// ### Globals Vars ###
uint pulse_counter = 0;
uint spark_counter = 0;
uint short_counter = 0;
uint spark_percent = 0;
uint short_percent = 0;

double mosfet_temperature = 0;
double power_resistor_temperature = 0;
double avg_power = 0;
double avg_charge = 0;
uint current_pwm_frequency = 0;
uint target_pwm_frequency = 2000;

// Globals for ADC and DMA
#define ADC_SAMPLE_NUM 10 // each adc sample is 2us so 10 samples will be 20us
uint8_t adc_samples[ADC_SAMPLE_NUM];
volatile bool adc_samples_ready = false; // This flag tells if ADC samples are ready
volatile double micro_coulomb_per_pulse = 0;
volatile uint adc_samples_sum = 0;
volatile double voltage = 0;
volatile bool pwm_wrap_int = false; // This flag tells if PWM has wraped
volatile double max_micro_coulomb_per_pulse = 2500;
uint slice_num = 0;
uint chan = 0;
uint pwm_compare_level = 0;
bool LowPowerMode = false;

//This function can be used to set pwm using a fixed on time
uint32_t pwm_set_freq_fixed_on_time(uint slice_num, uint chan, uint32_t f)
{
    const double desired_on_time = 0.00002; // 20 us
    uint32_t clock = 125000000;
    uint32_t divider16 = clock / f / 4096 +
                         (clock % (f * 4096) != 0);
    if (divider16 / 16 == 0)
        divider16 = 16;
    uint32_t wrap = clock * 16 / divider16 / f - 1;
    pwm_set_clkdiv_int_frac(slice_num, divider16 / 16,
                            divider16 & 0xF);
    pwm_set_wrap(slice_num, wrap);

    // Compute the duty cycle based on the desired on-time duration
    double T = 1.0 / f;
    double duty_cycle = desired_on_time / T;
    uint32_t on_time_count = (uint32_t)(wrap * duty_cycle);

    pwm_set_chan_level(slice_num, chan, on_time_count);
    pwm_compare_level = on_time_count;
    current_pwm_frequency = f;

    return wrap;
}

//This function can be used to set pwm frequency and duty
uint32_t pwm_set_freq_duty(uint slice_num, uint chan, uint32_t f, int d)
{
    uint32_t clock = 125000000;
    uint32_t divider16 = clock / f / 4096 +
                         (clock % (f * 4096) != 0);
    if (divider16 / 16 == 0)
        divider16 = 16;
    uint32_t wrap = clock * 16 / divider16 / f - 1;
    pwm_set_clkdiv_int_frac(slice_num, divider16 / 16,
                            divider16 & 0xF);
    pwm_set_wrap(slice_num, wrap);
    pwm_set_chan_level(slice_num, chan, wrap * d / 100);
    pwm_compare_level = (wrap * d / 100);
    current_pwm_frequency = f;
    return wrap;
}

//This Interrupt service routine function gets called when the PWM wraps around. i.e. starts its cycle over
void pwm_isr()
{
    pwm_clear_irq(slice_num); // Clear the interrupt
    pwm_wrap_int = true;      // Toggle the PWM wrap_int flag
    gpio_put(P12, 1);         // Used to check timing of how long this ISR took
    adc_run(false);
    adc_select_input(0);
    adc_fifo_drain();
    adc_run(true);
    dma_channel_transfer_to_buffer_now(DMA_CHANNEL, adc_samples, ADC_SAMPLE_NUM);
    dma_channel_wait_for_finish_blocking(DMA_CHANNEL);

    adc_run(false);
    // integrate the current
    adc_samples_sum = 0;
    for (int i = 0; i < ADC_SAMPLE_NUM; i++)
    {
        adc_samples_sum = adc_samples_sum + adc_samples[i];
    }
    micro_coulomb_per_pulse = ((double)adc_samples_sum) * (3.3 / 256.0); // samples to volts (DMA transferred top 8 Bits)
    micro_coulomb_per_pulse = micro_coulomb_per_pulse / 50.0;            // divide out gain of INA
    micro_coulomb_per_pulse = micro_coulomb_per_pulse / 0.001;           // I=v/r r = 1mohm
    micro_coulomb_per_pulse = micro_coulomb_per_pulse * 20.0;            // C = A* S; 20us of total sample time (.00002 * 1,000,000) as we display in micro's
    if (micro_coulomb_per_pulse > max_micro_coulomb_per_pulse)
    {
        if (LowPowerMode == false)
        {
           // pwm_set_freq_duty(slice_num, chan, 500, 1);
            pwm_set_freq_fixed_on_time(slice_num, chan, 500);
            LowPowerMode = true;
        }
    }
    else
    {
        if (LowPowerMode)
        {
            //pwm_set_freq_duty(slice_num, chan, 1500, 3);
            pwm_set_freq_fixed_on_time(slice_num, chan, target_pwm_frequency);
            LowPowerMode = false;
        }
    }
    gpio_put(P12, 0); // Used to check timing of how long this ISR took
}

//This function can be used to get temperature from NTC temperature sensor. 
double get_temperature(u_int8_t thermister_pin)
{
    adc_select_input(thermister_pin);
    double voltage_sum = 0.0;

    for (int i = 0; i < NTC_Thermister_Number_Of_Samples; i++)
    {
        double voltage_sample = adc_read() * (3.3 / 4096.0);
        voltage_sum += voltage_sample;
    }

    double average_voltage = voltage_sum / NTC_Thermister_Number_Of_Samples;
    double observed_thermister_resistance = (average_voltage * thermister_pullup_resistor_value) / (measured_3v3 - average_voltage);
    double steinhart = observed_thermister_resistance / NTC_3950_thermister_normal_resistance;
    steinhart = log(steinhart);
    steinhart /= NTC_3950_thermister_beta_coefficient;
    steinhart += 1.0 / (25.0 + 273.15);
    steinhart = 1.0 / steinhart;
    steinhart -= 273.15; // convert absolute temp to C
    double calc_temperature = steinhart;

    return calc_temperature;
}

//This function can be used to send a message to the GUI
void send_message(char *message)
{
    char data[DATA_LENGTH];
    snprintf(data, DATA_LENGTH, "message=%s\n", message);
    tud_cdc_write(data, strlen(data));
    tud_cdc_write_flush();
}

//This function handles shutting down the PSU in a safe way
void safety_shutdown()
{
    pwm_set_enabled(slice_num, false);
    send_message("Temperature Safety Shutdown!!");
}

//This function checks that the temperature sensors are not outside acceptable range. If they are call the safety_shutdown function
void thermal_runaway_protection_check()
{
    if (power_resistor_temperature > maximum_allowable_temperature_of_power_resistor)
    {
        safety_shutdown();
    }
    if (mosfet_temperature > maximum_allowable_temperature_of_power_MOSFET)
    {
        safety_shutdown();
    }
    if ((mosfet_temperature < minimum_allowable_temperature) || (power_resistor_temperature < minimum_allowable_temperature))
    {
        safety_shutdown();
    }
}

//This function configures the DMA for collecting ADC samples
void setup_dma()
{
    // Get a free DMA channel
    DMA_CHANNEL = dma_claim_unused_channel(true);

    dma_channel_config c = dma_channel_get_default_config(DMA_CHANNEL);

    // Set DMA to transfer 16-bit data from ADC to adc_samples array
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);

    // Set DMA to trigger on ADC data request
    channel_config_set_dreq(&c, DREQ_ADC);

    // Set up the DMA channel with the given configuration
    dma_channel_configure(
        DMA_CHANNEL,
        &c,
        adc_samples,    // Destination
        &adc_hw->fifo,  // Source
        ADC_SAMPLE_NUM, // Transfer count
        false           // Don't start immediately
    );
    dma_channel_set_irq0_enabled(DMA_CHANNEL, true);
}

//This function configures the ADC and the setup needed for allowing the DMA to get the data
void setup_adc()
{

    adc_gpio_init(26); // ADC0 is on GPIO 26
    adc_gpio_init(27); // ADC1 is on GPIO 27
    adc_gpio_init(28); // ADC2 is on GPIO 28
    adc_init();
    adc_select_input(0);
    adc_fifo_setup(true, // Write to FIFO
                   true, // Enable DMA data request (DREQ)
                   1,    // DREQ (data request) threshold (1>=)
                   true, // Allow overflow on FIFO full
                   true  // Enable ADC
    );
    adc_set_clkdiv(0);
}

//This function sets up the PWM and configures the interrupt on wrap
void setup_pwm()
{
    slice_num = pwm_gpio_to_slice_num(HIGH_VOLTAGE_MOSFET_PIN);
    chan = pwm_gpio_to_channel(HIGH_VOLTAGE_MOSFET_PIN);
    gpio_set_function(17, GPIO_FUNC_PWM);
    pwm_set_freq_fixed_on_time(slice_num, chan, target_pwm_frequency);
    // Set up the PWM interrupt
    pwm_clear_irq(slice_num);                         // Clear any pending interrupts
    pwm_set_irq_enabled(slice_num, true);             // Enable PWM slice interrupts
    irq_set_exclusive_handler(PWM_IRQ_WRAP, pwm_isr); // Set the ISR for the PWM interrupt
    irq_set_enabled(PWM_IRQ_WRAP, true);              // Enable the global interrupt
    pwm_set_enabled(slice_num, true);
}

//This function initializes GPIO
void init_gpio()
{
    gpio_init(USER_LED_PIN);
    gpio_set_dir(USER_LED_PIN, GPIO_OUT);
    gpio_init(P12);
    gpio_set_dir(P12, GPIO_OUT);
    gpio_init(SHORT_ALERT_MOSFET_PIN);
    gpio_set_dir(SHORT_ALERT_MOSFET_PIN, GPIO_OUT);
    gpio_init(23);
    gpio_set_dir(23, GPIO_OUT);
    gpio_put(23, 1); // Put VR into FWM mode for better voltage stability
}

//This function sends the updates to the GUI
void send_status_update()
{
    char data[DATA_LENGTH];

    // Format the data into the custom delimited string
    snprintf(data, DATA_LENGTH, "spark%%=%d,short%%=%d,avgPower=%.2f,avgCharge=%.2f,pulseFreq=%d,maxCoulomb=%.0f,resistorTemp=%.0f,mosfetTemp=%.0f\n",
             spark_percent, short_percent, avg_power, avg_charge, current_pwm_frequency, max_micro_coulomb_per_pulse, power_resistor_temperature, mosfet_temperature);

    // Send the data over USB serial
    tud_cdc_write(data, strlen(data));
    tud_cdc_write_flush();
}

//This function handles all the things we need to do after a pwm wrap, that we dont want to take up time in the ISR doing.
void post_pwm_wrap_ops()
{
    double filter_old_weight = ((double)current_pwm_frequency - 1.0) / (double)current_pwm_frequency;
    double filter_new_weight = 1 / (double)current_pwm_frequency;
    pulse_counter++;
    filter_old_weight = ((double)current_pwm_frequency - 1) / (double)current_pwm_frequency;
    filter_new_weight = 1 / (double)current_pwm_frequency;
    if (micro_coulomb_per_pulse > 200)
    {
        if (micro_coulomb_per_pulse > max_micro_coulomb_per_pulse)
        {
            short_counter++;
        }
        else
        {
            spark_counter++;
            
        }
    }
    pwm_wrap_int = false;
    avg_charge = ((avg_charge * filter_old_weight) + (micro_coulomb_per_pulse * filter_new_weight));
    avg_power = ((avg_charge / 20) * 72) / 1000;
    voltage = get_temperature(2);
    if (pulse_counter >= current_pwm_frequency)
    {
        spark_percent = (spark_counter * 100 / pulse_counter);
        short_percent = (short_counter * 100 / pulse_counter);
        short_counter = 0;
        spark_counter = 0;
        pulse_counter = 0;
    }
}

//This function handles when serial data is received from the GUI
void handle_received_data(const char *data_string) {
    char modifiable_string[DATA_LENGTH];
    strncpy(modifiable_string, data_string, DATA_LENGTH - 1);
    modifiable_string[DATA_LENGTH - 1] = '\0';  // Ensure null termination
    // Parse the custom delimited format
    char *token = strtok(modifiable_string, ",");
    while (token != NULL) {
        if (strncmp(token, "pwm_frequency=", 14) == 0) {
            int received_freq = atoi(token + 14);
            target_pwm_frequency = received_freq;
            pwm_set_freq_fixed_on_time(slice_num, chan, target_pwm_frequency);
            char tmp_msg[128];  // Buffer to store the formatted message
            sprintf(tmp_msg, "PWM frequency set to: %d\n", target_pwm_frequency);
            send_message(tmp_msg);
        } else if (strncmp(token, "micro_c_per_pulse=", 18) == 0) {
            int received_coulomb = atoi(token + 18);
            max_micro_coulomb_per_pulse = received_coulomb;
            char tmp_msg[128];  // Buffer to store the formatted message
            sprintf(tmp_msg, "Max uC per Pulse set to: %.0f\n", max_micro_coulomb_per_pulse);
            send_message(tmp_msg);
        }
        token = strtok(NULL, ",");
    }
}
int64_t previousMillisReport = 0;
int64_t currentMillis = 0;


int main()
{

    stdio_init_all();
    tusb_init();
    init_gpio();
    setup_adc();
    setup_dma();
    sleep_ms(200);
    setup_pwm();
    gpio_put(USER_LED_PIN, 1);
    gpio_put(SHORT_ALERT_MOSFET_PIN, 0);

    while (1)
    {
        currentMillis = to_us_since_boot(get_absolute_time());
        if (pwm_wrap_int)
        {
            post_pwm_wrap_ops();
        }

        // Check for received data from USB serial
        if (tud_cdc_available()) {
            char data_string[DATA_LENGTH];
            int num_bytes = tud_cdc_read(data_string, sizeof(data_string) - 1);
            data_string[num_bytes] = '\0';  // Null-terminate the string
            handle_received_data(data_string);
        }
        //This if statement is ran every 200ms i.e 5hz
        if (currentMillis - previousMillisReport >= 200000)
        {
            // ** Check for a short **
            if (short_percent > 25)
            {
                gpio_put(SHORT_ALERT_MOSFET_PIN, 1);
            }
            else
            {
                gpio_put(SHORT_ALERT_MOSFET_PIN, 0);
            }

            irq_set_enabled(PWM_IRQ_WRAP, false); // Disable interrupt so that the adc does not get interrupted. //FIXME this is not great as we will miss a couple of PWM pulses
            power_resistor_temperature = get_temperature(1);
            mosfet_temperature = get_temperature(2);
            irq_set_enabled(PWM_IRQ_WRAP, true);

            thermal_runaway_protection_check();
            send_status_update(); // ** send status via USB serial
            previousMillisReport = to_us_since_boot(get_absolute_time());
        }
    }
}
