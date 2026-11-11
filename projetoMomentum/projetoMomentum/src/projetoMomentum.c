#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>

// Telas do jogo (Máquina de estados - E6)
typedef enum TelaJogo {
    TELA_MENU = 0,
    TELA_RECORDES = 1,
    TELA_JOGO = 2,
    TELA_VITORIA_FINAL = 3
} TelaJogo;

typedef struct Fases {
    int dificuldade;                      // de 1 a 3 (facil, medio, dificil)
    int valorCarga;                       // peso da carga na lança
    int distanciaCarga;                   // de 1 a 10 a partir do mastro
    int pesosInventario[5];               // valores dos blocos disponíveis nesta fase
    int qtdPesosInventario;               // quantos valores do vetor acima são usados
    float tempoLimite;                    // em segundos
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
    float inventarioX; // Posição fixa X no chão
    float inventarioY; // Posição fixa Y no chão
    int carga;
    int slotAtual;     // -1 = está no inventário, senão é o índice do slot da contralança
    bool disponivel;   // true = este bloco faz parte do inventário da fase atual
} Pesos;

typedef struct Coordenadas {
    int pesoOcupante;
    float comecoX;
    float comecoY;
    float fimX;
    float fimY;
    bool ocupado;
} Coordenadas;

// Variáveis globais de estado e física
TelaJogo telaAtual = TELA_MENU;

float anguloGrua = 0.0f;            // ângulo da grua em graus
float anguloAlvo = 0.0f;            // ângulo para onde a grua tomba (esquerda negativo, direita positivo)
float velocidadeAngularGrua = 0.0f; // graus por frame
float aceleracaoGrua = 0.05f;       // aceleração em graus por frame ao quadrado
int equilibrio = 0;

// 0 = montando, 1 = carga subindo (vitória), 2 = fase concluída, 3 = grua caindo/derrota
int estadoFase = 0;
int quadroElevador = 0;          // quadro atual do spritesheet do elevador (0 a 14)
int contadorTicksElevador = 0;   // conta os ticks até trocar de quadro
int contadorTicksParado = 0;     // conta os ticks com a carga parada lá em cima
int contadorTicksQueda = 0;      // conta os ticks após a grua cair para reiniciar a fase

double inicioFase = 0.0;         // momento em que a fase começou (al_get_time, em segundos)
double tempoRestante = 0.0;      // segundos que faltam para o tempo acabar
double tempoGasto = 0.0;         // Δt: do início da fase até o Confirmar (RN01)
bool cronometroAtivo = false;    // true enquanto o tempo está correndo
bool tempoEsgotado = false;      // true quando o tempo chegou a zero

double pontuacaoFase = 0.0;        // pontos ganhos na fase atual (RF09)
double pontuacaoTotal = 0.0;       // soma das fases concluídas na partida (RF09)
double ultimaPontuacaoFinal = 0.0; // guarda a pontuação ao fechar as 5 fases
double recorde = 0.0;              // maior pontuação total registrada em arquivo (RF11)

// Guarda os últimos valores e a expressão completa para escrever na tela após confirmar (RF05)
double ultimoTorqueEsq = 0.0;
double ultimoTorqueDir = 0.0;
double ultimoSigmaM = 0.0;
char expressaoTorqueEsq[128] = "";

// Dados das 5 fases (projetadas para 13 posições)
Fases fases[5] = {
    { 1, 10, 4,{ 5, 10 },           2,  45.0f }, // M = 40
    { 1, 20, 6,{ 5, 10 },           2,  60.0f }, // M = 120
    { 2, 20, 8,{ 3, 5, 10 },        3,  75.0f }, // M = 160
    { 3, 30, 7,{2, 5, 10 },         3,  90.0f }, // M = 210
    { 3, 30, 8,{ 1, 2, 3, 5, 10},   5, 120.0f }  // M = 240
};

int faseAtual = 0; // índice da fase em jogo (0 = primeira fase)

// Função auxiliar para verificar clique do mouse dentro de um retângulo
bool ponto_no_retangulo(float px, float py, float x1, float y1, float x2, float y2) {
    return (px >= x1 && px <= x2 && py >= y1 && py <= y2);
}

// Desenha texto escalado usando a fonte nativa do Allegro sem precisar de arquivo .ttf externo
void desenhar_texto_escalado(ALLEGRO_FONT* fonte, ALLEGRO_COLOR cor, float x, float y, float escala, int flags, const char* texto) {
    ALLEGRO_TRANSFORM transform;
    al_identity_transform(&transform);
    al_scale_transform(&transform, escala, escala);
    al_use_transform(&transform);

    al_draw_text(fonte, cor, x / escala, y / escala, flags, texto);

    al_identity_transform(&transform);
    al_use_transform(&transform);
}

// Lê o recorde salvo em arquivo de texto (RF11)
double carregar_recorde(const char* caminhoArquivo) {
    FILE* arquivo = fopen(caminhoArquivo, "r");
    if (!arquivo) {
        return 0.0;
    }
    double valorLido = 0.0;
    if (fscanf(arquivo, "%lf", &valorLido) != 1) {
        valorLido = 0.0;
    }
    fclose(arquivo);
    return valorLido;
}

// Salva o novo recorde em arquivo de texto (RF11)
void salvar_recorde(const char* caminhoArquivo, double novoRecorde) {
    FILE* arquivo = fopen(caminhoArquivo, "w");
    if (!arquivo) {
        printf("Aviso: nao foi possivel salvar o recorde em %s\n", caminhoArquivo);
        return;
    }
    fprintf(arquivo, "%.2f\n", novoRecorde);
    fclose(arquivo);
}

int buscar_indice_carga(Pesos cargas[], int totalCargas, int valor) {
    for (int i = 0; i < totalCargas; i++) {
        if (cargas[i].carga == valor) {
            return i;
        }
    }
    return -1;
}

