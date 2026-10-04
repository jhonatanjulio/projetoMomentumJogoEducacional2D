#include <stdio.h>
#include <stdbool.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_primitives.h>

typedef struct Fases {
    int dificuldade; // de 1 a 3 (facil, medio, dificil);
    float cargaX;
    float cargaY;
} Fases;

typedef struct Pesos {
    ALLEGRO_BITMAP* sprite;
    float origemX;
    float origemY;
    float origemW;
    float origemH;
    float destinoX;
    float destinoY;
    float destinoW;
    float destinoH;
    float inventarioX; // Guarda a posição fixa X no chão
    float inventarioY; // Guarda a posição fixa Y no chão
    int carga;
    int slotAtual;     // -1 = está no inventário, senão é o índice do slot da contralança
} Pesos;

typedef struct Coordenadas { // struct padrão coordenadas
    int pesoOcupante;
    float comecoX;
    float comecoY;
    float fimX;
    float fimY;
    bool ocupado;
} Coordenadas;

float anguloGrua = 0;            // angulo da grua em graus
float anguloAlvo = 0;            // angulo para onde a grua tomba (esquerda negativo, direita positivo)
float velocidadeAngularGrua = 0; // graus por frame
float aceleracaoGrua = 0.03f;    // aceleracao em graus por frame ao quadrado
int equilibrio = 0;

int calcular_torque(Coordenadas slots[], int totalSlots, int pesoCarga, int distanciaCarga) {
    double torqueContraLanca = 0.0;
    double torqueLanca = 0.0;

    // Percorre os slots da contralança e soma o torque dos blocos encaixados
    for (int i = 0; i < totalSlots; i++) {
        if (slots[i].ocupado) {
            int distancia = i + 1; // Posição física de 1 a N a partir do mastro
            torqueContraLanca += (double)slots[i].pesoOcupante * distancia;
        }
    }

    // Calcula o torque fixo da carga na lança
    torqueLanca = (double)pesoCarga * distanciaCarga;

    printf("\n--- CALCULO DE TORQUE ---\n");
    printf("Torque Contralanca (Esquerda): %.2f\n", torqueContraLanca);
    printf("Torque Lanca (Direita)      : %.2f\n", torqueLanca);
    printf("Diferenca (Sigma M)         : %.2f\n", torqueContraLanca - torqueLanca);

    // Avalia o veredito mecânico (RN05)
    if (torqueContraLanca == torqueLanca) {
        printf("Resultado: EQUILIBRIO PERFEITO!\n");
        return 0;
    }
    else if (torqueContraLanca > torqueLanca) {
        printf("Resultado: TOMBA PARA A ESQUERDA!\n");
        return 1;
    }
    else {
        printf("Resultado: TOMBA PARA A DIREITA!\n");
        return -1;
    }
}

