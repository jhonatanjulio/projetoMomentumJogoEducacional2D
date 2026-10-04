#include <stdio.h>
#include <stdbool.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_primitives.h>

int main() {
    // 1. Inicializa o núcleo do Allegro
    if (!al_init()) {
        printf("Falha ao inicializar o Allegro.\n");
        return -1;
    }

    al_init_primitives_addon();

    if (!al_init_image_addon()) {
        printf("Falha ao inicializar as imagens.");
        return -1;
    }

    // 2. Instala o driver do mouse
    if (!al_install_mouse()) {
        printf("Falha ao inicializar o mouse.\n");
        return -1;
    }

    if (!al_install_keyboard()) { // inicializa o teclado
        printf(stderr, "Falha ao inicializar o teclado.\n");
        return -1;
    }

    // 3. Cria o temporizador para 60 FPS (1.0 / 60.0)
    ALLEGRO_TIMER* timer = al_create_timer(1.0 / 60.0);
    if (!timer) {
        printf("Falha ao criar o temporizador.\n");
        return -1;
    }

    bool estaTelaCheia = true;
    al_set_new_display_flags(ALLEGRO_FULLSCREEN_WINDOW); // seta flag de fullscreen mode


    // 4. Cria a janela do jogo
    ALLEGRO_DISPLAY* display = al_create_display(1920, 1080);
    if (!display) {
        printf("Falha ao criar a janela.\n");
        al_destroy_timer(timer);
        return -1;
    }

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
        int carga;
    } Pesos;

    typedef struct Coordenadas { // struct padrão coordenadas
        float comecoX;
        float comecoY;
        float fimX;
        float fimY;
        bool ocupado;
    } Coordenadas;

    Coordenadas slotsContraLanca[10];

    slotsContraLanca[0].comecoX = 555;
    slotsContraLanca[0].fimX = 580;
    slotsContraLanca[0].ocupado = false;
    // +5x de espaçamento entre os pesos
    slotsContraLanca[1].comecoX = 585;
    slotsContraLanca[1].fimX = 610;
    slotsContraLanca[1].ocupado = false;
    // +5x de espaçamento entre os pesos
    slotsContraLanca[2].comecoX = 615;
    slotsContraLanca[2].fimX = 640;
    slotsContraLanca[2].ocupado = false;
    // +5x de espaçamento entre os pesos
    slotsContraLanca[3].comecoX = 645;
    slotsContraLanca[3].fimX = 670;
    slotsContraLanca[3].ocupado = false;
    // +5x de espaçamento entre os pesos
    slotsContraLanca[4].comecoX = 675;
    slotsContraLanca[4].fimX = 700;
    slotsContraLanca[4].ocupado = false;
    // +5x de espaçamento entre os pesos
    slotsContraLanca[5].comecoX = 705;
    slotsContraLanca[5].fimX = 730;
    slotsContraLanca[5].ocupado = false;
    // +5x de espaçamento entre os pesos
    slotsContraLanca[6].comecoX = 735;
    slotsContraLanca[6].fimX = 760;
    slotsContraLanca[6].ocupado = false;
    // +5x de espaçamento entre os pesos
    slotsContraLanca[7].comecoX = 765;
    slotsContraLanca[7].fimX = 790;
    slotsContraLanca[7].ocupado = false;
    // +5x de espaçamento entre os pesos
    slotsContraLanca[8].comecoX = 795;
    slotsContraLanca[8].fimX = 820;
    slotsContraLanca[8].ocupado = false;
    // +5x de espaçamento entre os pesos
    slotsContraLanca[9].comecoX = 825;
    slotsContraLanca[9].fimX = 850;
    slotsContraLanca[9].ocupado = false;

    for (int i = 0; i < 10; i++) {
        slotsContraLanca[i].comecoY = 352.5;
        slotsContraLanca[i].fimY = 398.5;
        slotsContraLanca[i].ocupado = false;
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

    // altere o index do array pra mudar de slot (é para deixar em variável dinâmica no futuro)
    peso[4].destinoX = slotsContraLanca[3].comecoX - 25;
    peso[4].destinoY = slotsContraLanca[3].comecoY - 10;

    for (int i = 0; i < 5; i++) { // valores fixos
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

    // 5. Cria a fila que organiza os eventos (timer, mouse, janela)
    ALLEGRO_EVENT_QUEUE* fila_eventos = al_create_event_queue();
    if (!fila_eventos) {
        printf("Falha ao criar a fila de eventos.\n");
        al_destroy_display(display);
        al_destroy_timer(timer);
        return -1;
    }

    // Registra as três fontes de eventos na fila
    al_register_event_source(fila_eventos, al_get_display_event_source(display));
    al_register_event_source(fila_eventos, al_get_timer_event_source(timer));
    al_register_event_source(fila_eventos, al_get_keyboard_event_source());
    al_register_event_source(fila_eventos, al_get_mouse_event_source());

    // 6. Configuração antes do loop principal
    bool rodando = true;
    bool redesenhar = true;

    al_start_timer(timer);

    // 7. Loop principal do jogo (Game Loop)
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
        }

        else if (evento.type == ALLEGRO_EVENT_TIMER) {
            // O timer disparou (momento de atualizar o frame)
            redesenhar = true;
        }
        else if (evento.type == ALLEGRO_EVENT_MOUSE_AXES) {
            // O mouse foi movido (visualize no console)
            printf("Mouse moveu para: x=%d, y=%d\n", evento.mouse.x, evento.mouse.y);
        }
        else if (evento.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN) {
            // Um botao do mouse foi clicado
            printf("Clique no mouse! x=%d, y=%d\n", evento.mouse.x, evento.mouse.y);
        }

        // 8. Etapa de Renderização (Desenho na tela)
        // Só desenha se for a hora certa (redesenhar) e não houver outros eventos travados na fila
        if (redesenhar && al_is_event_queue_empty(fila_eventos)) {
            // Pinta o fundo da tela de preto (R:0, G:0, B:0)
            al_clear_to_color(al_map_rgb(0, 0, 0));

            // Desenha o cenario e o sprite da grua
            al_draw_bitmap(cenario, 0, 0, 0);
            al_draw_scaled_bitmap(grua, 0, 0, 500, 500, 250, 50, 1250, 1250, 0);
            al_draw_scaled_bitmap(elevador, 0, 0, 250, 250, slotsLanca[9].comecoX, slotsLanca[9].comecoY, 625, 625, 0); // comecoX = 782, +30x para passar pro proximo slot

            al_draw_filled_rectangle(555, 352.5, 580, 398.5, al_map_rgb(0, 0, 0)); // rect temporario
            al_draw_filled_rectangle(585, 352.5, 610, 398.5, al_map_rgb(0, 0, 0)); // rect temporario

            al_draw_scaled_bitmap(peso[4].sprite, peso[4].origemX, peso[4].origemY, peso[4].origemW, peso[4].origemH, peso[4].destinoX, peso[4].destinoY, peso[4].destinoW, peso[4].destinoH, 0); //peso comecoX = slot comecoX - 25, peso comecoY = slot comecoY - 10
            al_draw_scaled_bitmap(carga[3].sprite, carga[3].origemX, carga[3].origemY, carga[3].origemW, carga[3].origemH, carga[3].destinoX, carga[3].destinoY, carga[3].destinoW, carga[3].destinoH, 0); // comecoX = 1042, +30x para passar pro proximo slot junto do elevador


            // Joga as alterações para a tela visível
            al_flip_display();

            redesenhar = false;
        }
    }

    // 9. Encerramento: destroi as estruturas para não vazar memória (RNF02)
    al_destroy_event_queue(fila_eventos);
    al_destroy_display(display);
    al_destroy_timer(timer);

    return 0;
}