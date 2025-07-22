#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "pico/bootrom.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "font.h"
#include "matrixws.h"
#include "leds_buttons.h"
#include "display.h"
#include "html.h"
#include "webpage.h"

// ==================== CONFIGURAÇÕES ====================
#define I2C_PORT i2c0
#define I2C_SDA 0
#define I2C_SCL 1
#define WIFI_SSID "RONALDO"
#define WIFI_PASS "36394578"
#define DEBOUNCE_MS 300

// ==================== VARIÁVEIS GLOBAIS ====================
volatile uint32_t ultima_interrupcao = 0;
AHT20_Data leitura_aht20;
int32_t temperatura_bruta_bmp;
int32_t pressao_bruta;
struct bmp280_calib_param calib_bmp;

// Declaração das variáveis do display (com tipos corretos)
extern ssd1306_t ssd;  // Tipo corrigido para ssd1306_t
extern bool cor;       // Tipo corrigido para bool

// ==================== PROTÓTIPOS DE FUNÇÃO ====================
void hardware_init(void);
int wifi_connect(void);
void sensors_init(void);
void sensors_read(float dados[]);
void handle_button_press(uint gpio, uint32_t eventos);
void display_page(float dados[], int pagina);
void check_alerts(float dados[]);

// ==================== IMPLEMENTAÇÃO DAS FUNÇÕES ====================

void hardware_init(void) {
    inicializar_leds();
    iniciar_buzzer();
    controle(PINO_MATRIZ);
    iniciar_botoes();
    init_display();
    
    // Configura I2C
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    
    // Configura interrupções dos botões
    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &handle_button_press);
    gpio_set_irq_enabled_with_callback(BOTAO_B, GPIO_IRQ_EDGE_FALL, true, &handle_button_press);
}

int wifi_connect(void) {
    printf("[Rede] Iniciando conexão com a rede Wi-Fi...\n");
    ssd1306_fill(&ssd, false);
    escrever(&ssd, "Conectando Wi-Fi...", 5, 15, cor);
    ssd1306_send_data(&ssd);

    if (cyw43_arch_init()) {
        printf("[Rede] Falha na inicialização do módulo Wi-Fi\n");
        ssd1306_fill(&ssd, false);
        escrever(&ssd, "Falha no Wi-Fi", 15, 25, cor);
        ssd1306_send_data(&ssd);
        return 1;
    }

    cyw43_arch_enable_sta_mode();

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
        printf("[Rede] Não foi possível estabelecer conexão\n");
        ssd1306_fill(&ssd, false);
        escrever(&ssd, "Conexão falhou", 15, 25, cor);
        ssd1306_send_data(&ssd);
        return 1;
    }

    uint8_t *ip_bytes = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
    char ip_formatado[24];
    snprintf(ip_formatado, sizeof(ip_formatado), "IP: %d.%d.%d.%d", 
             ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);

    printf("[Rede] Conexão estabelecida. IP: %s\n", ip_formatado);
    
    ssd1306_fill(&ssd, false);
    escrever(&ssd, "Wi-Fi Conectado", 10, 15, cor);
    escrever(&ssd, ip_formatado, 5, 35, cor);
    ssd1306_send_data(&ssd);
    
    sleep_ms(2000);
    return 0;
}

void sensors_init(void) {
    bmp280_init(I2C_PORT);
    bmp280_get_calib_params(I2C_PORT, &calib_bmp);
    aht20_reset(I2C_PORT);
    aht20_init(I2C_PORT);
    printf("[Sensores] Dispositivos calibrados\n");
}

void sensors_read(float dados[]) {
    bmp280_read_raw(I2C_PORT, &temperatura_bruta_bmp, &pressao_bruta);
    int32_t temp_c = bmp280_convert_temp(temperatura_bruta_bmp, &calib_bmp);
    int32_t pressao = bmp280_convert_pressure(pressao_bruta, temperatura_bruta_bmp, &calib_bmp);
    double alt_calc = calculate_altitude(pressao);
    int ok_aht = aht20_read(I2C_PORT, &leitura_aht20);

    dados[0] = (pressao / 1000.0f) + offsets[0];
    dados[1] = (temp_c / 100.0f) + offsets[1];
    dados[2] = alt_calc + offsets[2];
    dados[3] = ok_aht ? (leitura_aht20.temperature + offsets[3]) : 
                       historico[3][(historico_idx[3] + HIST_SIZE - 1) % HIST_SIZE];
    dados[4] = ok_aht ? (leitura_aht20.humidity + offsets[4]) : 
                       historico[4][(historico_idx[4] + HIST_SIZE - 1) % HIST_SIZE];

    // Atualiza histórico
    for(int i = 0; i < 5; i++) {
        historico[i][historico_idx[i]] = dados[i];
        historico_idx[i] = (historico_idx[i] + 1) % HIST_SIZE;
    }
}

