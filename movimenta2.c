#include <stdio.h>
#include <stdbool.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>

int main(void){
    // Inicializar o Allegro
    if(!al_init()){
        printf("Erro ao iniciar o Allegro!\n");
        return 1;
    }
    // Inicializar o teclado 
    if(!al_install_keyboard()){
        printf("Erro no teclado!\n");
        return 1;
    }
    // Iniciar as primitivas (formas geometricas)
    if(!al_init_primitives_addon()){
        printf("Erro nas primitivas!\n");
        return 1;
    }

    // Criar uma janela de 800 * 600 pixels
    int largura_tela = 800;
    int altura_tela = 600;
    ALLEGRO_DISPLAY *janela = al_create_display(largura_tela, altura_tela);
    if (!janela){
        printf("Erro na janela!\n");
        return 1;
    }

    // Definir o título da janela 
    al_set_window_title(janela, "Jogo de teste");

    // Criar um Timer para rodar a 60 FPS
    ALLEGRO_TIMER *timer = al_create_timer(1.0 / 60.0);
    if (!timer) {
        printf("Erro ao criar o timer!\n");
        al_destroy_display(janela);
        return 1;
    }

    // Criar uma fila de eventos
    ALLEGRO_EVENT_QUEUE *fila = al_create_event_queue();

    // Registrar fontes de eventos na fila
    al_register_event_source(fila, al_get_keyboard_event_source());
    al_register_event_source(fila, al_get_display_event_source(janela));
    al_register_event_source(fila, al_get_timer_event_source(timer));

    // Posição do objeto (personagem)
    float x = 375;
    float y = 275;

    // Tamanho do objeto
    float tamanho = 50;

    // Velocidade do objeto (personagem)
    float velo = 5;
    bool rodando = true;

    // Iniciar o timer
    al_start_timer(timer);

    while(rodando){
        ALLEGRO_EVENT evento;
        al_wait_for_event(fila, &evento);

        // Se apertar o botão de fechar a janela ou ESC
        if (evento.type == ALLEGRO_EVENT_DISPLAY_CLOSE){
            rodando = false;
        }
        else if (evento.type == ALLEGRO_EVENT_KEY_DOWN){
            if (evento.keyboard.keycode == ALLEGRO_KEY_ESCAPE){
                rodando = false;
            }
        }

        // A cada "tick" do timer (60 vezes por segundo)
        if (evento.type == ALLEGRO_EVENT_TIMER) {
            ALLEGRO_KEYBOARD_STATE key_state;
            al_get_keyboard_state(&key_state);

            // Movimento para a ESQUERDA com limite
            if (al_key_down(&key_state, ALLEGRO_KEY_LEFT)){
                x -= velo;
                if (x < 0) {
                    x = 0; // Trava na borda esquerda
                }
            }
            // Movimento para a DIREITA com limite
            if (al_key_down(&key_state, ALLEGRO_KEY_RIGHT)){
                x += velo;
                if (x + tamanho > largura_tela) {
                    x = largura_tela - tamanho; // Trava na borda direita
                }
            }
            // Limpar a tela
            al_clear_to_color(al_map_rgb(0, 0, 0));

            // Desenhar no quadro
            al_draw_filled_rectangle(
                x, y,
                x + tamanho, y + tamanho,
                al_map_rgb(200, 200, 0)
            );

            // Atualizar a tela
            al_flip_display();
        }
    }

    // Liberar a memória
    al_destroy_event_queue(fila);
    al_destroy_timer(timer);
    al_destroy_display(janela);

    return 0;
}