bool peso_esta_na_fase(Fases* fase, int valorPeso) {
    for (int i = 0; i < fase->qtdPesosInventario; i++) {
        if (fase->pesosInventario[i] == valorPeso) {
            return true;
        }
    }
    return false;
}

bool validar_fases(Pesos cargas[], int totalCargas) {
    bool tudoCerto = true;

    for (int i = 0; i < 5; i++) {
        if (fases[i].distanciaCarga < 1 || fases[i].distanciaCarga > 13) {
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

    expressaoTorqueEsq[0] = '\0';
    bool primeiroTermo = true;

    // Percorre os slots da contralança, soma o torque e monta a expressão (ex: 10x6 + 5x4 = 80)
    for (int i = 0; i < totalSlots; i++) {
        if (slots[i].ocupado) {
            int distancia = totalSlots - i; // Posição física de 1 a 13 a partir do mastro
            torqueContraLanca += (double)slots[i].pesoOcupante * distancia;

            char termo[32];
            if (!primeiroTermo) {
                strncat(expressaoTorqueEsq, " + ", sizeof(expressaoTorqueEsq) - strlen(expressaoTorqueEsq) - 1);
            }
            snprintf(termo, sizeof(termo), "%dx%d", slots[i].pesoOcupante, distancia);
            strncat(expressaoTorqueEsq, termo, sizeof(expressaoTorqueEsq) - strlen(expressaoTorqueEsq) - 1);
            primeiroTermo = false;
        }
    }

    if (primeiroTermo) {
        snprintf(expressaoTorqueEsq, sizeof(expressaoTorqueEsq), "0 (nenhum bloco)");
    }
    else {
        char resultadoFinal[32];
        snprintf(resultadoFinal, sizeof(resultadoFinal), " = %.0f", torqueContraLanca);
        strncat(expressaoTorqueEsq, resultadoFinal, sizeof(expressaoTorqueEsq) - strlen(expressaoTorqueEsq) - 1);
    }

    // Calcula o torque fixo da carga na lança
    torqueLanca = (double)pesoCarga * distanciaCarga;

    // Armazena para exibição na tela (HUD pós-confirmação)
    ultimoTorqueEsq = torqueContraLanca;
    ultimoTorqueDir = torqueLanca;
    ultimoSigmaM = torqueContraLanca - torqueLanca;

    printf("\n--- CALCULO DE TORQUE ---\n");
    printf("Torque Contralanca (Esquerda): %s\n", expressaoTorqueEsq);
    printf("Torque Lanca (Direita)      : %dx%d = %.2f\n", pesoCarga, distanciaCarga, torqueLanca);
    printf("Diferenca (Sigma M)         : %.2f\n", ultimoSigmaM);

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

void carregar_fase(int indice, Pesos pesos[], int totalPesos, Coordenadas slots[], int totalSlots) {
    faseAtual = indice;

    anguloGrua = 0.0f;
    anguloAlvo = 0.0f;
    velocidadeAngularGrua = 0.0f;
    equilibrio = 0;

    estadoFase = 0;
    quadroElevador = 0;
    contadorTicksElevador = 0;
    contadorTicksParado = 0;
    contadorTicksQueda = 0;

    inicioFase = al_get_time();
    tempoRestante = fases[faseAtual].tempoLimite;
    tempoGasto = 0.0;
    pontuacaoFase = 0.0;
    cronometroAtivo = true;
    tempoEsgotado = false;

    ultimoTorqueEsq = 0.0;
    ultimoTorqueDir = (double)fases[faseAtual].valorCarga * fases[faseAtual].distanciaCarga;
    ultimoSigmaM = 0.0;
    expressaoTorqueEsq[0] = '\0';

    for (int i = 0; i < totalSlots; i++) {
        slots[i].ocupado = false;
        slots[i].pesoOcupante = 0;
    }

    int posicaoNoInventario = 0;

    for (int i = 0; i < totalPesos; i++) {
        pesos[i].disponivel = peso_esta_na_fase(&fases[faseAtual], pesos[i].carga);

        pesos[i].inventarioX = 100.0f + (posicaoNoInventario * 110.0f);
        pesos[i].inventarioY = 900.0f;
        if (pesos[i].disponivel) {
            posicaoNoInventario++;
        }

        pesos[i].destinoX = pesos[i].inventarioX;
        pesos[i].destinoY = pesos[i].inventarioY;
        pesos[i].slotAtual = -1;
    }

    printf("\n=== FASE %d === Carga %d na distancia %d (M = %d) | Tempo: %.0f s\n",
        faseAtual + 1,
        fases[faseAtual].valorCarga,
        fases[faseAtual].distanciaCarga,
        fases[faseAtual].valorCarga * fases[faseAtual].distanciaCarga,
        fases[faseAtual].tempoLimite);
}

// Ação unificada do botão Confirmar / Refazer (chamada pelo Mouse ou pela tecla Espaço)
void acionar_confirmar_ou_reset(Pesos peso[], Coordenadas slotsContraLanca[], int* indice_arrastado) {
    if (estadoFase == 0 && *indice_arrastado == -1 && cronometroAtivo) {
        // Para o cronômetro e guarda o Δt (RN01)
        cronometroAtivo = false;
        tempoGasto = al_get_time() - inicioFase;

        equilibrio = calcular_torque(slotsContraLanca, 13,
            fases[faseAtual].valorCarga, fases[faseAtual].distanciaCarga);

        if (equilibrio == 1) {
            anguloAlvo = -90.0f; // tomba para a esquerda
            estadoFase = 3;
            contadorTicksQueda = 0;
        }
        else if (equilibrio == -1) {
            anguloAlvo = 90.0f;  // tomba para a direita
            estadoFase = 3;
            contadorTicksQueda = 0;
        }
        else {
            // Equilíbrio perfeito: calcula pontuação (RF09, RN02, RN03)
            double dtCalculo = (tempoGasto < 1.0) ? 1.0 : tempoGasto;
            pontuacaoFase = 1000.0 * (1.0 / dtCalculo);
            pontuacaoTotal += pontuacaoFase;

            if (pontuacaoTotal > recorde) {
                recorde = pontuacaoTotal;
                salvar_recorde("recorde.txt", recorde);
            }

            anguloAlvo = 0.0f;
            estadoFase = 1; // sobe a carga
            quadroElevador = 0;
            contadorTicksElevador = 0;
            contadorTicksParado = 0;
        }
    }
    else if (estadoFase == 3) {
        // Reset imediato se estiver caindo ou caída (RN04)
        *indice_arrastado = -1;
        carregar_fase(faseAtual, peso, 5, slotsContraLanca, 13);
    }
}

int main() {
    if (!al_init()) {
        printf("Falha ao inicializar o Allegro.\n");
        return -1;
    }

    if (!al_init_primitives_addon()) {
        printf("Falha ao inicializar elementos primitivos.\n");
        return -1;
    }

    if (!al_init_image_addon()) {
        printf("Falha ao inicializar as imagens.\n");
        return -1;
    }

    al_init_font_addon();

    if (!al_install_mouse()) {
        printf("Falha ao inicializar o mouse.\n");
        return -1;
    }

    if (!al_install_keyboard()) {
        printf("Falha ao inicializar o teclado.\n");
        return -1;
    }

    ALLEGRO_TIMER* timer = al_create_timer(1.0 / 60.0);
    if (!timer) {
        printf("Falha ao criar o temporizador.\n");
        return -1;
    }

    bool estaTelaCheia = false;
    al_set_new_display_flags(ALLEGRO_WINDOWED);

    ALLEGRO_DISPLAY* display = al_create_display(1920, 1080);
    if (!display) {
        printf("Falha ao criar a janela.\n");
        al_destroy_timer(timer);
        return -1;
    }

    ALLEGRO_FONT* fonte = al_create_builtin_font();
    if (!fonte) {
        printf("Falha ao criar a fonte nativa.\n");
        al_destroy_display(display);
        al_destroy_timer(timer);
        return -1;
    }

    Coordenadas slotsContraLanca[13];
    slotsContraLanca[0].comecoX = 465; slotsContraLanca[0].fimX = 490;
    slotsContraLanca[1].comecoX = 495; slotsContraLanca[1].fimX = 520;
    slotsContraLanca[2].comecoX = 525; slotsContraLanca[2].fimX = 550;
    slotsContraLanca[3].comecoX = 555; slotsContraLanca[3].fimX = 580;
    slotsContraLanca[4].comecoX = 585; slotsContraLanca[4].fimX = 610;
    slotsContraLanca[5].comecoX = 615; slotsContraLanca[5].fimX = 640;
    slotsContraLanca[6].comecoX = 645; slotsContraLanca[6].fimX = 670;
    slotsContraLanca[7].comecoX = 675; slotsContraLanca[7].fimX = 700;
    slotsContraLanca[8].comecoX = 705; slotsContraLanca[8].fimX = 730;
    slotsContraLanca[9].comecoX = 735; slotsContraLanca[9].fimX = 760;
    slotsContraLanca[10].comecoX = 765; slotsContraLanca[10].fimX = 790;
    slotsContraLanca[11].comecoX = 795; slotsContraLanca[11].fimX = 820;
    slotsContraLanca[12].comecoX = 825; slotsContraLanca[12].fimX = 850;

    for (int i = 0; i < 13; i++) {
        slotsContraLanca[i].comecoY = 352.5f;
        slotsContraLanca[i].fimY = 398.5f;
        slotsContraLanca[i].ocupado = false;
        slotsContraLanca[i].pesoOcupante = 0;
    }

    Coordenadas slotsLanca[13];
    slotsLanca[0].comecoX = 692;
    slotsLanca[1].comecoX = 722;
    slotsLanca[2].comecoX = 752;
    slotsLanca[3].comecoX = 782;
    slotsLanca[4].comecoX = 812;
    slotsLanca[5].comecoX = 842;
    slotsLanca[6].comecoX = 872;
    slotsLanca[7].comecoX = 902;
    slotsLanca[8].comecoX = 932;
    slotsLanca[9].comecoX = 962;
    slotsLanca[10].comecoX = 992;
    slotsLanca[11].comecoX = 1022;
    slotsLanca[12].comecoX = 1052;

    for (int i = 0; i < 13; i++) {
        slotsLanca[i].comecoY = 371;
    }

    Coordenadas slotsCarga[13];
    slotsCarga[0].comecoX = 952;
    slotsCarga[1].comecoX = 982;
    slotsCarga[2].comecoX = 1012;
    slotsCarga[3].comecoX = 1042;
    slotsCarga[4].comecoX = 1072;
    slotsCarga[5].comecoX = 1102;
    slotsCarga[6].comecoX = 1132;
    slotsCarga[7].comecoX = 1162;
    slotsCarga[8].comecoX = 1192;
    slotsCarga[9].comecoX = 1222;
    slotsCarga[10].comecoX = 1252;
    slotsCarga[11].comecoX = 1282;
    slotsCarga[12].comecoX = 1312;

    for (int i = 0; i < 13; i++) {
        slotsCarga[i].comecoY = 868;
    }

    ALLEGRO_BITMAP* cenario = al_load_bitmap("assets/cenario/cenario_temp.png");
    ALLEGRO_BITMAP* cenariomenu = al_load_bitmap("assets/cenario/cenario_menu.png");
    ALLEGRO_BITMAP* grua = al_load_bitmap("assets/sprites/grua_sem_elevador.png");
    ALLEGRO_BITMAP* elevador = al_load_bitmap("assets/sprites/elevador_grua_spritesheet.png");

    Pesos peso[5];
    peso[0].sprite = al_load_bitmap("assets/sprites/peso_grua_1.png");  peso[0].carga = 1;
    peso[1].sprite = al_load_bitmap("assets/sprites/peso_grua_2.png");  peso[1].carga = 2;
    peso[2].sprite = al_load_bitmap("assets/sprites/peso_grua_3.png");  peso[2].carga = 3;
    peso[3].sprite = al_load_bitmap("assets/sprites/peso_grua_5.png");  peso[3].carga = 5;
    peso[4].sprite = al_load_bitmap("assets/sprites/peso_grua_10.png"); peso[4].carga = 10;

    for (int i = 0; i < 5; i++) {
        peso[i].origemX = 0;
        peso[i].origemY = 0;
        peso[i].origemW = 32;
        peso[i].origemH = 32;
        peso[i].destinoW = 80;
        peso[i].destinoH = 80;
    }

    Pesos carga[4];
    carga[0].sprite = al_load_bitmap("assets/sprites/carga5.png");  carga[0].carga = 5;
    carga[1].sprite = al_load_bitmap("assets/sprites/carga10.png"); carga[1].carga = 10;
    carga[2].sprite = al_load_bitmap("assets/sprites/carga20.png"); carga[2].carga = 20;
    carga[3].sprite = al_load_bitmap("assets/sprites/carga30.png"); carga[3].carga = 30;

    for (int i = 0; i < 4; i++) {
        carga[i].origemX = 0;
        carga[i].origemY = 0;
        carga[i].origemW = 32;
        carga[i].origemH = 32;
        carga[i].destinoW = 80;
        carga[i].destinoH = 80;
    }

    if (!validar_fases(carga, 4)) {
        printf("Dados das fases invalidos.\n");
        al_destroy_font(fonte);
        al_destroy_display(display);
        al_destroy_timer(timer);
        return -1;
    }

    recorde = carregar_recorde("recorde.txt");

    ALLEGRO_EVENT_QUEUE* fila_eventos = al_create_event_queue();
    if (!fila_eventos) {
        al_destroy_font(fonte);
        al_destroy_display(display);
        al_destroy_timer(timer);
        return -1;
    }

    al_register_event_source(fila_eventos, al_get_display_event_source(display));
    al_register_event_source(fila_eventos, al_get_timer_event_source(timer));
    al_register_event_source(fila_eventos, al_get_keyboard_event_source());
    al_register_event_source(fila_eventos, al_get_mouse_event_source());

    bool rodando = true;
    bool redesenhar = true;
    int indice_arrastado = -1;

    al_start_timer(timer);

    while (rodando) {
        ALLEGRO_EVENT evento;
        al_wait_for_event(fila_eventos, &evento);

        if (evento.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
            rodando = false;
        }
        else if (evento.type == ALLEGRO_EVENT_KEY_DOWN) {
            if (estaTelaCheia && evento.keyboard.keycode == ALLEGRO_KEY_F11) {
                al_toggle_display_flag(display, ALLEGRO_FULLSCREEN_WINDOW, false);
                al_toggle_display_flag(display, ALLEGRO_WINDOWED, true);
                estaTelaCheia = false;
            }
            else if (!estaTelaCheia && evento.keyboard.keycode == ALLEGRO_KEY_F11) {
                al_toggle_display_flag(display, ALLEGRO_FULLSCREEN_WINDOW, true);
                al_toggle_display_flag(display, ALLEGRO_WINDOWED, false);
                estaTelaCheia = true;
            }

            // ESC volta para o menu principal
            if (evento.keyboard.keycode == ALLEGRO_KEY_ESCAPE) {
                if (telaAtual != TELA_MENU) {
                    indice_arrastado = -1;
                    cronometroAtivo = false;
                    telaAtual = TELA_MENU;
                }
            }

            // Atalhos exclusivos durante o jogo
            if (telaAtual == TELA_JOGO) {
                if (evento.keyboard.keycode == ALLEGRO_KEY_R) {
                    pontuacaoTotal = 0.0;
                    indice_arrastado = -1;
                    carregar_fase(0, peso, 5, slotsContraLanca, 10);
                }

                if (evento.keyboard.keycode == ALLEGRO_KEY_N) {
                    int proximaFase = (faseAtual + 1) % 5;
                    carregar_fase(proximaFase, peso, 5, slotsContraLanca, 10);
                    indice_arrastado = -1;
                }

                if (evento.keyboard.keycode == ALLEGRO_KEY_SPACE) {
                    acionar_confirmar_ou_reset(peso, slotsContraLanca, &indice_arrastado);
                }
            }
        }
        else if (evento.type == ALLEGRO_EVENT_TIMER) {
            if (telaAtual == TELA_JOGO) {
                // Atualização da física de rotação da grua
                if (anguloGrua < anguloAlvo) {
                    velocidadeAngularGrua += aceleracaoGrua;
                    anguloGrua += velocidadeAngularGrua;
                    if (anguloGrua > anguloAlvo) {
                        anguloGrua = anguloAlvo;
                        velocidadeAngularGrua = 0.0f;
                    }
                }
                else if (anguloGrua > anguloAlvo) {
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

                // Cronômetro regressivo
                if (cronometroAtivo) {
                    tempoRestante = fases[faseAtual].tempoLimite - (al_get_time() - inicioFase);

                    if (tempoRestante <= 0.0) {
                        tempoRestante = 0.0;
                        cronometroAtivo = false;
                        tempoEsgotado = true;

                        if (indice_arrastado != -1) {
                            peso[indice_arrastado].destinoX = peso[indice_arrastado].inventarioX;
                            peso[indice_arrastado].destinoY = peso[indice_arrastado].inventarioY;
                            indice_arrastado = -1;
                        }

                        equilibrio = calcular_torque(slotsContraLanca, 10,
                            fases[faseAtual].valorCarga, fases[faseAtual].distanciaCarga);

                        anguloAlvo = (equilibrio == 1) ? -90.0f : 90.0f;
                        estadoFase = 3;
                        contadorTicksQueda = 0;
                    }
                }

                // Estado de queda: espera 1,5 s (90 ticks) após tocar o chão para o aluno ler o torque ou apertar Espaço
                if (estadoFase == 3 && anguloGrua == anguloAlvo) {
                    contadorTicksQueda++;
                    if (contadorTicksQueda >= 90) {
                        carregar_fase(faseAtual, peso, 5, slotsContraLanca, 10);
                        indice_arrastado = -1;
                    }
                }

                // Animação de vitória: recolhe o cabo
                if (estadoFase == 1) {
                    if (quadroElevador < 14) {
                        contadorTicksElevador++;
                        if (contadorTicksElevador >= 8) {
                            quadroElevador++;
                            contadorTicksElevador = 0;
                        }
                    }
                    else {
                        contadorTicksParado++;
                        if (contadorTicksParado >= 60) {
                            estadoFase = 2;
                        }
                    }
                }

                // Avança de fase ou vai para a tela de vitória final
                if (estadoFase == 2) {
                    int proximaFase = faseAtual + 1;
                    if (proximaFase >= 5) {
                        ultimaPontuacaoFinal = pontuacaoTotal;
                        pontuacaoTotal = 0.0;
                        indice_arrastado = -1;
                        telaAtual = TELA_VITORIA_FINAL;
                    }
                    else {
                        carregar_fase(proximaFase, peso, 5, slotsContraLanca, 10);
                        indice_arrastado = -1;
                    }
                }
            }

            redesenhar = true;
        }
        else if (evento.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN) {
            float mx = (float)evento.mouse.x;
            float my = (float)evento.mouse.y;

            if (telaAtual == TELA_MENU) {
                // Botão JOGAR (760, 420 a 1160, 500)
                if (ponto_no_retangulo(mx, my, 760, 420, 1160, 500)) {
                    pontuacaoTotal = 0.0;
                    indice_arrastado = -1;
                    carregar_fase(0, peso, 5, slotsContraLanca, 10);
                    telaAtual = TELA_JOGO;
                }
                // Botão RECORDES (760, 530 a 1160, 610)
                else if (ponto_no_retangulo(mx, my, 760, 530, 1160, 610)) {
                    recorde = carregar_recorde("recorde.txt");
                    telaAtual = TELA_RECORDES;
                }
                // Botão SAIR (760, 640 a 1160, 720)
                else if (ponto_no_retangulo(mx, my, 760, 640, 1160, 720)) {
                    rodando = false;
                }
            }
            else if (telaAtual == TELA_RECORDES) {
                // Botão VOLTAR AO MENU (760, 650 a 1160, 730)
                if (ponto_no_retangulo(mx, my, 760, 650, 1160, 730)) {
                    telaAtual = TELA_MENU;
                }
            }
            else if (telaAtual == TELA_VITORIA_FINAL) {
                // Botão JOGAR NOVAMENTE (760, 540 a 1160, 620)
                if (ponto_no_retangulo(mx, my, 760, 540, 1160, 620)) {
                    pontuacaoTotal = 0.0;
                    carregar_fase(0, peso, 5, slotsContraLanca, 10);
                    telaAtual = TELA_JOGO;
                }
                // Botão MENU INICIAL (760, 650 a 1160, 730)
                else if (ponto_no_retangulo(mx, my, 760, 650, 1160, 730)) {
                    telaAtual = TELA_MENU;
                }
            }
            else if (telaAtual == TELA_JOGO) {
                // Botão MENU no canto superior esquerdo (30, 30 a 210, 85)
                if (ponto_no_retangulo(mx, my, 30, 30, 210, 85)) {
                    indice_arrastado = -1;
                    cronometroAtivo = false;
                    telaAtual = TELA_MENU;
                }
                // Botão CONFIRMAR / REFAZER (1520, 890 a 1870, 970)
                else if (ponto_no_retangulo(mx, my, 1520, 890, 1870, 970)) {
                    acionar_confirmar_ou_reset(peso, slotsContraLanca, &indice_arrastado);
                }
                // Captura de blocos no inventário ou na contralança
                else if (indice_arrastado == -1 && estadoFase == 0 && cronometroAtivo) {
                    for (int i = 0; i < 5; i++) {
                        if (!peso[i].disponivel) {
                            continue;
                        }

                        if (ponto_no_retangulo(mx, my,
                            peso[i].destinoX, peso[i].destinoY,
                            peso[i].destinoX + peso[i].destinoW, peso[i].destinoY + peso[i].destinoH)) {

                            indice_arrastado = i;

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
        }
        else if (evento.type == ALLEGRO_EVENT_MOUSE_AXES) {
            if (telaAtual == TELA_JOGO && indice_arrastado != -1) {
                peso[indice_arrastado].destinoX = evento.mouse.x - (peso[indice_arrastado].destinoW / 2);
                peso[indice_arrastado].destinoY = evento.mouse.y - (peso[indice_arrastado].destinoH / 2);
            }
        }
        else if (evento.type == ALLEGRO_EVENT_MOUSE_BUTTON_UP) {
            if (telaAtual == TELA_JOGO && indice_arrastado != -1) {
                bool foiPosicionado = false;

                for (int i = 0; i < 13; i++) {
                    if (ponto_no_retangulo((float)evento.mouse.x, (float)evento.mouse.y,
                        slotsContraLanca[i].comecoX, slotsContraLanca[i].comecoY,
                        slotsContraLanca[i].fimX, slotsContraLanca[i].fimY) &&
                        !slotsContraLanca[i].ocupado)
                    {
                        peso[indice_arrastado].destinoX = slotsContraLanca[i].comecoX - 25;
                        peso[indice_arrastado].destinoY = slotsContraLanca[i].comecoY - 10;
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

                indice_arrastado = -1;
            }
        }

        // ==================== RENDERIZAÇÃO ====================
        if (redesenhar && al_is_event_queue_empty(fila_eventos)) {
            al_clear_to_color(al_map_rgb(20, 24, 30));

            if (telaAtual == TELA_MENU) {
                if (cenariomenu) {
                    al_draw_bitmap(cenariomenu, 0, 0, 0);
                }

                desenhar_texto_escalado(fonte, al_map_rgb(240, 190, 40), 960, 210, 4.5f, ALLEGRO_ALIGN_CENTRE, "EQUILIBRIO: TORQUE CHALLENGE");
                desenhar_texto_escalado(fonte, al_map_rgb(220, 220, 220), 960, 290, 2.5f, ALLEGRO_ALIGN_CENTRE, "Equilibre a grua da obra calculando o Momento de uma Forca");

                // Botão JOGAR
                al_draw_filled_rectangle(760, 420, 1160, 500, al_map_rgb(114, 47, 55));
                al_draw_rectangle(760, 420, 1160, 500, al_map_rgb(255, 255, 255), 3);
                desenhar_texto_escalado(fonte, al_map_rgb(255, 255, 255), 960, 448, 3.0f, ALLEGRO_ALIGN_CENTRE, "JOGAR");

                // Botão RECORDES
                al_draw_filled_rectangle(760, 530, 1160, 610, al_map_rgb(114, 47, 55));
                al_draw_rectangle(760, 530, 1160, 610, al_map_rgb(255, 255, 255), 3);
                desenhar_texto_escalado(fonte, al_map_rgb(255, 255, 255), 960, 558, 3.0f, ALLEGRO_ALIGN_CENTRE, "RECORDES");

                // Botão SAIR
                al_draw_filled_rectangle(760, 640, 1160, 720, al_map_rgb(114, 47, 55));
                al_draw_rectangle(760, 640, 1160, 720, al_map_rgb(255, 255, 255), 3);
                desenhar_texto_escalado(fonte, al_map_rgb(255, 255, 255), 960, 668, 3.0f, ALLEGRO_ALIGN_CENTRE, "SAIR");

                desenhar_texto_escalado(fonte, al_map_rgb(70, 90, 120), 960, 775, 2.0f, ALLEGRO_ALIGN_CENTRE, "Use o MOUSE para jogar  |  [F11] Tela Cheia");
            }
            else if (telaAtual == TELA_RECORDES) {
                if (cenario) {
                    al_draw_bitmap(cenario, 0, 0, 0);
                }
                al_draw_filled_rectangle(400, 180, 1520, 820, al_map_rgba(15, 20, 28, 235));
                al_draw_rectangle(400, 180, 1520, 820, al_map_rgb(255, 255, 255), 4);

                desenhar_texto_escalado(fonte, al_map_rgb(255, 255, 255), 960, 240, 4.0f, ALLEGRO_ALIGN_CENTRE, "RECORDE DA OBRA");

                char textoRecorde[128];
                snprintf(textoRecorde, sizeof(textoRecorde), "MAIOR PONTUACAO TOTAL: %.2f pts", recorde);
                desenhar_texto_escalado(fonte, al_map_rgb(255, 255, 255), 960, 380, 3.5f, ALLEGRO_ALIGN_CENTRE, textoRecorde);

                desenhar_texto_escalado(fonte, al_map_rgb(180, 210, 240), 960, 480, 2.5f, ALLEGRO_ALIGN_CENTRE, "Regra de Pontuacao: Pontos = 1000 x (1 / Tempo Gasto)");
                desenhar_texto_escalado(fonte, al_map_rgb(180, 210, 240), 960, 530, 2.2f, ALLEGRO_ALIGN_CENTRE, "Quanto mais rapido equilibrar as 5 fases, maior a nota!");

                al_draw_filled_rectangle(760, 650, 1160, 730, al_map_rgb(114, 47, 55));
                al_draw_rectangle(760, 650, 1160, 730, al_map_rgb(255, 255, 255), 3);
                desenhar_texto_escalado(fonte, al_map_rgb(255, 255, 255), 960, 678, 3.0f, ALLEGRO_ALIGN_CENTRE, "VOLTAR AO MENU");
            }
            else if (telaAtual == TELA_VITORIA_FINAL) {
                if (cenario) {
                    al_draw_bitmap(cenario, 0, 0, 0);
                }
                al_draw_filled_rectangle(400, 180, 1520, 820, al_map_rgba(15, 25, 20, 235));
                al_draw_rectangle(400, 180, 1520, 820, al_map_rgb(60, 200, 90), 4);

                desenhar_texto_escalado(fonte, al_map_rgb(80, 230, 110), 960, 240, 4.0f, ALLEGRO_ALIGN_CENTRE, "OBRA CONCLUIDA COM SUCESSO!");
                desenhar_texto_escalado(fonte, al_map_rgb(255, 255, 255), 960, 320, 2.5f, ALLEGRO_ALIGN_CENTRE, "Voce equilibrou todas as 5 fases do desafio de torque!");

                char bufFinal[128], bufRec[128];
                snprintf(bufFinal, sizeof(bufFinal), "Sua Pontuacao Final: %.2f pts", ultimaPontuacaoFinal);
                snprintf(bufRec, sizeof(bufRec), "Recorde Registrado : %.2f pts", recorde);
                desenhar_texto_escalado(fonte, al_map_rgb(80, 230, 110), 960, 400, 3.2f, ALLEGRO_ALIGN_CENTRE, bufFinal);
                desenhar_texto_escalado(fonte, al_map_rgb(200, 220, 255), 960, 460, 2.8f, ALLEGRO_ALIGN_CENTRE, bufRec);

                al_draw_filled_rectangle(760, 540, 1160, 620, al_map_rgb(45, 160, 70));
                al_draw_rectangle(760, 540, 1160, 620, al_map_rgb(255, 255, 255), 3);
                desenhar_texto_escalado(fonte, al_map_rgb(255, 255, 255), 960, 568, 2.6f, ALLEGRO_ALIGN_CENTRE, "JOGAR NOVAMENTE");

                al_draw_filled_rectangle(760, 650, 1160, 730, al_map_rgb(70, 90, 120));
                al_draw_rectangle(760, 650, 1160, 730, al_map_rgb(255, 255, 255), 3);
                desenhar_texto_escalado(fonte, al_map_rgb(255, 255, 255), 960, 678, 2.8f, ALLEGRO_ALIGN_CENTRE, "MENU INICIAL");
            }
            else if (telaAtual == TELA_JOGO) {
                al_draw_bitmap(cenario, 0, 0, 0);
                al_draw_scaled_rotated_bitmap(grua, 260, 345, 250 + 650, 50 + 862.5f, 2.5f, 2.5f, anguloGrua * ALLEGRO_PI / 180.0f, 0);

                if (anguloGrua == 0.0f) {
                    int indiceSlotCarga = fases[faseAtual].distanciaCarga - 1;
                    int indiceSpriteCarga = buscar_indice_carga(carga, 4, fases[faseAtual].valorCarga);

                    float subidaCarga = quadroElevador * 12.5f;

                    al_draw_scaled_bitmap(elevador, quadroElevador * 250, 0, 250, 250, slotsLanca[indiceSlotCarga].comecoX, slotsLanca[indiceSlotCarga].comecoY, 625, 625, 0);
                    al_draw_scaled_bitmap(carga[indiceSpriteCarga].sprite, 0, 0, 250, 250, slotsCarga[indiceSlotCarga].comecoX, slotsCarga[indiceSlotCarga].comecoY - subidaCarga, 625, 625, 0);

                    for (int i = 0; i < 5; i++) {
                        if (!peso[i].disponivel) {
                            continue;
                        }

                        al_draw_scaled_bitmap(peso[i].sprite, peso[i].origemX, peso[i].origemY,
                            peso[i].origemW, peso[i].origemH,
                            peso[i].destinoX, peso[i].destinoY,
                            peso[i].destinoW, peso[i].destinoH, 0);

                        /*char txtPeso[16];
                        snprintf(txtPeso, sizeof(txtPeso), "F=%d", peso[i].carga);
                        desenhar_texto_escalado(fonte, al_map_rgb(255, 255, 100),
                            peso[i].destinoX + 40, peso[i].destinoY + 84, 2.0f, ALLEGRO_ALIGN_CENTRE, txtPeso);*/
                    }

                    for (int i = 0; i < 13; i++) {
                        al_draw_rectangle(slotsContraLanca[i].comecoX, slotsContraLanca[i].comecoY,
                            slotsContraLanca[i].fimX, slotsContraLanca[i].fimY, al_map_rgb(30, 30, 30), 2);

                        int distSlot = 13 - i;
                        char txtDist[8];
                        snprintf(txtDist, sizeof(txtDist), "%d", distSlot);
                        float centroSlotX = (slotsContraLanca[i].comecoX + slotsContraLanca[i].fimX) / 2.0f;
                        desenhar_texto_escalado(fonte, al_map_rgb(255, 255, 255), centroSlotX, slotsContraLanca[i].comecoY - 20, 1.8f, ALLEGRO_ALIGN_CENTRE, txtDist);
                    }
                }

                // ==================== HUD ====================
                al_draw_filled_rectangle(40, 30, 230, 85, al_map_rgba(114, 47, 55, 220));
                al_draw_rectangle(40, 30, 230, 85, al_map_rgb(255, 255, 255), 3);
                desenhar_texto_escalado(fonte, al_map_rgb(255, 255, 255), 137, 48, 2.2f, ALLEGRO_ALIGN_CENTRE, "MENU [ESC]");

                char infoFase[128], infoCarga[128];

                al_draw_filled_rectangle(40, 105, 230, 145, al_map_rgba(15, 20, 28, 215));
                snprintf(infoFase, sizeof(infoFase), "FASE %d / 5", faseAtual + 1);

                al_draw_filled_rectangle(1500, 600, 1850, 850, al_map_rgb(114, 47, 55));
                al_draw_rectangle(1500, 600, 1850, 850, al_map_rgb(255, 255, 255), 3);

                al_draw_filled_rectangle(40, 155, 1230, 215, al_map_rgba(15, 20, 28, 215));
                snprintf(infoCarga, sizeof(infoCarga), "CARGA NA LANCA (Direita): Forca F = %d  |  Distancia d = %d", fases[faseAtual].valorCarga, fases[faseAtual].distanciaCarga);

                desenhar_texto_escalado(fonte, al_map_rgb(255, 255, 255), 50, 118, 2.0f, ALLEGRO_ALIGN_LEFT, infoFase);
                desenhar_texto_escalado(fonte, al_map_rgb(255, 255, 255), 50, 160, 2.2f, ALLEGRO_ALIGN_LEFT, infoCarga);
                desenhar_texto_escalado(fonte, al_map_rgb(255, 255, 255), 1510, 610, 2.2f, ALLEGRO_ALIGN_LEFT, "FORMULA: M = F x d");
                desenhar_texto_escalado(fonte, al_map_rgb(180, 220, 250), 1510, 635, 1.1f, ALLEGRO_ALIGN_LEFT, "Equilibrio: Torque Esq = Torque Dir");
                desenhar_texto_escalado(fonte, al_map_rgb(180, 220, 250), 1510, 655, 1.1f, ALLEGRO_ALIGN_LEFT, "Equilibrio: Sigma M = 0");
                desenhar_texto_escalado(fonte, al_map_rgb(180, 220, 250), 50, 195, 2.0f, ALLEGRO_ALIGN_LEFT, "Arraste os contrapesos para as posicoes 1 a 10 da contralanca e confirme!");

                float proporcaoTempo = (float)(tempoRestante / fases[faseAtual].tempoLimite);
                ALLEGRO_COLOR corBarra = al_map_rgb(60, 180, 75);
                if (proporcaoTempo < 0.5f) corBarra = al_map_rgb(240, 190, 40);
                if (proporcaoTempo < 0.25f) corBarra = al_map_rgb(220, 50, 40);

                al_draw_filled_rectangle(760, 35, 1160, 75, al_map_rgb(40, 40, 40));
                al_draw_filled_rectangle(760, 35, 760 + 400 * proporcaoTempo, 75, corBarra);
                al_draw_rectangle(760, 35, 1160, 75, al_map_rgb(0, 0, 0), 2);

                char txtTempo[64];
                snprintf(txtTempo, sizeof(txtTempo), "TEMPO: %.1f s", tempoRestante);
                desenhar_texto_escalado(fonte, al_map_rgb(255, 255, 255), 960, 46, 2.3f, ALLEGRO_ALIGN_CENTRE, txtTempo);

                al_draw_filled_rectangle(1380, 30, 1890, 115, al_map_rgba(15, 20, 28, 215));
                al_draw_rectangle(1380, 30, 1890, 115, al_map_rgb(100, 180, 255), 2);

                char txtPontos[128], txtRecHUD[128];
                snprintf(txtPontos, sizeof(txtPontos), "PONTOS: %.1f", pontuacaoTotal);
                snprintf(txtRecHUD, sizeof(txtRecHUD), "RECORDE: %.1f", recorde);
                desenhar_texto_escalado(fonte, al_map_rgb(255, 255, 255), 1405, 45, 2.3f, ALLEGRO_ALIGN_LEFT, txtPontos);
                desenhar_texto_escalado(fonte, al_map_rgb(240, 190, 40), 1405, 80, 2.2f, ALLEGRO_ALIGN_LEFT, txtRecHUD);

                // ==================== PAINEL DE EXPRESSÃO COMPLETA DO TORQUE (PÓS-CONFIRMAÇÃO) ====================
                if (estadoFase == 1 || estadoFase == 3) {
                    al_draw_filled_rectangle(410, 305, 1510, 535, al_map_rgba(15, 20, 28, 240));
                    ALLEGRO_COLOR corBorda = (estadoFase == 1) ? al_map_rgb(60, 210, 90) : al_map_rgb(230, 60, 50);
                    al_draw_rectangle(410, 305, 1510, 535, corBorda, 3);

                    char lin1[128], lin2[128], lin3[128], lin4[128];
                    snprintf(lin1, sizeof(lin1), "CALCULO DETALHADO DO TORQUE (M = F x d):");
                    snprintf(lin2, sizeof(lin2), "Contralanca (Esq): %s", expressaoTorqueEsq);
                    snprintf(lin3, sizeof(lin3), "Lanca (Dir)      : %dx%d = %.0f", fases[faseAtual].valorCarga, fases[faseAtual].distanciaCarga, ultimoTorqueDir);

                    if (estadoFase == 1) {
                        snprintf(lin4, sizeof(lin4), "Sigma M = %.0f - %.0f = 0  ->  EQUILIBRIO! (+%.1f pts)", ultimoTorqueEsq, ultimoTorqueDir, pontuacaoFase);
                    }
                    else if (tempoEsgotado) {
                        snprintf(lin4, sizeof(lin4), "TEMPO ESGOTADO! Sigma M = %.0f -> GRUA TOMBOU!", ultimoSigmaM);
                    }
                    else {
                        snprintf(lin4, sizeof(lin4), "Sigma M = %.0f - %.0f = %.0f  ->  DESEQUILIBRIO (GRUA TOMBOU!)", ultimoTorqueEsq, ultimoTorqueDir, ultimoSigmaM);
                    }

                    desenhar_texto_escalado(fonte, al_map_rgb(220, 220, 220), 960, 342, 2.3f, ALLEGRO_ALIGN_CENTRE, lin1);
                    desenhar_texto_escalado(fonte, al_map_rgb(220, 220, 220), 960, 382, 2.2f, ALLEGRO_ALIGN_CENTRE, lin2);
                    desenhar_texto_escalado(fonte, al_map_rgb(220, 220, 220), 960, 418, 2.2f, ALLEGRO_ALIGN_CENTRE, lin3);
                    desenhar_texto_escalado(fonte, corBorda, 960, 465, 2.3f, ALLEGRO_ALIGN_CENTRE, lin4);
                }

                // ==================== BOTÃO CONFIRMAR / REFAZER ====================
                if (estadoFase == 0) {
                    al_draw_filled_rectangle(1500, 890, 1850, 970, al_map_rgb(45, 160, 70));
                    al_draw_rectangle(1500, 890, 1850, 970, al_map_rgb(255, 255, 255), 3);
                    desenhar_texto_escalado(fonte, al_map_rgb(255, 255, 255), 1675, 920, 2.2f, ALLEGRO_ALIGN_CENTRE, "CONFIRMAR [ESPACO]");
                }
                else if (estadoFase == 3) {
                    al_draw_filled_rectangle(1500, 890, 1850, 970, al_map_rgb(210, 65, 50));
                    al_draw_rectangle(1500, 890, 1850, 970, al_map_rgb(255, 255, 255), 3);
                    desenhar_texto_escalado(fonte, al_map_rgb(255, 255, 255), 1680, 920, 2.2f, ALLEGRO_ALIGN_CENTRE, "REFAZER [ESPACO]");
                }
            }

            al_flip_display();
            redesenhar = false;
        }
    }

    // Limpeza de memória (RNF02)
    al_destroy_bitmap(cenario);
    al_destroy_bitmap(grua);
    al_destroy_bitmap(elevador);
    for (int i = 0; i < 5; i++) {
        al_destroy_bitmap(peso[i].sprite);
    }
    for (int i = 0; i < 4; i++) {
        al_destroy_bitmap(carga[i].sprite);
    }

    al_destroy_font(fonte);
    al_destroy_event_queue(fila_eventos);
    al_destroy_display(display);
    al_destroy_timer(timer);

    return 0;
}