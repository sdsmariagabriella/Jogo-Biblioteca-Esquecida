#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_image.h>

typedef enum {
    ESTADO_MENU,
    ESTADO_HISTORIA,
    ESTADO_JOGANDO,
    game_over
} EstadoJogo;

// ---------------------------------------------------------------
// SISTEMA DE PROPORCAO
//
// Todas as medidas do jogo foram pensadas numa tela base de 800x600.
// As macros abaixo convertem esses valores para o tamanho real da tela:
//
//   PX(v)  -> escala horizontal (usa a largura da tela)
//   PY(v)  -> escala vertical   (usa a altura da tela)
//   ES(v)  -> escala uniforme   (menor entre as duas, mantem formas
//                                quadradas/redondas sem distorcer)
//
// Para mudar o tamanho da janela, altere apenas largura_tela e
// altura_tela dentro da main.
// ---------------------------------------------------------------

#define BASE_LARGURA 800.0f
#define BASE_ALTURA  600.0f

#define PX(v) ((v) * esc_x)
#define PY(v) ((v) * esc_y)
#define ES(v) ((v) * esc)

// Desenha texto escalado (a fonte padrao tem tamanho fixo de 8 pixels,
// entao usamos uma transformacao para ela acompanhar o tamanho da tela)
static void desenha_texto(
    ALLEGRO_FONT *fonte,
    ALLEGRO_COLOR cor,
    float x, float y,
    int flags,
    float escala,
    const char *texto
)
{
    ALLEGRO_TRANSFORM antiga = *al_get_current_transform();
    ALLEGRO_TRANSFORM nova;

    al_identity_transform(&nova);
    al_scale_transform(&nova, escala, escala);
    al_translate_transform(&nova, x, y);
    al_use_transform(&nova);

    al_draw_text(fonte, cor, 0, 0, flags, texto);

    al_use_transform(&antiga);
}

// Atalho para desenhar texto usando a fonte e a escala da main
#define TEXTO(cor, x, y, flags, str) \
    desenha_texto(fonte, (cor), (x), (y), (flags), esc, (str))

