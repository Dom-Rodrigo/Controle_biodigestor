#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "pico/bootrom.h"

#define PWM_AGITADOR 20
#define BUTTON_A 5
#define BUTTON_B 6
#define BUTTON_BOOTSEL 22

#define LED_GREEN 11
#define LED_BLUE 12
#define LED_RED 13

uint pwm__agitador_setup(){
    gpio_set_function(PWM_AGITADOR, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(PWM_AGITADOR);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 38.4);
    pwm_init(slice, &config, true);

    return slice;
}

uint pwm_led_bombas_setup(int gpio){
    /* 60Hz é o max de algumas bombas*/
    gpio_set_function(gpio, GPIO_FUNC_PWM);

    uint slice = pwm_gpio_to_slice_num(gpio);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 32); // (125*10⁶)/(32*65536) = aprox 60Hz
    pwm_init(slice, &config, true);

    return slice;

}

volatile uint32_t last_time;
volatile bool agitador_on;
void gpio_irq_handler(uint gpio, uint32_t event_mask) {
    uint32_t current_time = to_us_since_boot(get_absolute_time());
    if (current_time - last_time > 500000){
        if (!gpio_get(BUTTON_BOOTSEL)) {
            last_time = current_time;
            rom_reset_usb_boot(0, 0);
        }
        if (!gpio_get(BUTTON_A)) {
            last_time = current_time;
            agitador_on=!agitador_on;
            printf("%d\n", agitador_on);
        }
    }   
}

void bomba_de_entrada(int* tanque, float duty_cycle){
    /* Bomba de lobulos */
    int rpm_entrada = 1300 * duty_cycle; // max
    int vazao = (rpm_entrada * 16)/1300; //max 16 m3 por minuto, nesse prototipo sera m3 por segundo
    (*tanque)+=vazao;
    printf("RPM ENTRADA: %d\n", rpm_entrada);
    printf("VAZAO ENTRADA: %d m³/min\n", vazao);
    pwm_set_gpio_level(LED_GREEN, 0xffff*duty_cycle);
}

void bomba_de_saida(int* tanque, float duty_cycle){
    /* Bomba de lobulos */
    int rpm_saida = 1300 * duty_cycle; // max
    int vazao = (rpm_saida * 16)/1300; //max 16 m3 por minuto, nesse prototipo sera m3 por segundo
    (*tanque)-=vazao;
    printf("RPM SAÍDA: %d\n", rpm_saida);
    printf("VAZAO SAÍDA: %d m³/min\n", vazao);
    pwm_set_gpio_level(LED_BLUE, 0xffff*duty_cycle);

}
int main()
{
    stdio_init_all();
    uint slice_agitador = pwm__agitador_setup();
    uint slice_bomba_entrada = pwm_led_bombas_setup(LED_GREEN);
    uint slice_bomba_saida = pwm_led_bombas_setup(LED_BLUE);

    
    gpio_init(LED_RED);
    gpio_set_dir(LED_RED, GPIO_OUT);

    gpio_init(LED_GREEN);
    gpio_set_dir(LED_GREEN, GPIO_OUT);

    gpio_init(LED_BLUE);
    gpio_set_dir(LED_BLUE, GPIO_OUT);

    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);

    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);

    gpio_init(BUTTON_BOOTSEL);
    gpio_set_dir(BUTTON_BOOTSEL, GPIO_IN);
    gpio_pull_up(BUTTON_BOOTSEL);


    gpio_set_irq_enabled_with_callback(BUTTON_BOOTSEL, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    bool going_up = true;
    float initial_dutycicle = 0.025;
    int tanque = 0;
    while (true) {
        pwm_set_gpio_level(PWM_AGITADOR, 0xffff*initial_dutycicle);
        if (agitador_on){
            gpio_put(LED_RED, 1);
            sleep_ms(10);
            if (going_up){
                initial_dutycicle= initial_dutycicle - 0.00025;
                if (initial_dutycicle <= 0.025)
                    going_up=false;
            }
            else
            {
                initial_dutycicle= initial_dutycicle + 0.00025;
                if (initial_dutycicle >= 0.12)
                    going_up=true;
            } 
        }
        else {
            gpio_put(LED_RED, 0);
        }
        bomba_de_entrada(&tanque, 1);
        bomba_de_saida(&tanque, 0);
        printf("TANQUE: %dm³\n", tanque);
        // sleep_ms(1000);
    }
}