int main() {
    // Inicializa o núcleo do Allegro
    if (!al_init()) {
        printf("Falha ao inicializar o Allegro.\n");
        return -1;
    }

    al_init_primitives_addon();

    if (!al_init_image_addon()) {
        printf("Falha ao inicializar as imagens.");
        return -1;
    }

    // Instala o driver do mouse
    if (!al_install_mouse()) {
        printf("Falha ao inicializar o mouse.\n");
        return -1;
    }

    if (!al_install_keyboard()) { // inicializa o teclado
        printf("Falha ao inicializar o teclado.\n");
        return -1;
    }

    // Cria o temporizador para 60 FPS (1.0 / 60.0)
    ALLEGRO_TIMER* timer = al_create_timer(1.0 / 60.0);
    if (!timer) {
        printf("Falha ao criar o temporizador.\n");
        return -1;
    }

    bool estaTelaCheia = false;
    al_set_new_display_flags(ALLEGRO_FULLSCREEN_WINDOW); // seta flag de fullscreen mode


    // Cria a janela do jogo
    ALLEGRO_DISPLAY* display = al_create_display(1920, 1080);
    if (!display) {
        printf("Falha ao criar a janela.\n");
        al_destroy_timer(timer);
        return -1;
    }

    Coordenadas slotsContraLanca[10];

    slotsContraLanca[0].comecoX = 555;
    slotsContraLanca[0].fimX = 580;
    // +5x de espaçamento entre os pesos
    slotsContraLanca[1].comecoX = 585;
    slotsContraLanca[1].fimX = 610;
    // +5x de espaçamento entre os pesos
    slotsContraLanca[2].comecoX = 615;
    slotsContraLanca[2].fimX = 640;
    // +5x de espaçamento entre os pesos
    slotsContraLanca[3].comecoX = 645;
    slotsContraLanca[3].fimX = 670;
    // +5x de espaçamento entre os pesos
    slotsContraLanca[4].comecoX = 675;
    slotsContraLanca[4].fimX = 700;
    // +5x de espaçamento entre os pesos
    slotsContraLanca[5].comecoX = 705;
    slotsContraLanca[5].fimX = 730;
    // +5x de espaçamento entre os pesos
    slotsContraLanca[6].comecoX = 735;
    slotsContraLanca[6].fimX = 760;
    // +5x de espaçamento entre os pesos
    slotsContraLanca[7].comecoX = 765;
    slotsContraLanca[7].fimX = 790;
    // +5x de espaçamento entre os pesos
    slotsContraLanca[8].comecoX = 795;
    slotsContraLanca[8].fimX = 820;
    // +5x de espaçamento entre os pesos
    slotsContraLanca[9].comecoX = 825;
    slotsContraLanca[9].fimX = 850;

    for (int i = 0; i < 10; i++) {
        slotsContraLanca[i].comecoY = 352.5;
        slotsContraLanca[i].fimY = 398.5;
        slotsContraLanca[i].ocupado = false;
        slotsContraLanca[i].pesoOcupante = 0;
    }

    Coordenadas slotsLanca[10];

    slotsLanca[0].comecoX = 782;

    slotsLanca[1].comecoX = 812;

    slotsLanca[2].comecoX = 842;

    slotsLanca[3].comecoX = 872;

    slotsLanca[4].comecoX = 902;

    slotsLanca[5].comecoX = 932;

    slotsLanca[6].comecoX = 962;

    slotsLanca[7].comecoX = 992;

    slotsLanca[8].comecoX = 1022;

    slotsLanca[9].comecoX = 1052;

    for (int i = 0; i < 10; i++) {
        slotsLanca[i].comecoY = 371;
    }

    Coordenadas slotsCarga[10];

    slotsCarga[0].comecoX = 1042;

    slotsCarga[1].comecoX = 1072;

    slotsCarga[2].comecoX = 1102;

    slotsCarga[3].comecoX = 1132;

    slotsCarga[4].comecoX = 1162;

    slotsCarga[5].comecoX = 1192;

    slotsCarga[6].comecoX = 1222;

    slotsCarga[7].comecoX = 1252;

    slotsCarga[8].comecoX = 1282;

    slotsCarga[9].comecoX = 1312;

    for (int i = 0; i < 10; i++) {
        slotsCarga[i].comecoY = 868;
    }

    ALLEGRO_BITMAP* cenario = al_load_bitmap("assets/cenario/cenario_temp.png");
    ALLEGRO_BITMAP* grua = al_load_bitmap("assets/sprites/grua_sem_elevador.png");
    ALLEGRO_BITMAP* elevador = al_load_bitmap("assets/sprites/elevador_grua.png");

    // definição dos pesos (contra lança)
    Pesos peso[5];

    peso[0].sprite = al_load_bitmap("assets/sprites/peso_grua_1.png");
    peso[0].carga = 1;

    peso[1].sprite = al_load_bitmap("assets/sprites/peso_grua_2.png");
    peso[1].carga = 2;

    peso[2].sprite = al_load_bitmap("assets/sprites/peso_grua_3.png");
    peso[2].carga = 3;

    peso[3].sprite = al_load_bitmap("assets/sprites/peso_grua_5.png");
    peso[3].carga = 5;

    peso[4].sprite = al_load_bitmap("assets/sprites/peso_grua_10.png");
    peso[4].carga = 10;

    // Distribui os 5 pesos lado a lado na base da tela (inventário)
    for (int i = 0; i < 5; i++) {
        peso[i].origemX = 0;
        peso[i].origemY = 0;
        peso[i].origemW = 32;
        peso[i].origemH = 32;
        peso[i].destinoW = 80;
        peso[i].destinoH = 80;

        peso[i].inventarioX = 100 + (i * 100); // Espaçados a cada 100 pixels
        peso[i].inventarioY = 900;             // Altura do chão

        peso[i].destinoX = peso[i].inventarioX;
        peso[i].destinoY = peso[i].inventarioY;

        peso[i].slotAtual = -1; // começa no inventário, fora de qualquer slot
    }

    // definição das cargas (lança)
    Pesos carga[4];

    carga[0].sprite = al_load_bitmap("assets/sprites/carga5.png");
    carga[0].carga = 5;

    carga[1].sprite = al_load_bitmap("assets/sprites/carga10.png");
    carga[1].carga = 10;

    carga[2].sprite = al_load_bitmap("assets/sprites/carga20.png");
    carga[2].carga = 20;

    carga[3].sprite = al_load_bitmap("assets/sprites/carga30.png");
    carga[3].carga = 30;

    for (int i = 0; i < 4; i++) { // valores fixos
        carga[i].origemX = 0;
        carga[i].origemY = 0;
        carga[i].origemW = 32;
        carga[i].origemH = 32;
        carga[i].destinoW = 80;
        carga[i].destinoH = 80;
    }

    //Posição da Carga
    carga[3].destinoX = 1042;
    carga[3].destinoY = 868;

    // Cria a fila que organiza os eventos (timer, mouse, janela)
    ALLEGRO_EVENT_QUEUE* fila_eventos = al_create_event_queue();
    if (!fila_eventos) {
        printf("Falha ao criar a fila de eventos.\n");
        al_destroy_display(display);
        al_destroy_timer(timer);
        return -1;
    }

    // Registra as fontes de eventos na fila
    al_register_event_source(fila_eventos, al_get_display_event_source(display));
    al_register_event_source(fila_eventos, al_get_timer_event_source(timer));
    al_register_event_source(fila_eventos, al_get_keyboard_event_source());
    al_register_event_source(fila_eventos, al_get_mouse_event_source());

    // Configuração antes do loop principal
    bool rodando = true;
    bool redesenhar = true;
    int indice_arrastado = -1; // -1 indica que nenhum bloco está sendo segurado

    al_start_timer(timer);

    // Loop principal do jogo (Game Loop)
    while (rodando) {
        ALLEGRO_EVENT evento;
        // Fica aguardando até um evento chegar na fila
        al_wait_for_event(fila_eventos, &evento);

        // Verifica qual foi o evento
        if (evento.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
            // Clicou no X da janela
            rodando = false;
        }

        if (evento.type == ALLEGRO_EVENT_KEY_DOWN) // alternar minimizar/maximizar tela inteira
        {
            if (estaTelaCheia && evento.keyboard.keycode == ALLEGRO_KEY_F11) { // minimiza
                al_toggle_display_flag(display, ALLEGRO_FULLSCREEN_WINDOW, false);
                al_toggle_display_flag(display, ALLEGRO_WINDOWED, true);
                estaTelaCheia = false;
            }
            else if (estaTelaCheia == false && evento.keyboard.keycode == ALLEGRO_KEY_F11) { // maximiza
                al_toggle_display_flag(display, ALLEGRO_FULLSCREEN_WINDOW, true);
                al_toggle_display_flag(display, ALLEGRO_WINDOWED, false);
                estaTelaCheia = true;
            }

            // Teste da barra de espaço: calcula o torque com os pesos realmente encaixados
            // e define para onde a grua tomba
            if (evento.keyboard.keycode == ALLEGRO_KEY_SPACE) {
                int cargaTeste = 10;
                int distanciaCargaTeste = 3;

                equilibrio = calcular_torque(slotsContraLanca, 10, cargaTeste, distanciaCargaTeste);

                // Define só o alvo; o timer vai aproximando o ângulo atual dele
                if (equilibrio == 1) {
                    anguloAlvo = -90; // tomba para a esquerda
                }
                else if (equilibrio == -1) {
                    anguloAlvo = 90;  // tomba para a direita
                }
                else {
                    anguloAlvo = 0;   // equilíbrio
                }
            }
        }

        else if (evento.type == ALLEGRO_EVENT_TIMER) {
            // O timer disparou (momento de atualizar o frame)

            // Só acelera enquanto a grua ainda não chegou no alvo; ao chegar, zera a velocidade
            if (anguloGrua < anguloAlvo) { // tomba para a direita
                velocidadeAngularGrua += aceleracaoGrua;
                anguloGrua += velocidadeAngularGrua;
                if (anguloGrua > anguloAlvo) {
                    anguloGrua = anguloAlvo;
                    velocidadeAngularGrua = 0;
                }
            }
            else if (anguloGrua > anguloAlvo) { // tomba para a esquerda
                velocidadeAngularGrua += aceleracaoGrua;
                anguloGrua -= velocidadeAngularGrua;
                if (anguloGrua < anguloAlvo) {
                    anguloGrua = anguloAlvo;
                    velocidadeAngularGrua = 0;
                }
            }
            else {
                velocidadeAngularGrua = 0;
            }

            redesenhar = true;
        }
        else if (evento.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN) {
            // Tenta capturar um bloco apenas se as mãos estiverem livres
            if (indice_arrastado == -1) {
                for (int i = 0; i < 5; i++) {
                    // Validação AABB (Colisão Ponto-Retângulo)
                    if (evento.mouse.x >= peso[i].destinoX &&
                        evento.mouse.x <= (peso[i].destinoX + peso[i].destinoW) &&
                        evento.mouse.y >= peso[i].destinoY &&
                        evento.mouse.y <= (peso[i].destinoY + peso[i].destinoH)) {

                        indice_arrastado = i; // Fixa o bloco

                        // Se o peso estava encaixado em um slot, libera o slot ao pegá-lo
                        if (peso[i].slotAtual != -1) {
                            slotsContraLanca[peso[i].slotAtual].ocupado = false;
                            slotsContraLanca[peso[i].slotAtual].pesoOcupante = 0;
                            peso[i].slotAtual = -1;
                        }

                        break;
                    }
                }
            }
        }
        else if (evento.type == ALLEGRO_EVENT_MOUSE_AXES) {
            // Move o bloco rastreado centralizando-o no cursor
            if (indice_arrastado != -1) {
                peso[indice_arrastado].destinoX = evento.mouse.x - (peso[indice_arrastado].destinoW / 2);
                peso[indice_arrastado].destinoY = evento.mouse.y - (peso[indice_arrastado].destinoH / 2);
            }
        }
        else if (evento.type == ALLEGRO_EVENT_MOUSE_BUTTON_UP) {
            // Solta o bloco: encaixa em um slot livre ou volta para o inventário
            if (indice_arrastado != -1) {
                bool foiPosicionado = false;

                for (int i = 0; i < 10; i++) {
                    if (evento.mouse.x >= slotsContraLanca[i].comecoX &&
                        evento.mouse.x <= slotsContraLanca[i].fimX &&
                        evento.mouse.y >= slotsContraLanca[i].comecoY &&
                        evento.mouse.y <= slotsContraLanca[i].fimY &&
                        !slotsContraLanca[i].ocupado)
                    {
                        peso[indice_arrastado].destinoX = slotsContraLanca[i].comecoX - 25; // peso comecoX = slot comecoX - 25
                        peso[indice_arrastado].destinoY = slotsContraLanca[i].comecoY - 10; // peso comecoY = slot comecoY - 10
                        peso[indice_arrastado].slotAtual = i;
                        slotsContraLanca[i].ocupado = true;
                        slotsContraLanca[i].pesoOcupante = peso[indice_arrastado].carga;
                        foiPosicionado = true;
                        break;
                    }
                }

                if (!foiPosicionado) {
                    peso[indice_arrastado].destinoX = peso[indice_arrastado].inventarioX;
                    peso[indice_arrastado].destinoY = peso[indice_arrastado].inventarioY;
                }

                indice_arrastado = -1; // Libera as mãos
            }
        }

        // Etapa de Renderização (Desenho na tela)
        // Só desenha se for a hora certa (redesenhar) e não houver outros eventos travados na fila
        if (redesenhar && al_is_event_queue_empty(fila_eventos)) {
            // Pinta o fundo da tela de preto (R:0, G:0, B:0)
            al_clear_to_color(al_map_rgb(0, 0, 0));

            // Desenha o cenario e o sprite da grua
            al_draw_bitmap(cenario, 0, 0, 0);
            al_draw_scaled_rotated_bitmap(grua, 260, 345, 250 + 650, 50 + 862.5, 2.5, 2.5, anguloGrua * ALLEGRO_PI / 180, 0); // escala de 2.5 (2.5*500 = 1250px)
            al_draw_scaled_bitmap(elevador, 0, 0, 250, 250, slotsLanca[9].comecoX, slotsLanca[9].comecoY, 625, 625, 0); // comecoX = 782, +30x para passar pro proximo slot

            // Desenha todos os blocos de contrapeso
            for (int i = 0; i < 5; i++) {
                al_draw_scaled_bitmap(peso[i].sprite, peso[i].origemX, peso[i].origemY,
                    peso[i].origemW, peso[i].origemH,
                    peso[i].destinoX, peso[i].destinoY,
                    peso[i].destinoW, peso[i].destinoH, 0);

            }

            // Contorno dos slots da contralança (temporario, para debug)
            for (int i = 0; i < 10; i++) {
                al_draw_rectangle(slotsContraLanca[i].comecoX, slotsContraLanca[i].comecoY, slotsContraLanca[i].fimX, slotsContraLanca[i].fimY, al_map_rgb(0, 0, 0), 0);
            }

            // Joga as alterações para a tela visível
            al_flip_display();

            redesenhar = false;
        }

    }

    // Encerramento: destroi as estruturas para não vazar memória
    al_destroy_event_queue(fila_eventos);
    al_destroy_display(display);
    al_destroy_timer(timer);

    return 0;
}