void handle_button_press(uint gpio, uint32_t eventos) {
    uint32_t agora = to_ms_since_boot(get_absolute_time());
    if (agora - ultima_interrupcao > DEBOUNCE_MS) {
        if (gpio == BOTAO_A) {
            pagina_atual = (pagina_atual + 1) % 5;
            printf("[Sistema] Navegação: Exibindo página %d\n", pagina_atual);
        } else if (gpio == BOTAO_B) {
            reset_usb_boot(0, 0);
            printf("[Sistema] Dispositivo preparado para reinicialização via USB\n");
        }
        ultima_interrupcao = agora;
    }
}

void display_page(float dados[], int pagina) {
    ssd1306_fill(&ssd, false);
    
    // Cabeçalho fixo
    escrever(&ssd, "Sistema Monitor", 5, 0, cor);
    escrever(&ssd, "----------------", 0, 10, cor);
    
    const char* formato;
    const char* unidade;
    const char* titulo = nomes_leituras[pagina];
    
    switch(pagina) {
        case 0: formato = "%.2f"; unidade = "kPa"; break;
        case 1: formato = "%.1f"; unidade = "°C"; break;
        case 2: formato = "%.1f"; unidade = "m"; break;
        case 3: formato = "%.1f"; unidade = "°C"; break;
        case 4: formato = "%.0f"; unidade = "%"; break;
    }
    
    char valor_str[16];
    snprintf(valor_str, sizeof(valor_str), formato, dados[pagina]);
    
    escrever(&ssd, titulo, 15, 20, cor);
    
    char display_line[32];
    snprintf(display_line, sizeof(display_line), "%s %s", valor_str, unidade);
    escrever(&ssd, display_line, 25, 40, cor);
    
    // Verifica alertas
    int dentro_limites = 1;
    for(int i = 0; i < 5; i++) {
        if(dados[i] < limites_min[i] || dados[i] > limites_max[i]) {
            dentro_limites = 0;
            break;
        }
    }
    
    escrever(&ssd, dentro_limites ? "✓ OK" : "! ALERTA", 70, 20, cor);
    ssd1306_send_data(&ssd);
}

void check_alerts(float dados[]) {
    int dentro_limites = 1;
    for(int i = 0; i < 5; i++) {
        if(dados[i] < limites_min[i] || dados[i] > limites_max[i]) {
            dentro_limites = 0;
            break;
        }
    }

    if(dentro_limites) {
        gpio_put(led_red, 0);
        gpio_put(led_blue, 1);
        desliga();
    } else {
        gpio_put(led_red, 1);
        gpio_put(led_blue, 0);
        alerta_sistema();
        tocar_nota(500, 300);
        printf("[Alerta] Valores fora dos limites!\n");
    }
}

// ==================== FUNÇÃO PRINCIPAL ====================
int main() {
    // Inicializações
    hardware_init();
    stdio_init_all();
    sleep_ms(2000);
    printf("[Sistema] Inicializando v2.4\n");

    // Conexão Wi-Fi
    if(wifi_connect()) {
        printf("[Sistema] Modo offline ativado\n");
        ssd1306_fill(&ssd, false);
        escrever(&ssd, "Modo Offline", 20, 30, cor);
        ssd1306_send_data(&ssd);
        sleep_ms(1000);
    }
    
    start_http_server();
    sensors_init();

    // Loop principal
    float dados[5];
    while(true) {
        cyw43_arch_poll();
        sensors_read(dados);
        display_page(dados, pagina_atual);
        check_alerts(dados);
        sleep_ms(500);
    }

    cyw43_arch_deinit();
    return 0;
}