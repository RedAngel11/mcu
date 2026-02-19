#include "pico/stdlib.h" 
#include "hardware/gpio.h"

const uint LED_PIN = PICO_DEFAULT_LED_PIN;

int main()
{
    stdio_init_all(); 
    
    while (1)
    {
        gpio_init(LED_PIN);
        gpio_set_dir(LED_PIN, GPIO_OUT);
        char symbol = getchar(); 
        
        printf("received char: %c [ ASCII code: %d ]\n", symbol, symbol);
        switch(symbol)
        {
        case 'e':
            gpio_put(LED_PIN, true);
            printf("led enable done\n");
            break;

        default:
            break;
        }
    }
}