#include <stdio.h>
#include <stdbool.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_primitives.h>

typedef struct Fases {
    int dificuldade;                      // de 1 a 3 (facil, medio, dificil)
    int valorCarga;                       // peso da carga na lança (precisa existir um sprite com esse valor)
    int distanciaCarga;                   // de 1 a 10 a partir do mastro
    int pesosInventario[5];               // valores dos blocos que o jogador recebe nesta fase
    int qtdPesosInventario;               // quantos valores do vetor acima são usados
    float tempoLimite;                    // em segundos (usado pelo cronômetro)
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
    bool disponivel;   // true = este bloco faz parte do inventário da fase atual
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
float anguloAlvo = 0.0f;            // angulo para onde a grua tomba (esquerda negativo, direita positivo)
float velocidadeAngularGrua = 0.0f; // graus por frame
float aceleracaoGrua = 0.05f;    // aceleracao em graus por frame ao quadrado
int equilibrio = 0;

int estadoFase = 0;              // 0 = montando, 1 = carga subindo (vitória), 2 = fase concluída
int quadroElevador = 0;          // quadro atual do spritesheet do elevador (0 a 14)
int contadorTicksElevador = 0;   // conta os ticks até trocar de quadro
int contadorTicksParado = 0;     // conta os ticks com a carga parada lá em cima

double inicioFase = 0.0;         // momento em que a fase começou (al_get_time, em segundos)
double tempoRestante = 0.0;      // segundos que faltam para o tempo acabar
double tempoGasto = 0.0;         // Δt: do início da fase até o Confirmar (RN01)
bool cronometroAtivo = false;    // true enquanto o tempo está correndo
bool tempoEsgotado = false;      // true quando o tempo chegou a zero

// Dados das fases (projetadas para 10 posições; soluções de referência nos comentários)
// Ordem dos campos: dificuldade, valorCarga, distanciaCarga, {pesos do inventário}, qtd de pesos, tempoLimite
Fases fases[5] = {
    { 1, 10, 1, { 5, 10 },           2,  45.0f }, // M = 10  -> 10 na pos. 1, ou 5 na pos. 2
    { 1, 20, 3, { 5, 10 },           2,  60.0f }, // M = 60  -> 10 na pos. 6
    { 2, 30, 4, { 3, 5, 10 },        3,  75.0f }, // M = 120 -> 10 na pos. 10 + 5 na pos. 4
    { 3, 30, 5, { 3, 5, 10 },        3,  90.0f }, // M = 150 -> 10 na pos. 10 + 5 na pos. 7 + 3 na pos. 5
    { 3, 20, 9, { 1, 2, 3, 5, 10 },  5, 120.0f }  // M = 180 -> 10 na 10 + 5 na 9 + 3 na 8 + 2 na 5 + 1 na 1
};

int faseAtual = 0; // índice da fase em jogo (0 = primeira fase)

// Procura no vetor de cargas qual sprite tem o valor pedido. Retorna o índice, ou -1 se não existir.
int buscar_indice_carga(Pesos cargas[], int totalCargas, int valor) {
    for (int i = 0; i < totalCargas; i++) {
        if (cargas[i].carga == valor) {
            return i;
        }
    }
    return -1;
}

// Diz se um bloco com esse valor faz parte do inventário da fase.
bool peso_esta_na_fase(Fases* fase, int valorPeso) {
    for (int i = 0; i < fase->qtdPesosInventario; i++) {
        if (fase->pesosInventario[i] == valorPeso) {
            return true;
        }
    }
    return false;
}

// Confere se os dados das fases são válidos antes do jogo começar.
// Evita acessar slotsCarga fora do vetor ou desenhar uma carga sem sprite.
bool validar_fases(Pesos cargas[], int totalCargas) {
    bool tudoCerto = true;

    for (int i = 0; i < 5; i++) {
        if (fases[i].distanciaCarga < 1 || fases[i].distanciaCarga > 10) {
            printf("Fase %d: distancia da carga (%d) fora de 1 a 10.\n", i + 1, fases[i].distanciaCarga);
            tudoCerto = false;
        }
        if (buscar_indice_carga(cargas, totalCargas, fases[i].valorCarga) == -1) {
            printf("Fase %d: nao existe sprite para a carga %d.\n", i + 1, fases[i].valorCarga);
            tudoCerto = false;
        }
        if (fases[i].qtdPesosInventario < 1 || fases[i].qtdPesosInventario > 5) {
            printf("Fase %d: quantidade de pesos no inventario invalida (%d).\n", i + 1, fases[i].qtdPesosInventario);
            tudoCerto = false;
        }
    }

    return tudoCerto;
}

int calcular_torque(Coordenadas slots[], int totalSlots, int pesoCarga, int distanciaCarga) {
    double torqueContraLanca = 0.0;
    double torqueLanca = 0.0;

    // Percorre os slots da contralança e soma o torque dos blocos encaixados
    for (int i = 0; i < totalSlots; i++) {
        if (slots[i].ocupado) {
            int distancia = totalSlots - i; // Posição física de 1 a N a partir do mastro
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

// Deixa a fase escolhida como no início: grua em pé, contralança vazia e blocos da fase no chão
void carregar_fase(int indice, Pesos pesos[], int totalPesos, Coordenadas slots[], int totalSlots) {
    faseAtual = indice;

    // Grua volta a ficar em pé e parada
    anguloGrua = 0.0f;
    anguloAlvo = 0.0f;
    velocidadeAngularGrua = 0.0f;
    equilibrio = 0;

    // Volta para a montagem, com a carga lá embaixo
    estadoFase = 0;
    quadroElevador = 0;
    contadorTicksElevador = 0;
    contadorTicksParado = 0;

    // Reinicia o cronômetro com o tempo limite da fase
    inicioFase = al_get_time();
    tempoRestante = fases[faseAtual].tempoLimite;
    tempoGasto = 0.0;
    cronometroAtivo = true;
    tempoEsgotado = false;

    // Esvazia todos os slots da contralança
    for (int i = 0; i < totalSlots; i++) {
        slots[i].ocupado = false;
        slots[i].pesoOcupante = 0;
    }

    // Coloca no chão só os pesos da fase, lado a lado
    int posicaoNoInventario = 0;

    for (int i = 0; i < totalPesos; i++) {
        pesos[i].disponivel = peso_esta_na_fase(&fases[faseAtual], pesos[i].carga);

        pesos[i].inventarioX = 100 + (posicaoNoInventario * 100); // Espaçados a cada 100 pixels
        pesos[i].inventarioY = 900;                               // Altura do chão
        if (pesos[i].disponivel) {
            posicaoNoInventario++;
        }

        pesos[i].destinoX = pesos[i].inventarioX;
        pesos[i].destinoY = pesos[i].inventarioY;
        pesos[i].slotAtual = -1; // fora de qualquer slot
    }

    printf("\n=== FASE %d === Carga %d na distancia %d\n", faseAtual + 1,
        fases[faseAtual].valorCarga, fases[faseAtual].distanciaCarga);
}

int main() {
    // Inicializa o núcleo do Allegro
    if (!al_init()) {
        printf("Falha ao inicializar o Allegro.\n");
        return -1;
    }

    if (!al_init_primitives_addon()) {
        printf("Falha ao inicializar elementos primitivos.");
        return -1;
    }

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
    ALLEGRO_BITMAP* elevador = al_load_bitmap("assets/sprites/elevador_grua_spritesheet.png"); // 15 quadros de 250x250 lado a lado

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

    // Tamanhos fixos dos pesos (a posição no chão é definida em carregar_fase)
    for (int i = 0; i < 5; i++) {
        peso[i].origemX = 0;
        peso[i].origemY = 0;
        peso[i].origemW = 32;
        peso[i].origemH = 32;
        peso[i].destinoW = 80;
        peso[i].destinoH = 80;
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

    // A posição da carga não é mais fixa aqui: ela vem da distância da fase atual (ver desenho)

    // Confere os dados das fases antes de começar o jogo
    if (!validar_fases(carga, 4)) {
        printf("Dados das fases invalidos. Corrija o vetor 'fases' e compile de novo.\n");
        al_destroy_display(display);
        al_destroy_timer(timer);
        return -1;
    }

    // Começa o jogo na primeira fase
    carregar_fase(0, peso, 5, slotsContraLanca, 10);

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
        else if (evento.type == ALLEGRO_EVENT_KEY_DOWN) {
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

            // Teste da tecla N: pula para a próxima fase (depois da última, volta para a primeira)
            if (evento.keyboard.keycode == ALLEGRO_KEY_N) {
                int proximaFase = faseAtual + 1;
                if (proximaFase >= 5) {
                    proximaFase = 0;
                }

                carregar_fase(proximaFase, peso, 5, slotsContraLanca, 10);
                indice_arrastado = -1; // solta qualquer bloco que estivesse na mão
            }

            // Teste da barra de espaço: calcula o torque e define para onde a grua tomba
            // Só confirma durante a montagem, com as mãos livres e com o tempo correndo
            if (evento.keyboard.keycode == ALLEGRO_KEY_SPACE && estadoFase == 0 && indice_arrastado == -1 && cronometroAtivo) {
                // Para o cronômetro e guarda o tempo gasto na fase (RN01)
                cronometroAtivo = false;
                tempoGasto = al_get_time() - inicioFase;
                printf("Tempo gasto na fase: %.2f s\n", tempoGasto);

                // A carga e a distância agora vêm da fase atual
                equilibrio = calcular_torque(slotsContraLanca, 10,
                    fases[faseAtual].valorCarga, fases[faseAtual].distanciaCarga);

                if (equilibrio == 1) {
                    anguloAlvo = -90.0f; // tomba para a esquerda
                }
                else if (equilibrio == -1) {
                    anguloAlvo = 90.0f;  // tomba para a direita
                }
                else {
                    anguloAlvo = 0.0f;   // equilíbrio
                    estadoFase = 1;      // começa a subir a carga
                    quadroElevador = 0;
                    contadorTicksElevador = 0;
                    contadorTicksParado = 0;
                }
            }
        }
        else if (evento.type == ALLEGRO_EVENT_TIMER) {
            // O timer disparou (momento de atualizar o frame)
            if (anguloGrua < anguloAlvo) { // tomba para a direita
                velocidadeAngularGrua += aceleracaoGrua;
                anguloGrua += velocidadeAngularGrua;
                if (anguloGrua > anguloAlvo) {
                    anguloGrua = anguloAlvo;
                    velocidadeAngularGrua = 0.0f;
                }
            }
            else if (anguloGrua > anguloAlvo) { // tomba para a esquerda
                velocidadeAngularGrua += aceleracaoGrua;
                anguloGrua -= velocidadeAngularGrua;
                if (anguloGrua < anguloAlvo) {
                    anguloGrua = anguloAlvo;
                    velocidadeAngularGrua = 0.0f;
                }
            }
            else {
                velocidadeAngularGrua = 0.0f;
            }

            // Cronômetro regressivo: calcula quanto falta a partir do momento em que a fase começou
            if (cronometroAtivo) {
                tempoRestante = fases[faseAtual].tempoLimite - (al_get_time() - inicioFase);

                if (tempoRestante <= 0.0) {
                    tempoRestante = 0.0;
                    cronometroAtivo = false;
                    tempoEsgotado = true;
                    printf("Tempo esgotado na fase %d!\n", faseAtual + 1);
                }
            }

            // Animação de vitória: recolhe o cabo quadro a quadro e depois espera parada lá em cima
            if (estadoFase == 1) {
                if (quadroElevador < 14) {
                    contadorTicksElevador++;
                    if (contadorTicksElevador >= 8) { // troca de quadro a cada 8 ticks (~0,13 s)
                        quadroElevador++;
                        contadorTicksElevador = 0;
                    }
                }
                else {
                    contadorTicksParado++;
                    if (contadorTicksParado >= 60) { // 60 ticks = 1 segundo parada lá em cima
                        estadoFase = 2;
                        printf("Fase %d concluida!\n", faseAtual + 1);
                    }
                }
            }

            // Fase concluída: avança para a próxima (depois da última, volta para a primeira)
            if (estadoFase == 2) {
                int proximaFase = faseAtual + 1;
                if (proximaFase >= 5) {
                    printf("Todas as fases concluidas!\n");
                    proximaFase = 0;
                }

                carregar_fase(proximaFase, peso, 5, slotsContraLanca, 10);
                indice_arrastado = -1;
            }

            redesenhar = true;
        }
        else if (evento.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN) {
            // Tenta capturar um bloco apenas se as mãos estiverem livres e a fase estiver na montagem
            if (indice_arrastado == -1 && estadoFase == 0) {
                for (int i = 0; i < 5; i++) {
                    // Blocos fora do inventário da fase não podem ser pegos
                    if (!peso[i].disponivel) {
                        continue;
                    }

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

            // Desenha o cenario e o sprite da grua rotacionando
            al_draw_bitmap(cenario, 0, 0, 0);
            al_draw_scaled_rotated_bitmap(grua, 260, 345, 250 + 650, 50 + 862.5, 2.5, 2.5, anguloGrua * ALLEGRO_PI / 180, 0);

            // Só desenha o elevador, os contrapesos e os slots SE a grua estiver em pé
            if (anguloGrua == 0) {
                // Na lança, a distância d fica no índice d - 1 (índice 0 = mais perto do mastro)
                int indiceSlotCarga = fases[faseAtual].distanciaCarga - 1;
                int indiceSpriteCarga = buscar_indice_carga(carga, 4, fases[faseAtual].valorCarga);

                // Cada quadro do spritesheet recolhe 5 px de cabo (12,5 px na tela), então a carga sobe junto
                float subidaCarga = quadroElevador * 12.5f;

                al_draw_scaled_bitmap(elevador, quadroElevador * 250, 0, 250, 250, slotsLanca[indiceSlotCarga].comecoX, slotsLanca[indiceSlotCarga].comecoY, 625, 625, 0);
                al_draw_scaled_bitmap(carga[indiceSpriteCarga].sprite, 0, 0, 250, 250, slotsCarga[indiceSlotCarga].comecoX, slotsCarga[indiceSlotCarga].comecoY - subidaCarga, 625, 625, 0);

                // Desenha só os blocos de contrapeso que a fase oferece
                for (int i = 0; i < 5; i++) {
                    if (!peso[i].disponivel) {
                        continue;
                    }

                    al_draw_scaled_bitmap(peso[i].sprite, peso[i].origemX, peso[i].origemY,
                        peso[i].origemW, peso[i].origemH,
                        peso[i].destinoX, peso[i].destinoY,
                        peso[i].destinoW, peso[i].destinoH, 0);

                }

                // Contorno dos slots da contralança (temporario, para debug)
                for (int i = 0; i < 10; i++) {
                    al_draw_rectangle(slotsContraLanca[i].comecoX, slotsContraLanca[i].comecoY, slotsContraLanca[i].fimX, slotsContraLanca[i].fimY, al_map_rgb(0, 0, 0), 0);
                }
            }

            // Barra do cronômetro (temporária, até o HUD): encolhe conforme o tempo acaba
            float proporcaoTempo = tempoRestante / fases[faseAtual].tempoLimite; // 1 = cheio, 0 = acabou
            ALLEGRO_COLOR corBarra = al_map_rgb(60, 180, 75);   // verde
            if (proporcaoTempo < 0.5f) {
                corBarra = al_map_rgb(240, 190, 40);            // amarelo na metade do tempo
            }
            if (proporcaoTempo < 0.25f) {
                corBarra = al_map_rgb(220, 50, 40);             // vermelho no último quarto
            }

            al_draw_filled_rectangle(760, 40, 1160, 70, al_map_rgb(40, 40, 40));                   // fundo
            al_draw_filled_rectangle(760, 40, 760 + 400 * proporcaoTempo, 70, corBarra);          // tempo restante
            al_draw_rectangle(760, 40, 1160, 70, al_map_rgb(0, 0, 0), 2);                          // contorno

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