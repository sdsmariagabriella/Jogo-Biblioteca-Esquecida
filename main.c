
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_image.h>

int main(void)
{
    // Inicializa o Allegro
    if (!al_init()) {
        printf("Erro ao iniciar o Allegro!\n");
        return 1;
    }

    // Inicializa teclado e mouse
    if (!al_install_keyboard()) {
        printf("Erro no teclado!\n");
        return 1;
    }

    if (!al_install_mouse()) {
        printf("Erro no mouse!\n");
        return 1;
    }

    // Inicializa os addons
    if (!al_init_primitives_addon()) {
        printf("Erro nas primitivas!\n");
        return 1;
    }

    if (!al_init_image_addon()) {
        printf("Erro nas imagens!\n");
        return 1;
    }

    // O addon de fontes nao retorna valor
    al_init_font_addon();

    // Configuracoes da janela
    int largura_tela = 800;
    int altura_tela = 600;

    ALLEGRO_DISPLAY *janela =
        al_create_display(largura_tela, altura_tela);

    if (!janela) {
        printf("Erro ao criar a janela!\n");
        return 1;
    }

    al_set_window_title(janela, "Biblioteca Esquecida");

    // Carrega a imagem da sala 1
    ALLEGRO_BITMAP *imagem_sala1 =
        al_load_bitmap("img/abandonada.png");

    if (!imagem_sala1) {
        printf("Erro ao carregar a imagem da sala 1!\n");
        al_destroy_display(janela);
        return 1;
    }

    //sala 2
    ALLEGRO_BITMAP * imagem_sala2 =
        al_load_bitmap("img/abandonada2.png");

    if (!imagem_sala2){
        printf("Erro ao carregar a iamgem da sala 2\n ");
        al_destroy_bitmap(imagem_sala1);
        al_destroy_display(janela);
        return 1;
    }


    // Cria a fonte
    ALLEGRO_FONT *fonte = al_create_builtin_font();

    if (!fonte) {
        printf("Erro ao criar a fonte!\n");
        al_destroy_bitmap(imagem_sala1);
        al_destroy_display(janela);
        return 1;
    }

    // Cria o timer
    ALLEGRO_TIMER *timer = al_create_timer(1.0 / 60.0);

    if (!timer) {
        printf("Erro ao criar o timer!\n");
        al_destroy_font(fonte);
        al_destroy_bitmap(imagem_sala1);
        al_destroy_display(janela);
        return 1;
    }

    // Cria a fila de eventos
    ALLEGRO_EVENT_QUEUE *fila = al_create_event_queue();

    if (!fila) {
        printf("Erro ao criar a fila de eventos!\n");
        al_destroy_timer(timer);
        al_destroy_font(fonte);
        al_destroy_bitmap(imagem_sala1);
        al_destroy_display(janela);
        return 1;
    }

    // Registra os eventos
    al_register_event_source(
        fila, al_get_keyboard_event_source()
    );

    al_register_event_source(
        fila, al_get_mouse_event_source()
    );

    al_register_event_source(
        fila, al_get_display_event_source(janela)
    );

    al_register_event_source(
        fila, al_get_timer_event_source(timer)
    );

    // ---------------- TELA INICIAL ----------------

    bool jogo_iniciado = false;
    bool rodando = true;

    // Posicao do botao iniciar
    float botao_x1 = 280;
    float botao_y1 = 420;
    float botao_x2 = 520;
    float botao_y2 = 475;

    // ---------------- PERSONAGEM ----------------

    float x = 100;
    float y = 275;
    float tamanho = 50;
    float velo = 5;

    // ---------------- SALAS ----------------

    int sala_atual = 1;
    float margem_entrada = 10;
    int vidas = 3;

    // Zona de interacao
    float zona_x = 700;
    float zona_y = 250;
    float zona_largura = 80;
    float zona_altura = 80;

    bool esta_na_zona = false;

    // ---------------- QUIZ ----------------

    bool quiz_aberto = false;
    char resposta[100];

    int opcao_selecionada = 0;
    int opcao_respondida = -1;

    const char *alternativas[4] = {
        "A) Cavalo de Troia",
        "B) Guerra dos Cem Anos",
        "C) Muralha de Tróia",
        "D) Mitologia Romana"
    };

    al_start_timer(timer);

    // ---------------- LOOP PRINCIPAL ----------------

    while (rodando) {

        ALLEGRO_EVENT evento;
        al_wait_for_event(fila, &evento);

        // Fechar a janela
        if (evento.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
            rodando = false;
        }

        // ---------------- EVENTOS DO TECLADO ----------------

        else if (evento.type == ALLEGRO_EVENT_KEY_DOWN) {

            int tecla = evento.keyboard.keycode;

            // Na tela inicial
            if (!jogo_iniciado) {

                if (tecla == ALLEGRO_KEY_ENTER) {
                    jogo_iniciado = true;
                }

                if (tecla == ALLEGRO_KEY_ESCAPE) {
                    rodando = false;
                }
            }

            // Durante o jogo
            else {

                if (quiz_aberto) {

                    if (tecla == ALLEGRO_KEY_UP) {
                        opcao_selecionada =
                            (opcao_selecionada + 3) % 4;
                    }

                    else if (tecla == ALLEGRO_KEY_DOWN) {
                        opcao_selecionada =
                            (opcao_selecionada + 1) % 4;
                    }

                    else if (tecla == ALLEGRO_KEY_ENTER && opcao_respondida == -1) {
                        opcao_respondida = opcao_selecionada;

                        if (opcao_respondida == 0){
                        printf("Resposta correta!\n");
                        }else{
                            vidas--;
                            printf("Você perdeu uma vida!");
                        }
                    }

                    else if (
                        tecla == ALLEGRO_KEY_E ||
                        tecla == ALLEGRO_KEY_ESCAPE
                    ) {
                        quiz_aberto = false;
                    }
                }

                else {

                    if (tecla == ALLEGRO_KEY_ESCAPE) {
                        rodando = false;
                    }

                    else if (
                        tecla == ALLEGRO_KEY_E &&
                        esta_na_zona
                    ) {
                        quiz_aberto = true;
                        opcao_selecionada = 0;
                        opcao_respondida = -1;
                    }
                }
            }
        }

        // ---------------- EVENTOS DO MOUSE ----------------

        else if (
            evento.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN &&
            !jogo_iniciado
        ) {

            // Verifica se clicou no botao iniciar
            if (
                evento.mouse.button == 1 &&
                evento.mouse.x >= botao_x1 &&
                evento.mouse.x <= botao_x2 &&
                evento.mouse.y >= botao_y1 &&
                evento.mouse.y <= botao_y2
            ) {
                jogo_iniciado = true;
            }
        }

        // ---------------- ATUALIZACAO DO JOGO ----------------

        if (evento.type == ALLEGRO_EVENT_TIMER) {

            // Se ainda nao iniciou, desenha a tela inicial
            if (!jogo_iniciado) {

                // Fundo da tela inicial
                al_draw_scaled_bitmap(
                    imagem_sala1,
                    0, 0,
                    al_get_bitmap_width(imagem_sala1),
                    al_get_bitmap_height(imagem_sala1),
                    0, 0,
                    largura_tela,
                    altura_tela,
                    0
                );

                // Escurece o fundo
                al_draw_filled_rectangle(
                    0, 0,
                    largura_tela, altura_tela,
                    al_map_rgba(0, 0, 0, 190)
                );

                // Titulo
                al_draw_text(
                    fonte,
                    al_map_rgb(219, 198, 167),
                    largura_tela / 2,
                    100,
                    ALLEGRO_ALIGN_CENTRE,
                    "BIBLIOTECA ESQUECIDA"
                );

                // Subtitulo
                al_draw_text(
                    fonte,
                    al_map_rgb(220, 220, 220),
                    largura_tela / 2,
                    155,
                    ALLEGRO_ALIGN_CENTRE,
                    "Uma aventura entre livros e misterios"
                );

                // Texto de explicacao
                al_draw_text(
                    fonte,
                    al_map_rgb(255, 255, 255),
                    largura_tela / 2,
                    230,
                    ALLEGRO_ALIGN_CENTRE,
                    "Explore a biblioteca esquecida"
                );

                al_draw_text(
                    fonte,
                    al_map_rgb(255, 255, 255),
                    largura_tela / 2,
                    260,
                    ALLEGRO_ALIGN_CENTRE,
                    "e descubra os misterios escondidos."
                );

                al_draw_text(
                    fonte,
                    al_map_rgb(255, 255, 255),
                    largura_tela / 2,
                    310,
                    ALLEGRO_ALIGN_CENTRE,
                    "Movimente-se com as SETAS do teclado."
                );

                al_draw_text(
                    fonte,
                    al_map_rgb(255, 255, 255),
                    largura_tela / 2,
                    340,
                    ALLEGRO_ALIGN_CENTRE,
                    "Pressione E para interagir com objetos."
                );

                al_draw_text(
                    fonte,
                    al_map_rgb(255, 255, 255),
                    largura_tela / 2,
                    370,
                    ALLEGRO_ALIGN_CENTRE,
                    "Responda ao quiz usando as setas e ENTER."
                );

                // Botao iniciar
                al_draw_filled_rectangle(
                    botao_x1, botao_y1,
                    botao_x2, botao_y2,
                    al_map_rgb(100, 65, 35)
                );

                al_draw_rectangle(
                    botao_x1, botao_y1,
                    botao_x2, botao_y2,
                    al_map_rgb(255, 220, 150),
                    3
                );

                al_draw_text(
                    fonte,
                    al_map_rgb(255, 255, 255),
                    largura_tela / 2,
                    botao_y1 + 20,
                    ALLEGRO_ALIGN_CENTRE,
                    "INICIAR JOGO"
                );

                al_draw_text(
                    fonte,
                    al_map_rgb(200, 200, 200),
                    largura_tela / 2,
                    520,
                    ALLEGRO_ALIGN_CENTRE,
                    "Pressione ENTER ou clique para comecar"
                );
            }

            // ---------------- JOGO Tela 1----------------

            else {

                // Movimento do personagem
                if (!quiz_aberto) {

                    ALLEGRO_KEYBOARD_STATE key_state;
                    al_get_keyboard_state(&key_state);

                    if (al_key_down(&key_state, ALLEGRO_KEY_LEFT)) {
                        x -= velo;

                        if (x < 0) {
                            x = 0;
                        }
                    }

                    if (al_key_down(&key_state, ALLEGRO_KEY_RIGHT)) {
                        x += velo;

                        if (x + tamanho > largura_tela) {
                            x = largura_tela - tamanho;
                        }
                    }

                    if (al_key_down(&key_state, ALLEGRO_KEY_UP)) {
                        y -= velo;

                        if (y < 0) {
                            y = 0;
                        }
                    }

                    if (al_key_down(&key_state, ALLEGRO_KEY_DOWN)) {
                        y += velo;

                        if (y + tamanho > altura_tela) {
                            y = altura_tela - tamanho;
                        }
                    }

                    // Troca de sala
                    if (
                        sala_atual == 1 &&
                        x + tamanho >= largura_tela
                    ) {
                        sala_atual = 2;
                        x = margem_entrada;
                    }

                    else if (
                        sala_atual == 2 &&
                        x <= 0
                    ) {
                        sala_atual = 1;
                        x = largura_tela - tamanho - margem_entrada;
                    }
                }

                // Verifica colisao com a zona
                esta_na_zona = (
                    sala_atual == 1 &&
                    x < zona_x + zona_largura &&
                    x + tamanho > zona_x &&
                    y < zona_y + zona_altura &&
                    y + tamanho > zona_y
                );

                // Desenha o fundo
                if (sala_atual == 1) {

                    al_draw_scaled_bitmap(
                        imagem_sala1,
                        0, 0,
                        al_get_bitmap_width(imagem_sala1),
                        al_get_bitmap_height(imagem_sala1),
                        0, 0,
                        largura_tela,
                        altura_tela,
                        0
                    );
                }

                else {
                     al_draw_scaled_bitmap(
                        imagem_sala2,
                        0, 0,
                        al_get_bitmap_width(imagem_sala2),
                        al_get_bitmap_height(imagem_sala2),
                        0, 0,
                        largura_tela,
                        altura_tela,
                        0
                        );
                }

                // Desenha a zona de interacao
                if (sala_atual == 1) {

                    al_draw_rectangle(
                        zona_x, zona_y,
                        zona_x + zona_largura,
                        zona_y + zona_altura,
                        al_map_rgb(0, 0, 255),
                        2
                    );
                }

                // Desenha o personagem
                al_draw_filled_rectangle(
                    x, y,
                    x + tamanho,
                    y + tamanho,
                    al_map_rgb(200, 200, 0)
                );

                // Mensagem para interagir
                if (esta_na_zona && !quiz_aberto) {

                    al_draw_text(
                        fonte,
                        al_map_rgb(255, 255, 255),
                        largura_tela / 2,
                        50,
                        ALLEGRO_ALIGN_CENTRE,
                        "Pressione [E] para interagir com o bau!"
                    );
                }

                // Mostra a sala atual
                al_draw_textf(
                    fonte,
                    al_map_rgb(255, 255, 255),
                    10, 10,
                    0,
                    "Sala %d",
                    sala_atual
                );

                for (int i = 0; i < vidas; i++){
                    float x = 700 + (i * 25);
                    float y = 15;

                    ALLEGRO_COLOR vermelho = al_map_rgb(255,0,0);

                    //parte do coração de cima
                    al_draw_filled_circle(x + 6, y + 6 , 6, vermelho );
                    al_draw_filled_circle(x + 15, y + 6, 6, vermelho);

                    //parte de baixo
                    al_draw_filled_triangle(x, y + 8,x + 21, y + 8,x + 10.5, y + 21,vermelho);
                }

                // ---------------- TELA DO QUIZ ----------------

                if (quiz_aberto) {

                    float jx1 = 150;
                    float jy1 = 100;
                    float jx2 = 650;
                    float jy2 = 450;

                    // Escurece o fundo
                    al_draw_filled_rectangle(
                        0, 0,
                        largura_tela, altura_tela,
                        al_map_rgba(0, 0, 0, 150)
                    );

                    // Painel do quiz
                    al_draw_filled_rectangle(
                        jx1, jy1, jx2, jy2,
                        al_map_rgb(40, 40, 60)
                    );

                    al_draw_rectangle(
                        jx1, jy1, jx2, jy2,
                        al_map_rgb(255, 255, 255),
                        3
                    );

                    // Titulo
                    al_draw_text(
                        fonte,
                        al_map_rgb(255, 220, 80),
                        (jx1 + jx2) / 2,
                        jy1 + 20,
                        ALLEGRO_ALIGN_CENTRE,
                        "QUIZ"
                    );

                    // Pergunta
                    al_draw_text(
                        fonte,
                        al_map_rgb(255, 255, 255),
                        (jx1 + jx2) / 2,
                        jy1 + 60,
                        ALLEGRO_ALIGN_CENTRE,
                        "Qual foi a estrategia usada para invadir Troia?"
                    );

                    // Alternativas
                    for (int i = 0; i < 4; i++) {

                        float oy = jy1 + 110 + i * 40;

                        ALLEGRO_COLOR cor_texto =
                            al_map_rgb(200, 200, 200);

                        if (i == opcao_selecionada) {

                            al_draw_filled_rectangle(
                                jx1 + 30, oy - 8,
                                jx2 - 30, oy + 22,
                                al_map_rgb(70, 70, 120)
                            );

                            cor_texto =
                                al_map_rgb(255, 255, 255);
                        }

                        al_draw_text(
                            fonte,
                            cor_texto,
                            jx1 + 50,
                            oy,
                            0,
                            alternativas[i]
                        );
                    }

                    // Resposta escolhida
                    if (opcao_respondida >= 0) {

                        al_draw_textf(
                            fonte,
                            al_map_rgb(120, 255, 120),
                            (jx1 + jx2) / 2,
                            jy2 - 70,
                            ALLEGRO_ALIGN_CENTRE,
                            "Voce respondeu: %s",
                            alternativas[opcao_respondida]
                        );
                    }

                    // Instrucoes
                    al_draw_text(
                        fonte,
                        al_map_rgb(180, 180, 180),
                        (jx1 + jx2) / 2,
                        jy2 - 35,
                        ALLEGRO_ALIGN_CENTRE,
                        "[CIMA/BAIXO] escolher  [ENTER] responder  [E/ESC] fechar"
                    );
                }
            }

            // Atualiza a tela
            al_flip_display();
        }
    }

    // ---------------- LIBERAR MEMORIA ----------------

    al_destroy_font(fonte);
    al_destroy_bitmap(imagem_sala2);
    al_destroy_bitmap(imagem_sala1);
    al_destroy_event_queue(fila);
    al_destroy_timer(timer);
    al_destroy_display(janela);

    return 0;
}
