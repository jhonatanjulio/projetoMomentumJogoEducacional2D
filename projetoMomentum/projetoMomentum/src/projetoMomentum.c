#include <stdio.h>
#include <stdbool.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_image.h>

int main() {
    // 1. Inicializa o núcleo do Allegro
    if (!al_init()) {
        printf("Falha ao inicializar o Allegro.\n");
        return -1;
    }

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

    bool isFullWindowed = true;
    al_set_new_display_flags(ALLEGRO_FULLSCREEN_WINDOW); // seta flag de fullscreen mode


    // 4. Cria a janela do jogo
    ALLEGRO_DISPLAY* display = al_create_display(1920, 1080);
    if (!display) {
        printf("Falha ao criar a janela.\n");
        al_destroy_timer(timer);
        return -1;
    }

    ALLEGRO_BITMAP* cenario = al_load_bitmap("assets/cenario/cenario_temp.png");
    ALLEGRO_BITMAP* grua = al_load_bitmap("assets/sprites/grua_sem_elevador.png");
    ALLEGRO_BITMAP* elevador = al_load_bitmap("assets/sprites/elevador_grua.png");
    ALLEGRO_BITMAP* peso1 = al_load_bitmap("assets/sprites/peso_grua_1.png");
    ALLEGRO_BITMAP* peso2 = al_load_bitmap("assets/sprites/peso_grua_2.png");
    ALLEGRO_BITMAP* peso3 = al_load_bitmap("assets/sprites/peso_grua_3.png");
    ALLEGRO_BITMAP* peso5 = al_load_bitmap("assets/sprites/peso_grua_5.png");
    ALLEGRO_BITMAP* peso10 = al_load_bitmap("assets/sprites/peso_grua_10.png");
    ALLEGRO_BITMAP* carga5 = al_load_bitmap("assets/sprites/carga5.png");
    ALLEGRO_BITMAP* carga10 = al_load_bitmap("assets/sprites/carga10.png");
    ALLEGRO_BITMAP* carga20 = al_load_bitmap("assets/sprites/carga20.png");
    ALLEGRO_BITMAP* carga30 = al_load_bitmap("assets/sprites/carga30.png");

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
            if (isFullWindowed && evento.keyboard.keycode == ALLEGRO_KEY_F11) { // minimiza
                al_toggle_display_flag(display, ALLEGRO_FULLSCREEN_WINDOW, false);
                al_toggle_display_flag(display, ALLEGRO_WINDOWED, true);
                isFullWindowed = false;
            }

            else if (isFullWindowed == false && evento.keyboard.keycode == ALLEGRO_KEY_F11) { // maximiza
                al_toggle_display_flag(display, ALLEGRO_FULLSCREEN_WINDOW, true);
                al_toggle_display_flag(display, ALLEGRO_WINDOWED, false);
                isFullWindowed = true;
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
            al_draw_scaled_bitmap(elevador, 0, 0, 250, 250, 962, 371, 625, 625, 0);
            al_draw_scaled_bitmap(peso10, 0, 0, 32, 32, 530, 343, 80, 80, 0);
            al_draw_scaled_bitmap(carga10, 0, 0, 32, 32, 1222, 868, 80, 80, 0);


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