// Desenha um coracao pequeno (vidas no canto da tela)
static void desenha_coracao(float x, float y, float e, ALLEGRO_COLOR cor)
{
    // Parte de cima do coracao
    al_draw_filled_circle(x + 6 * e, y + 6 * e, 6 * e, cor);
    al_draw_filled_circle(x + 15 * e, y + 6 * e, 6 * e, cor);

    // Parte de baixo do coracao
    al_draw_filled_triangle(
        x, y + 8 * e,
        x + 21 * e, y + 8 * e,
        x + 10.5f * e, y + 21 * e,
        cor
    );
}

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

    // ---------------- CONFIGURACOES DA JANELA ----------------
    // Para mudar a resolucao, altere somente estes dois valores

    const int largura_tela = 1280;
    const int altura_tela = 720;

    // Fatores de escala em relacao a tela base (800x600)
    const float esc_x = largura_tela / BASE_LARGURA;
    const float esc_y = altura_tela / BASE_ALTURA;
    const float esc = (esc_x < esc_y) ? esc_x : esc_y;

    ALLEGRO_DISPLAY *janela =
        al_create_display(largura_tela, altura_tela);

    if (!janela) {
        printf("Erro ao criar a janela!\n");
        return 1;
    }

    al_set_window_title(janela, "Biblioteca Esquecida");

    // Carrega a imagem da sala 1
    ALLEGRO_BITMAP *imagem_sala1 =
        al_load_bitmap("img/bibliotecaDestruida2.png");

    if (!imagem_sala1) {
        printf("Erro ao carregar a imagem da sala 1!\n");
        al_destroy_display(janela);
        return 1;
    }

    // Carrega a imagem da sala 2
    ALLEGRO_BITMAP *imagem_sala2 =
        al_load_bitmap("img/bibliotecaDestruida1.png");

    if (!imagem_sala2) {
        printf("Erro ao carregar a imagem da sala 2!\n");
        al_destroy_bitmap(imagem_sala1);
        al_destroy_display(janela);
        return 1;
    }

    // carregar imagem game over 
    ALLEGRO_BITMAP *imagem_game_over =
        al_load_bitmap("img/bibliotecaRuinas.png");
    if(!imagem_game_over){
        printf("Erro ao carregar a imagem de game over!\n");
        return 1;
    }


    // Cria a fonte
    ALLEGRO_FONT *fonte = al_create_builtin_font();

    if (!fonte) {
        printf("Erro ao criar a fonte!\n");
        al_destroy_bitmap(imagem_sala2);
        al_destroy_bitmap(imagem_sala1);
        al_destroy_display(janela);
        return 1;
    }

    // Cria o timer
    ALLEGRO_TIMER *timer = al_create_timer(1.0 / 60.0);

    if (!timer) {
        printf("Erro ao criar o timer!\n");
        al_destroy_font(fonte);
        al_destroy_bitmap(imagem_sala2);
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
        al_destroy_bitmap(imagem_sala2);
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

    // Posicao do botao iniciar
    float botao_x1 = PX(280);
    float botao_y1 = PY(420);
    float botao_x2 = PX(520);
    float botao_y2 = PY(475);

    // ---------------- PERSONAGEM ----------------

    float tamanho = ES(50);
    float velo = PX(5);

    // Posicao fixa no eixo y (o personagem so se move no eixo x)
    const float Y_INICIAL = PY(500);

    float x = PX(100);
    float y = Y_INICIAL;

    // ---------------- SALAS ----------------

    int sala_atual = 1;
    float margem_entrada = PX(10);
    int vidas = 3;

    // Zona de interacao alinhada a altura do personagem
    float zona_x = PX(700);
    float zona_y = PY(450);
    float zona_largura = PX(80);
    float zona_altura = PY(80);

    bool esta_na_zona = false;

    // ---------------- QUIZ ----------------

    bool quiz_aberto = false;

    int opcao_selecionada = 0;
    int opcao_respondida = -1;

    const char *alternativas[4] = {
    "A) Um grande terremoto",
    "B) A erupcao do Monte Vesuvio",
    "C) Uma grande inundacao",
    "D) Um incendio na cidade"
    };

    // ---------------- ESTADOS DO JOGO ----------------

    EstadoJogo estado_atual = ESTADO_MENU;
    bool rodando = true;

    al_start_timer(timer);

    //------------------------------------------------

    //---------- animação da historia ------------------
    int caracteres_visiveis = 0;
    int contador_frames = 0;
    const int velocidade_texto = 10; // Quanto menor, mais rapido o texto aparece

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
            if (estado_atual == ESTADO_MENU) {

                if (tecla == ALLEGRO_KEY_ENTER) {
                    estado_atual = ESTADO_HISTORIA;
                    estado_atual = ESTADO_HISTORIA;
                    caracteres_visiveis = 0; //restar o texto
                    contador_frames = 0; //resetar o contador de frames
                }

                if (tecla == ALLEGRO_KEY_ESCAPE) {
                    rodando = false;
                }
            }

            // Na tela da historia
            else if (estado_atual == ESTADO_HISTORIA) {

                if (tecla == ALLEGRO_KEY_ENTER || tecla == ALLEGRO_KEY_SPACE) {
                    estado_atual = ESTADO_JOGANDO;
                }

                if (tecla == ALLEGRO_KEY_ESCAPE) {
                    rodando = false;
                }
            }

            // Tela de game over
            else if (estado_atual == game_over) {

                if (tecla == ALLEGRO_KEY_ENTER) {
                    vidas = 3;
                    sala_atual = 1;
                    x = PX(100);
                    y = Y_INICIAL;

                    quiz_aberto = false;
                    opcao_selecionada = 0;
                    opcao_respondida = -1;
                    estado_atual = ESTADO_MENU;
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

                        if (opcao_respondida == 1) {
                            printf("Resposta correta!\n");
                        } else {
                            vidas--;
                            printf("Voce perdeu uma vida!\n");
                            if (vidas <= 0) {
                                quiz_aberto = false;
                                estado_atual = game_over;
                            }
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

        else if (evento.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN) {

            if (estado_atual == ESTADO_MENU) {
                // Verifica se clicou no botao iniciar
                if (
                    evento.mouse.button == 1 &&
                    evento.mouse.x >= botao_x1 &&
                    evento.mouse.x <= botao_x2 &&
                    evento.mouse.y >= botao_y1 &&
                    evento.mouse.y <= botao_y2
                ) {
                    estado_atual = ESTADO_HISTORIA;
                }
            }

            else if (estado_atual == ESTADO_HISTORIA) {
                if (evento.mouse.button == 1) {
                    estado_atual = ESTADO_JOGANDO;
                }
            }
        }

        // ---------------- ATUALIZACAO DO JOGO ----------------

        if (evento.type == ALLEGRO_EVENT_TIMER) {

            // Tela inicial
            if (estado_atual == ESTADO_MENU) {

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
                TEXTO(
                    al_map_rgb(219, 198, 167),
                    largura_tela / 2, PY(100),
                    ALLEGRO_ALIGN_CENTRE,
                    "BIBLIOTECA ESQUECIDA"
                );

                // Subtitulo
                TEXTO(
                    al_map_rgb(220, 220, 220),
                    largura_tela / 2, PY(155),
                    ALLEGRO_ALIGN_CENTRE,
                    "Uma aventura entre livros e misterios"
                );

                // Texto de explicacao
                TEXTO(
                    al_map_rgb(255, 255, 255),
                    largura_tela / 2, PY(230),
                    ALLEGRO_ALIGN_CENTRE,
                    "Explore a biblioteca esquecida"
                );

                TEXTO(
                    al_map_rgb(255, 255, 255),
                    largura_tela / 2, PY(260),
                    ALLEGRO_ALIGN_CENTRE,
                    "e descubra os misterios escondidos."
                );

                TEXTO(
                    al_map_rgb(255, 255, 255),
                    largura_tela / 2, PY(310),
                    ALLEGRO_ALIGN_CENTRE,
                    "Movimente-se com as SETAS do teclado."
                );

                TEXTO(
                    al_map_rgb(255, 255, 255),
                    largura_tela / 2, PY(340),
                    ALLEGRO_ALIGN_CENTRE,
                    "Pressione E para interagir com objetos."
                );

                TEXTO(
                    al_map_rgb(255, 255, 255),
                    largura_tela / 2, PY(370),
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
                    ES(3)
                );

                // Texto centralizado verticalmente no botao
                TEXTO(
                    al_map_rgb(255, 255, 255),
                    largura_tela / 2,
                    botao_y1 + ((botao_y2 - botao_y1) - ES(8)) / 2,
                    ALLEGRO_ALIGN_CENTRE,
                    "INICIAR JOGO"
                );

                TEXTO(
                    al_map_rgb(200, 200, 200),
                    largura_tela / 2, PY(520),
                    ALLEGRO_ALIGN_CENTRE,
                    "Pressione ENTER ou clique para comecar"
                );
            }

            // Tela preta da historia
            else if (estado_atual == ESTADO_HISTORIA) {

                al_clear_to_color(al_map_rgb(0, 0, 0));

                // Usamos 'static' para que o vetor seja criado apenas uma vez na memoria
                static const char *linhas_historia[] = {
                    "A HISTORIA",
                    "",
                    "Apos o falecimento de seus pais, enquanto organizava a casa, uma jovem",
                    "encontrou uma caixa antiga com seu proprio nome gravado, parecendo",
                    "pertencer a um tataravo.",
                    "",

                };
                int total_linhas = 6;

                // Logica para revelar as linhas gradualmente com o tempo
                contador_frames++;
                if (contador_frames >= velocidade_texto) {
                    if (caracteres_visiveis < total_linhas) {
                        caracteres_visiveis++;
                    }
                    contador_frames = 0;
                }

                // Desenha cada linha visivel uma embaixo da outra
                float y_inicial = PY(120);
                float espacamento_linha = PY(30);

                for (int i = 0; i < caracteres_visiveis && i < total_linhas; i++) {
                    TEXTO(
                        al_map_rgb(230, 230, 230),
                        largura_tela / 2, 
                        y_inicial + (i * espacamento_linha),
                        ALLEGRO_ALIGN_CENTRE,
                        linhas_historia[i]
                    );
                }

                // Aviso para continuar no rodape
                TEXTO(
                    al_map_rgb(180, 180, 180),
                    largura_tela / 2, PY(550),
                    ALLEGRO_ALIGN_CENTRE,
                    "Pressione ENTER, ESPACO ou clique para continuar"
                );
            }
            // ---------------- TELA DE GAME OVER ----------------
            else if (estado_atual == game_over) {

                    // Desenha a imagem de fundo
                    al_draw_scaled_bitmap(
                        imagem_game_over,
                        0, 0,
                        al_get_bitmap_width(imagem_game_over),
                        al_get_bitmap_height(imagem_game_over),
                        0, 0,
                        largura_tela,
                        altura_tela,
                        0
                    );

                // Escurece a imagem para destacar os textos
                    al_draw_filled_rectangle(
                        0, 0,
                        largura_tela, altura_tela,
                        al_map_rgba(0, 0, 0, 150)
                    );

                // Coracoes no topo (2 cheios e o ultimo quebrado)
                ALLEGRO_COLOR vermelho = al_map_rgb(230, 50, 80);
                float inicio_x = largura_tela / 2 - ES(54);
                float topo_y = PY(130);

                for (int i = 0; i < 2; i++) {
                    float cx = inicio_x + (i * ES(40));
                    al_draw_filled_circle(cx + ES(8), topo_y + ES(8), ES(8), vermelho);
                    al_draw_filled_circle(cx + ES(20), topo_y + ES(8), ES(8), vermelho);
                    al_draw_filled_triangle(
                        cx + ES(1), topo_y + ES(11),
                        cx + ES(27), topo_y + ES(11),
                        cx + ES(14), topo_y + ES(28),
                        vermelho
                    );
                }

                // Coracao quebrado ao lado
                float qx = inicio_x + (2 * ES(40));
                al_draw_filled_circle(qx + ES(7), topo_y + ES(8), ES(7), vermelho);
                al_draw_filled_triangle(
                    qx + ES(1), topo_y + ES(11),
                    qx + ES(12), topo_y + ES(11),
                    qx + ES(11), topo_y + ES(27),
                    vermelho
                );
                al_draw_filled_circle(qx + ES(21), topo_y + ES(8), ES(7), vermelho);
                al_draw_filled_triangle(
                    qx + ES(16), topo_y + ES(11),
                    qx + ES(27), topo_y + ES(11),
                    qx + ES(17), topo_y + ES(27),
                    vermelho
                );

                // Titulo GAME OVER / VOCE MORREU
                TEXTO(
                    al_map_rgb(255, 210, 40),
                    largura_tela / 2, PY(210),
                    ALLEGRO_ALIGN_CENTRE,
                    "VOCE MORREU!"
                );

                // Instrucao para voltar ao inicio
                TEXTO(
                    al_map_rgb(255, 255, 255),
                    largura_tela / 2, PY(310),
                    ALLEGRO_ALIGN_CENTRE,
                    "VOLTAR AO INICIO?"
                );

                TEXTO(
                    al_map_rgb(200, 200, 200),
                    largura_tela / 2, PY(355),
                    ALLEGRO_ALIGN_CENTRE,
                    "Pressione ENTER para voltar ao inicio"
                );
            }

            // Jogo
            else {

                // Movimento do personagem (somente no eixo x)
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

                    // Garante que o personagem fique sempre na mesma altura
                    y = Y_INICIAL;

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
                        ES(2)
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

                    TEXTO(
                        al_map_rgb(255, 255, 255),
                        largura_tela / 2, PY(50),
                        ALLEGRO_ALIGN_CENTRE,
                        "Pressione [E] para interagir com o bau!"
                    );
                }

                // Mostra a sala atual
                char texto_sala[32];
                snprintf(texto_sala, sizeof(texto_sala), "Sala %d", sala_atual);

                TEXTO(
                    al_map_rgb(255, 255, 255),
                    PX(10), PY(10),
                    0,
                    texto_sala
                );

                // Desenha os coracoes de vida (canto superior direito)
                for (int i = 0; i < vidas; i++) {
                    float hx = PX(700) + (i * ES(25));
                    float hy = PY(15);

                    desenha_coracao(hx, hy, esc, al_map_rgb(255, 0, 0));
                }

                // ---------------- TELA DO QUIZ ----------------

                if (quiz_aberto) {

                    float jx1 = PX(150);
                    float jy1 = PY(100);
                    float jx2 = PX(650);
                    float jy2 = PY(450);

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
                        ES(3)
                    );

                    // Titulo
                    TEXTO(
                        al_map_rgb(255, 220, 80),
                        (jx1 + jx2) / 2, jy1 + PY(20),
                        ALLEGRO_ALIGN_CENTRE,
                        "QUIZ"
                    );

                    // Pergunta - primeira linha
                    TEXTO(
                        al_map_rgb(255, 255, 255),
                        (jx1 + jx2) / 2, jy1 + PY(60),
                        ALLEGRO_ALIGN_CENTRE,
                        "Em uma manha, o sol simplesmente desapareceu"
                    );

                    // Segunda linha
                    TEXTO(
                        al_map_rgb(255, 255, 255),
                        (jx1 + jx2) / 2, jy1 + PY(82),
                        ALLEGRO_ALIGN_CENTRE,
                        "e uma chuva de pedras e cinzas cobriu a cidade,"
                    );

                    // Terceira linha
                    TEXTO(
                        al_map_rgb(255, 255, 255),
                        (jx1 + jx2) / 2, jy1 + PY(104),
                        ALLEGRO_ALIGN_CENTRE,
                        "congelando a vida dos moradores por quase dois mil anos. O que causou isso?"
                    );

                    // Alternativas
                    for (int i = 0; i < 4; i++) {

                        float oy = jy1 + PY(150) + i * PY(40);

                        ALLEGRO_COLOR cor_texto =
                            al_map_rgb(200, 200, 200);

                        if (i == opcao_selecionada) {

                            al_draw_filled_rectangle(
                                jx1 + PX(30), oy - PY(8),
                                jx2 - PX(30), oy + PY(22),
                                al_map_rgb(70, 70, 120)
                            );

                            cor_texto =
                                al_map_rgb(255, 255, 255);
                        }

                        TEXTO(
                            cor_texto,
                            jx1 + PX(50), oy,
                            0,
                            alternativas[i]
                        );
                    }

                    // Resposta escolhida
                    if (opcao_respondida >= 0) {

                        char texto_resposta[128];
                        snprintf(
                            texto_resposta,
                            sizeof(texto_resposta),
                            "Voce respondeu: %s",
                            alternativas[opcao_respondida]
                        );

                        TEXTO(
                            al_map_rgb(120, 255, 120),
                            (jx1 + jx2) / 2, jy2 - PY(70),
                            ALLEGRO_ALIGN_CENTRE,
                            texto_resposta
                        );
                    }

                    // Instrucoes
                    TEXTO(
                        al_map_rgb(180, 180, 180),
                        (jx1 + jx2) / 2, jy2 - PY(35),
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
    al_destroy_bitmap(imagem_game_over);
    al_destroy_event_queue(fila);
    al_destroy_timer(timer);
    al_destroy_display(janela);

    return 0;
}