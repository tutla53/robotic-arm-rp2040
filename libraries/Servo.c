#include "pico/stdlib.h"
#include "hardware/pwm.h"

#include "Servo.h"
#include "PWMmgr.h"

void pwm_set_dutyH(uint slice_num, uint chan, float d){
    /*For Servo*/
    pwm_set_chan_level(slice_num, chan, pwm_get_wrap(slice_num) * d / 10000);
}

void ServoInit(Servo_t *s, uint gpio, float min_period, float max_period, bool invert){  	
    gpio_set_function(gpio, GPIO_FUNC_PWM);
    s->gpio = gpio;
    s->slice = pwm_gpio_to_slice_num(gpio);
    s->chan = pwm_gpio_to_channel(gpio);
    pwm_set_enabled(s->slice, false);
    s->on = false;
    s->speed = 0;
    s->resolution = pwm_set_freq_duty(s->slice, s->chan, SERVO_FREQ, 0);

    float servo_min_duty = (min_period*10*SERVO_FREQ);
    float servo_max_duty = (max_period*10*SERVO_FREQ);
    pwm_set_dutyH(s->slice, s->chan, servo_min_duty);
    s->min_duty = servo_min_duty;
    s->max_duty = servo_max_duty;
    
    if (s->chan) {
        pwm_set_output_polarity(s->slice, false, invert);
    }
    else {
        pwm_set_output_polarity(s->slice, invert, false);
    }
    s->invert = invert;
}

void ServoOn(Servo_t *s){
    pwm_set_enabled(s->slice, true);
    s->on = true;
}

void ServoOff(Servo_t *s){
    pwm_set_enabled(s->slice, false);
    s->on = false;
} 

void ServoPosition(Servo_t *s, float p){
    uint16_t M = (s->max_duty - s->min_duty)/100;
    pwm_set_dutyH(s->slice, s->chan, p*M+s->min_duty);
}