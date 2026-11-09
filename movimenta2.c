#include <stdio.h>
#include <stdbool.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>

int main(void){
    if(!al_init()){
        printf("Erro ao iniciar o Allegro!\n");
        return 1;
    }
    if(!al_install_keyboard()){
        printf("Erro no teclado!\n");
        return 1;
    }
    if(!al_init_primitives_addon()){
        printf("Erro nas primitivas!\n");
        return 1;
    }
    if(!al_init_font_addon()){
        printf("Erro ao iniciar o addon de fontes!\n");
        return 1;
    }

    int largura_tela = 800;
    int altura_tela = 600;
    ALLEGRO_DISPLAY *janela = al_create_display(largura_tela, altura_tela);
    if (!janela){
        printf("Erro na janela!\n");
        return 1;
    }
    al_set_window_title(janela, "Jogo de teste - Salas e Quiz");

    ALLEGRO_FONT *fonte = al_create_builtin_font();
    if (!fonte) {
        printf("Erro ao criar a fonte!\n");
        al_destroy_display(janela);
        return 1;
    }

    ALLEGRO_TIMER *timer = al_create_timer(1.0 / 60.0);
    if (!timer) {
        printf("Erro ao criar o timer!\n");
        al_destroy_font(fonte);
        al_destroy_display(janela);
        return 1;
    }

    ALLEGRO_EVENT_QUEUE *fila = al_create_event_queue();
    al_register_event_source(fila, al_get_keyboard_event_source());
    al_register_event_source(fila, al_get_display_event_source(janela));
    al_register_event_source(fila, al_get_timer_event_source(timer));

    float x = 100;
    float y = 275;
    float tamanho = 50;
    float velo = 5;
    bool rodando = true;

    int sala_atual = 1;
    float margem_entrada = 10;

    // Zona de interação (existe só na sala 1)
    float zona_x = 700;
    float zona_y = 250;
    float zona_largura = 80;
    float zona_altura = 80;

    // Estado da zona (atualizado a cada tick, usado no evento de tecla)
    bool esta_na_zona = false;

    // Estado do quiz
    bool quiz_aberto = false;
    int opcao_selecionada = 0;      // 0 a 3
    int opcao_respondida = -1;      // -1 = ainda não respondeu
    const char *alternativas[4] = {
        "A) Alternativa 1",
        "B) Alternativa 2",
        "C) Alternativa 3",
        "D) Alternativa 4"
    };

    al_start_timer(timer);

    while(rodando){
        ALLEGRO_EVENT evento;
        al_wait_for_event(fila, &evento);

        if (evento.type == ALLEGRO_EVENT_DISPLAY_CLOSE){
            rodando = false;
        }
        else if (evento.type == ALLEGRO_EVENT_KEY_DOWN){
            int tecla = evento.keyboard.keycode;

            if (quiz_aberto){
                // Controles dentro do quiz
                if (tecla == ALLEGRO_KEY_UP){
                    opcao_selecionada = (opcao_selecionada + 3) % 4;
                }
                else if (tecla == ALLEGRO_KEY_DOWN){
                    opcao_selecionada = (opcao_selecionada + 1) % 4;
                }
                else if (tecla == ALLEGRO_KEY_ENTER){
                    opcao_respondida = opcao_selecionada;
                }
                else if (tecla == ALLEGRO_KEY_E || tecla == ALLEGRO_KEY_ESCAPE){
                    quiz_aberto = false;
                }
            }
            else {
                if (tecla == ALLEGRO_KEY_ESCAPE){
                    rodando = false;
                }
                else if (tecla == ALLEGRO_KEY_E && esta_na_zona){
                    // Abre o quiz e reinicia o estado dele
                    quiz_aberto = true;
                    opcao_selecionada = 0;
                    opcao_respondida = -1;
                }
            }
        }

        if (evento.type == ALLEGRO_EVENT_TIMER) {
            // Só move o personagem se o quiz estiver fechado
            if (!quiz_aberto){
                ALLEGRO_KEYBOARD_STATE key_state;
                al_get_keyboard_state(&key_state);

                if (al_key_down(&key_state, ALLEGRO_KEY_LEFT)){
                    x -= velo;
                    if (x < 0) x = 0;
                }
                if (al_key_down(&key_state, ALLEGRO_KEY_RIGHT)){
                    x += velo;
                    if (x + tamanho > largura_tela) x = largura_tela - tamanho;
                }
                if (al_key_down(&key_state, ALLEGRO_KEY_UP)){
                    y -= velo;
                    if (y < 0) y = 0;
                }
                if (al_key_down(&key_state, ALLEGRO_KEY_DOWN)){
                    y += velo;
                    if (y + tamanho > altura_tela) y = altura_tela - tamanho;
                }

                // Troca de sala
                if (sala_atual == 1 && x + tamanho >= largura_tela){
                    sala_atual = 2;
                    x = margem_entrada;
                }
                else if (sala_atual == 2 && x <= 0){
                    sala_atual = 1;
                    x = largura_tela - tamanho - margem_entrada;
                }
            }

            // Colisão com a zona (só na sala 1)
            esta_na_zona = (sala_atual == 1 &&
                            x < zona_x + zona_largura &&
                            x + tamanho > zona_x &&
                            y < zona_y + zona_altura &&
                            y + tamanho > zona_y);

            // Fundo
            if (sala_atual == 1){
                al_clear_to_color(al_map_rgb(0, 0, 0));
            } else {
                al_clear_to_color(al_map_rgb(20, 60, 40));
            }

            if (sala_atual == 1){
                al_draw_rectangle(zona_x, zona_y, zona_x + zona_largura, zona_y + zona_altura,
                                  al_map_rgb(0, 0, 255), 2);
            }

            al_draw_filled_rectangle(x, y, x + tamanho, y + tamanho, al_map_rgb(200, 200, 0));

            if (esta_na_zona && !quiz_aberto) {
                al_draw_text(fonte, al_map_rgb(255, 255, 255),
                             largura_tela / 2, 50, ALLEGRO_ALIGN_CENTRE,
                             "Pressione [E] para interagir com o bau!");
            }

            al_draw_textf(fonte, al_map_rgb(255, 255, 255), 10, 10, 0, "Sala %d", sala_atual);

            // ---- Janelinha do quiz (desenhada por cima de tudo) ----
            if (quiz_aberto){
                float jx1 = 150, jy1 = 100, jx2 = 650, jy2 = 450;

                // Escurece o fundo para dar destaque ao painel
                al_draw_filled_rectangle(0, 0, largura_tela, altura_tela, al_map_rgba(0, 0, 0, 150));

                // Painel e borda
                al_draw_filled_rectangle(jx1, jy1, jx2, jy2, al_map_rgb(40, 40, 60));
                al_draw_rectangle(jx1, jy1, jx2, jy2, al_map_rgb(255, 255, 255), 3);

                // Título e pergunta
                al_draw_text(fonte, al_map_rgb(255, 220, 80), (jx1 + jx2) / 2, jy1 + 20,
                             ALLEGRO_ALIGN_CENTRE, "QUIZ");
                al_draw_text(fonte, al_map_rgb(255, 255, 255), (jx1 + jx2) / 2, jy1 + 60,
                             ALLEGRO_ALIGN_CENTRE, "Pergunta de teste?");

                // Alternativas
                for (int i = 0; i < 4; i++){
                    float oy = jy1 + 110 + i * 40;
                    ALLEGRO_COLOR cor_texto = al_map_rgb(200, 200, 200);

                    if (i == opcao_selecionada){
                        // Destaque da alternativa selecionada
                        al_draw_filled_rectangle(jx1 + 30, oy - 8, jx2 - 30, oy + 22,
                                                 al_map_rgb(70, 70, 120));
                        cor_texto = al_map_rgb(255, 255, 255);
                    }
                    al_draw_text(fonte, cor_texto, jx1 + 50, oy, 0, alternativas[i]);
                }

                // Feedback após responder
                if (opcao_respondida >= 0){
                    al_draw_textf(fonte, al_map_rgb(120, 255, 120), (jx1 + jx2) / 2, jy2 - 70,
                                  ALLEGRO_ALIGN_CENTRE, "Voce respondeu: %s",
                                  alternativas[opcao_respondida]);
                }

                // Instruções
                al_draw_text(fonte, al_map_rgb(180, 180, 180), (jx1 + jx2) / 2, jy2 - 35,
                             ALLEGRO_ALIGN_CENTRE,
                             "[CIMA/BAIXO] escolher   [ENTER] responder   [E/ESC] fechar");
            }

            al_flip_display();
        }
    }

    al_destroy_font(fonte);
    al_destroy_event_queue(fila);
    al_destroy_timer(timer);
    al_destroy_display(janela);

    return 0;
}