#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/pwm.h"
#include "pico/sync.h"

#include "blink.pio.h"
#include "quadrature_encoder.pio.h"

enum color{
    red = 0,
    green = 1,
    blue = 2
};

#define NUM_CHANNELS 3
#define LEVEL_MAX 255

// Base pin to connect the A phase of the encoder.
// The B phase must be connected to the next pin
#define encoder_pin_AB 20

#define button_pin 19

    // we don't really need to keep the offset, as this program must be loaded
    // at offset 0
#define quadrature_pio_offset 0 

typedef unsigned int pin; 

struct pin_data{
    pin pin; 
    uint slice;
    uint pwm_chan; 
};

typedef struct status_led {
    pin pin;
    bool state; 
 } status_led_t; 

 typedef struct output_led {
    struct pin_data pin[NUM_CHANNELS]; 
    int encoder_base[NUM_CHANNELS];
    int level[NUM_CHANNELS];
 } output_led_t;

status_led_t status_leds[NUM_CHANNELS] = {{4, 0}, {3, 0}, {2, 0}}; 

output_led_t out = {{{6, 0, 0}, {7, 0, 0}, {8, 0, 0}}, {0, 0, 0}, {0, 0, 0}};

int current_led; 


void blink_pin_forever(PIO pio, uint sm, uint offset, uint pin, uint freq) {
    blink_program_init(pio, sm, offset, pin);
    pio_sm_set_enabled(pio, sm, true);

    printf("Blinking pin %d at %d Hz\n", pin, freq);

    // PIO counter program takes 3 more cycles in total than we pass as
    // input (wait for n + 1; mov; jmp)
    pio->txf[sm] = (125000000 / (2 * freq)) - 3;
}

int update_pwm(){
    for (int chan = 0; chan < NUM_CHANNELS; chan ++){

    pwm_set_chan_level(out.pin[chan].slice, out.pin[chan].pwm_chan, out.level[chan]);

    sleep_ms(10);
    }
}

int update_status_leds(){
    for (int led = 0; led < NUM_CHANNELS; led ++){
        if (current_led == led){
            gpio_put(status_leds[led].pin, true);
        } else {
            gpio_put(status_leds[led].pin, false);
        }
    }
}

int init_gpio(){
    for (int led = 0; led < NUM_CHANNELS; led ++){
    gpio_init(status_leds[led].pin);
    gpio_set_dir(status_leds[led].pin, true);
    }

    for (int chan = 0; chan < NUM_CHANNELS; chan ++){
        gpio_set_function(out.pin[chan].pin, GPIO_FUNC_PWM);


        out.pin[chan].slice = pwm_gpio_to_slice_num(out.pin[chan].pin);
        out.pin[chan].pwm_chan = pwm_gpio_to_channel(out.pin[chan].pin);

        pwm_set_wrap(out.pin[chan].slice, 256);

        pwm_set_enabled(out.pin[chan].slice, true);
    }

    update_pwm();


}


int main()
{
    stdio_init_all();

    //Set up PIO programs 
    PIO blink_pio = pio0;
    PIO encoder_pio = pio1;

    uint blink_offset = pio_add_program(blink_pio, &blink_program);
    blink_pin_forever(blink_pio, 0, blink_offset, PICO_DEFAULT_LED_PIN, 3);

    pio_add_program(encoder_pio, &quadrature_encoder_program);
    quadrature_encoder_program_init(encoder_pio, quadrature_pio_offset, 
                                    encoder_pin_AB, 0);

    //Initialize GPIO 
    init_gpio();

    current_led = red; 

    update_pwm();
    update_status_leds();

    int encoder_value;
        
    while (1) {
        //Check Button 
        if (gpio_get(button_pin) == 0){
            printf("Button Pushed!\n");

            current_led += 1; 
            current_led %= NUM_CHANNELS;

            out.encoder_base[current_led] = encoder_value - out.level[current_led]; 

            int status = gpio_get(button_pin);
            while (status == 0){
                status = gpio_get(button_pin);
            }
        }    

        encoder_value = quadrature_encoder_get_count(encoder_pio,  
                                                 quadrature_pio_offset);

        out.level[current_led] = encoder_value - out.encoder_base[current_led];

        //Restrain output level between 0 and 255 
        if (out.level[current_led] < 0){
            out.level[current_led] = 0; 
            out.encoder_base[current_led] = encoder_value; 
            printf("Below\n");
        } else if (out.level[current_led] > LEVEL_MAX){
            out.level[current_led] = LEVEL_MAX; 
            out.encoder_base[current_led] = encoder_value - LEVEL_MAX; 
            printf("Above\n");
        }

        printf("%d \t|R: %d, %d \t|G: %d, %d \t|B: %d, %d\n", encoder_value, 
                out.encoder_base[red], out.level[red], out.encoder_base[green], 
                    out.level[green], out.encoder_base[blue], out.level[blue]);

        update_pwm();
        update_status_leds();

        sleep_ms(100);
        }

    }


