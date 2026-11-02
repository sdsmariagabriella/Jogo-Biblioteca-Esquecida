#include <stdio.h>
#include <stdbool.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h> // PASSO 1: Incluir a biblioteca de fontes do Allegro

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

    // PASSO 2: Inicializar o addon de fontes para poder escrever na tela
    if(!al_init_font_addon()){
        printf("Erro ao iniciar o addon de fontes!\n");
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
    al_set_window_title(janela, "Jogo de teste - Area de Interacao");

    // PASSO 3: Criar uma fonte padrão do Allegro (não precisa de arquivo externo .ttf)
    ALLEGRO_FONT *fonte = al_create_builtin_font();
    if (!fonte) {
        printf("Erro ao criar a fonte!\n");
        al_destroy_display(janela);
        return 1;
    }

    // Criar um Timer para rodar a 60 FPS
    ALLEGRO_TIMER *timer = al_create_timer(1.0 / 60.0);
    if (!timer) {
        printf("Erro ao criar o timer!\n");
        al_destroy_font(fonte);
        al_destroy_display(janela);
        return 1;
    }

    // Criar uma fila de eventos
    ALLEGRO_EVENT_QUEUE *fila = al_create_event_queue();

    // Registrar fontes de eventos na fila
    al_register_event_source(fila, al_get_keyboard_event_source());
    al_register_event_source(fila, al_get_display_event_source(janela));
    al_register_event_source(fila, al_get_timer_event_source(timer));

    // Posição inicial do objeto (personagem)
    float x = 100;
    float y = 275;

    // Tamanho do objeto
    float tamanho = 50;

    // Velocidade do objeto (personagem)
    float velo = 5;
    bool rodando = true;

    // PASSO 4: Definir a "Zona de Interação" no lado direito central da tela
    // Tela tem 800x600. Lado direito central: x = 700, y = 250, com 80 de largura e altura.
    float zona_x = 700;
    float zona_y = 250;
    float zona_largura = 80;
    float zona_altura = 80;

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
                if (x < 0) x = 0;
            }
            // Movimento para a DIREITA com limite
            if (al_key_down(&key_state, ALLEGRO_KEY_RIGHT)){
                x += velo;
                if (x + tamanho > largura_tela) x = largura_tela - tamanho;
            }
            // Movimento para CIMA com limite
            if (al_key_down(&key_state, ALLEGRO_KEY_UP)){
                y -= velo;
                if (y < 0) y = 0;
            }
            // Movimento para BAIXO com limite
            if (al_key_down(&key_state, ALLEGRO_KEY_DOWN)){
                y += velo;
                if (y + tamanho > altura_tela) y = altura_tela - tamanho;
            }

            // PASSO 5: Verificar colisão entre o quadrado e a zona de interação (AABB Collision)
            bool esta_na_zona = (x < zona_x + zona_largura &&
                                 x + tamanho > zona_x &&
                                 y < zona_y + zona_altura &&
                                 y + tamanho > zona_y);

            // Limpar a tela (fundo preto)
            al_clear_to_color(al_map_rgb(0, 0, 0));

            // PASSO 6: Desenhar a Zona de Interação (um retângulo tracejado/vazio em azul para o jogador ver onde é)
            al_draw_rectangle(zona_x, zona_y, zona_x + zona_largura, zona_y + zona_altura, al_map_rgb(0, 0, 255), 2);

            // Desenhar o personagem (quadrado amarelo)
            al_draw_filled_rectangle(
                x, y,
                x + tamanho, y + tamanho,
                al_map_rgb(200, 200, 0)
            );

            // PASSO 7: Se o jogador estiver na zona, desenhar o texto na parte superior da tela
            if (esta_na_zona) {
                al_draw_text(
                    fonte, 
                    al_map_rgb(255, 255, 255), // Cor branca
                    largura_tela / 2,          // Centralizado horizontalmente
                    50,                        // Posição Y (perto do topo)
                    ALLEGRO_ALIGN_CENTRE,      // Alinhamento central
                    "Pressione [E] para interagir com o bau!" // Mensagem
                );
            }

            // Atualizar a tela
            al_flip_display();
        }
    }

    // PASSO 8: Liberar a memória da fonte e demais elementos ao fechar o jogo
    al_destroy_font(fonte);
    al_destroy_event_queue(fila);
    al_destroy_timer(timer);
    al_destroy_display(janela);

    return 0;
}