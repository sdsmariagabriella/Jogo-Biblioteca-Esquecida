#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_image.h>

// Coloque 1 para desenhar a linha da escada e do andar de cima
// (ajuda a ajustar os valores da escada na main)
#define DEBUG_ESCADA 0

typedef enum {
    ESTADO_MENU,
    ESTADO_HISTORIA,
    ESTADO_JOGANDO,
    game_over
} EstadoJogo;

// Dados de um quiz: ate 4 linhas de pergunta, 4 alternativas e a correta
typedef struct {
    const char *pergunta[4];
    int num_linhas;
    const char *alternativas[4];
    int resposta_certa;   // 0 = A, 1 = B, 2 = C, 3 = D
} Quiz;

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
//   FX(f)  -> posicao como fracao da largura (0.0 a 1.0)
//   FY(f)  -> posicao como fracao da altura  (0.0 a 1.0)
//
// FX e FY sao usados para a escada e o andar de cima, porque o fundo
// ocupa a tela inteira: uma fracao da tela e uma fracao da imagem.
//
// Para mudar o tamanho da janela, altere apenas largura_tela e
// altura_tela dentro da main.
// ---------------------------------------------------------------

#define BASE_LARGURA 800.0f
#define BASE_ALTURA  600.0f

#define PX(v) ((v) * esc_x)
#define PY(v) ((v) * esc_y)
#define ES(v) ((v) * esc)
#define FX(f) ((f) * largura_tela)
#define FY(f) ((f) * altura_tela)

// Desenha texto escalado (a fonte padrao tem tamanho fixo de 8 pixels,
// entao usamos uma transformacao para ela acompanhar o tamanho da tela).
// A transformacao atual (ex: tremor de tela) e combinada com a escala,
// assim o texto tambem treme junto com o resto da tela.
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
    al_compose_transform(&nova, &antiga);
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

// Altura dos "pes" do personagem em cada ponto da escada.
// Na base (centro = x1) fica na altura do chao e no topo (centro = x2)
// fica na altura do andar de cima.
static float pes_na_escada(
    float centro,
    float x1, float x2,
    float pes_base, float pes_topo
)
{
    float t = (centro - x1) / (x2 - x1);

    if (t < 0) {
        t = 0;
    }
    if (t > 1) {
        t = 1;
    }

    return pes_base + (pes_topo - pes_base) * t;
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

    // Quadros por segundo do jogo
    const int FPS = 60;

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
    ALLEGRO_TIMER *timer = al_create_timer(1.0 / FPS);

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

    // Posicao do chao no eixo y (o personagem so sai dela na escada)
    const float Y_INICIAL = PY(500);

    float x = PX(100);
    float y = Y_INICIAL;

    // ---------------- ESCADA E ANDAR DE CIMA (SALA 1) ----------------
    // Valores como fracao da tela (0.0 a 1.0). Se a escada nao ficar
    // alinhada com a imagem, ajuste os valores abaixo (use DEBUG_ESCADA).

    const float escada_x1 = FX(0.215);  // base da escada (centro do personagem)
    const float escada_x2 = FX(0.345);  // topo da escada (centro do personagem)
    const float margem_escada = FX(0.03); // distancia para poder entrar na escada

    const float pes_chao = Y_INICIAL + tamanho;  // altura dos pes no chao
    const float pes_cima = FY(0.325);            // altura dos pes no andar de cima
    const float Y_CIMA = pes_cima - tamanho;     // posicao y do quadrado la em cima

    int andar = 0;            // 0 = terreo, 1 = andar de cima
    bool na_escada = false;   // true enquanto esta subindo ou descendo

    // ---------------- SALAS ----------------

    int sala_atual = 1;
    float margem_entrada = PX(10);
    int vidas = 3;

    // Zona de interacao do bau (terreo)
    float zona_x = PX(700);
    float zona_y = PY(450);
    float zona_largura = PX(80);
    float zona_altura = PY(80);

    bool esta_na_zona = false;

    // Zona de interacao do andar de cima
    float zona2_x = FX(0.08);
    float zona2_largura = FX(0.10);
    float zona2_y = Y_CIMA - PY(15);
    float zona2_altura = tamanho + PY(30);

    bool esta_na_zona2 = false;

    // ---------------- QUIZ ----------------

    bool quiz_aberto = false;

    int opcao_selecionada = 0;
    int opcao_respondida = -1;

    // Quiz 0 = bau do terreo, quiz 1 = andar de cima
    int quiz_atual = 0;

    const Quiz quizzes[2] = {
        // Quiz 0: bau
        {
            {
                "Em uma manha, o sol simplesmente desapareceu",
                "e uma chuva de pedras e cinzas cobriu a cidade,",
                "congelando a vida dos moradores por quase dois mil anos. O que causou isso?"
            },
            3,
            {
                "A) Um grande terremoto",
                "B) A erupcao do Monte Vesuvio",
                "C) Uma grande inundacao",
                "D) Um incendio na cidade"
            },
            1
        },

        // Quiz 1: andar de cima (Titanic)
        //
        // Explicacao: O naufragio do RMS Titanic (1912). O navio colidiu com
        // um iceberg no Oceano Atlantico Norte em uma noite sem lua. Por
        // causa da alta velocidade e do tamanho da embarcacao, ele nao
        // conseguiu se desviar a tempo, e a falta de botes salva-vidas
        // suficientes para todos agravou a tragedia.
        {
            {
                "A maior e mais luxuosa embarcacao de sua epoca, considerada",
                "praticamente inquebravel, afundou em sua viagem inaugural,",
                "matando mais de 1.500 pessoas em aguas congelantes, mesmo",
                "apos avisos de perigo na rota. O que aconteceu?"
            },
            4,
            {
                "A) O naufragio do Lusitania (1915)",
                "B) O naufragio do Costa Concordia (2012)",
                "C) O naufragio do RMS Titanic (1912)",
                "D) O naufragio do Andrea Doria (1956)"
            },
            2
        }
    };

    // ---------------- MENSAGEM DE RESPOSTA E TREMOR ----------------

    // Duracao da mensagem "Resposta correta!" / "Resposta errada..." (2 segundos)
    const int duracao_mensagem = 2 * FPS;

    // Duracao do tremor de tela ao errar (meio segundo)
    const int duracao_tremor = FPS / 2;

    int frames_mensagem = 0;       // frames restantes da mensagem (0 = sem mensagem)
    int frames_tremor = 0;         // frames restantes do tremor (0 = sem tremor)
    bool ultima_resposta_correta = false;

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
                    andar = 0;
                    na_escada = false;
                    x = PX(100);
                    y = Y_INICIAL;

                    quiz_aberto = false;
                    opcao_selecionada = 0;
                    opcao_respondida = -1;
                    frames_mensagem = 0;
                    frames_tremor = 0;
                    estado_atual = ESTADO_MENU;
                }

                if (tecla == ALLEGRO_KEY_ESCAPE) {
                    rodando = false;
                }
            }

            // Durante o jogo
            else {

                if (quiz_aberto) {

                    // Enquanto a mensagem de resposta aparece, as teclas sao ignoradas
                    if (frames_mensagem > 0) {
                        // nada acontece
                    }

                    else if (tecla == ALLEGRO_KEY_UP) {
                        opcao_selecionada =
                            (opcao_selecionada + 3) % 4;
                    }

                    else if (tecla == ALLEGRO_KEY_DOWN) {
                        opcao_selecionada =
                            (opcao_selecionada + 1) % 4;
                    }

                    else if (tecla == ALLEGRO_KEY_ENTER && opcao_respondida == -1) {
                        opcao_respondida = opcao_selecionada;

                        // Inicia a mensagem de 2 segundos
                        frames_mensagem = duracao_mensagem;

                        if (opcao_respondida == quizzes[quiz_atual].resposta_certa) {
                            ultima_resposta_correta = true;
                            printf("Resposta correta!\n");
                        } else {
                            ultima_resposta_correta = false;
                            vidas--;
                            frames_tremor = duracao_tremor;
                            printf("Voce perdeu uma vida!\n");
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
                        (esta_na_zona || esta_na_zona2)
                    ) {
                        // Escolhe o quiz de acordo com a zona
                        quiz_atual = esta_na_zona ? 0 : 1;

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

                // Contagem da mensagem de resposta (2 segundos)
                if (frames_mensagem > 0) {
                    frames_mensagem--;

                    // Quando a mensagem termina, o quiz fecha junto
                    if (frames_mensagem == 0) {
                        quiz_aberto = false;

                        // Se acabaram as vidas, vai para o game over
                        if (vidas <= 0) {
                            estado_atual = game_over;
                        }
                    }
                }

                // Calcula o tremor de tela (diminui aos poucos ate parar)
                float tremor_x = 0;
                float tremor_y = 0;

                if (frames_tremor > 0) {
                    float forca = ES(8) * frames_tremor / duracao_tremor;

                    tremor_x = ((rand() % 201) - 100) / 100.0f * forca;
                    tremor_y = ((rand() % 201) - 100) / 100.0f * forca;

                    frames_tremor--;
                }

                // Aplica o tremor em tudo que for desenhado a seguir
                ALLEGRO_TRANSFORM tremor;
                al_identity_transform(&tremor);
                al_translate_transform(&tremor, tremor_x, tremor_y);
                al_use_transform(&tremor);

                // ---------------- MOVIMENTO DO PERSONAGEM ----------------
                if (!quiz_aberto) {

                    ALLEGRO_KEYBOARD_STATE key_state;
                    al_get_keyboard_state(&key_state);

                    bool tecla_esq = al_key_down(&key_state, ALLEGRO_KEY_LEFT);
                    bool tecla_dir = al_key_down(&key_state, ALLEGRO_KEY_RIGHT);
                    bool tecla_cima = al_key_down(&key_state, ALLEGRO_KEY_UP);
                    bool tecla_baixo = al_key_down(&key_state, ALLEGRO_KEY_DOWN);

                    // ----- Subindo ou descendo a escada -----
                    if (sala_atual == 1 && na_escada) {

                        float centro = x + tamanho / 2;
                        float sentido = 0;

                        // DIREITA/CIMA sobem, ESQUERDA/BAIXO descem
                        if (tecla_dir || tecla_cima) {
                            sentido += 1;
                        }
                        if (tecla_esq || tecla_baixo) {
                            sentido -= 1;
                        }

                        centro += sentido * velo;

                        // Chegou no topo: fica no andar de cima
                        if (centro >= escada_x2) {
                            centro = escada_x2;
                            na_escada = false;
                            andar = 1;
                        }

                        // Chegou na base: volta para o terreo
                        else if (centro <= escada_x1) {
                            centro = escada_x1;
                            na_escada = false;
                            andar = 0;
                        }

                        x = centro - tamanho / 2;

                        if (na_escada) {
                            y = pes_na_escada(
                                    centro,
                                    escada_x1, escada_x2,
                                    pes_chao, pes_cima
                                ) - tamanho;
                        } else {
                            y = (andar == 1) ? Y_CIMA : Y_INICIAL;
                        }
                    }

                    // ----- Andando normalmente -----
                    else {

                        if (tecla_esq) {
                            x -= velo;

                            if (x < 0) {
                                x = 0;
                            }
                        }

                        if (tecla_dir) {
                            x += velo;

                            if (x + tamanho > largura_tela) {
                                x = largura_tela - tamanho;
                            }
                        }

                        // O andar de cima termina no topo da escada
                        if (
                            sala_atual == 1 &&
                            andar == 1 &&
                            x + tamanho / 2 > escada_x2
                        ) {
                            x = escada_x2 - tamanho / 2;
                        }

                        // Altura do personagem de acordo com o andar
                        y = (sala_atual == 1 && andar == 1) ? Y_CIMA : Y_INICIAL;

                        // Entrar na escada
                        if (sala_atual == 1) {

                            float centro = x + tamanho / 2;

                            // Perto da base: CIMA para subir
                            if (
                                andar == 0 &&
                                tecla_cima &&
                                fabsf(centro - escada_x1) <= margem_escada
                            ) {
                                na_escada = true;
                                x = escada_x1 + 1 - tamanho / 2;
                            }

                            // Perto do topo: BAIXO para descer
                            else if (
                                andar == 1 &&
                                tecla_baixo &&
                                centro >= escada_x2 - margem_escada
                            ) {
                                na_escada = true;
                                x = escada_x2 - 1 - tamanho / 2;
                            }

                            if (na_escada) {
                                y = pes_na_escada(
                                        x + tamanho / 2,
                                        escada_x1, escada_x2,
                                        pes_chao, pes_cima
                                    ) - tamanho;
                            }
                        }

                        // Troca de sala
                        if (
                            sala_atual == 1 &&
                            x + tamanho >= largura_tela
                        ) {
                            sala_atual = 2;
                            andar = 0;
                            na_escada = false;
                            x = margem_entrada;
                        }

                        else if (
                            sala_atual == 2 &&
                            x <= 0
                        ) {
                            sala_atual = 1;
                            andar = 0;
                            na_escada = false;
                            x = largura_tela - tamanho - margem_entrada;
                        }
                    }
                }

                // Verifica colisao com a zona do bau
                esta_na_zona = (
                    sala_atual == 1 &&
                    x < zona_x + zona_largura &&
                    x + tamanho > zona_x &&
                    y < zona_y + zona_altura &&
                    y + tamanho > zona_y
                );

                // Verifica colisao com a zona do andar de cima
                esta_na_zona2 = (
                    sala_atual == 1 &&
                    andar == 1 &&
                    !na_escada &&
                    x < zona2_x + zona2_largura &&
                    x + tamanho > zona2_x &&
                    y < zona2_y + zona2_altura &&
                    y + tamanho > zona2_y
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

                // Desenha as zonas de interacao
                if (sala_atual == 1) {

                    // Zona do bau
                    al_draw_rectangle(
                        zona_x, zona_y,
                        zona_x + zona_largura,
                        zona_y + zona_altura,
                        al_map_rgb(0, 0, 255),
                        ES(2)
                    );

                    // Zona do andar de cima
                    al_draw_rectangle(
                        zona2_x, zona2_y,
                        zona2_x + zona2_largura,
                        zona2_y + zona2_altura,
                        al_map_rgb(0, 0, 255),
                        ES(2)
                    );

                    #if DEBUG_ESCADA
                    // Linha da escada (verde) e do andar de cima (azul claro)
                    al_draw_line(
                        escada_x1, pes_chao,
                        escada_x2, pes_cima,
                        al_map_rgb(0, 255, 0), ES(2)
                    );
                    al_draw_line(
                        0, pes_cima,
                        escada_x2, pes_cima,
                        al_map_rgb(0, 255, 255), ES(2)
                    );
                    #endif
                }

                // Desenha o personagem
                al_draw_filled_rectangle(
                    x, y,
                    x + tamanho,
                    y + tamanho,
                    al_map_rgb(200, 200, 0)
                );

                // Mensagens de interacao (uma de cada vez)
                if (!quiz_aberto) {

                    float centro_atual = x + tamanho / 2;

                    bool perto_da_base =
                        sala_atual == 1 &&
                        !na_escada &&
                        andar == 0 &&
                        fabsf(centro_atual - escada_x1) <= margem_escada;

                    bool perto_do_topo =
                        sala_atual == 1 &&
                        !na_escada &&
                        andar == 1 &&
                        centro_atual >= escada_x2 - margem_escada;

                    if (esta_na_zona) {
                        TEXTO(
                            al_map_rgb(255, 255, 255),
                            largura_tela / 2, PY(50),
                            ALLEGRO_ALIGN_CENTRE,
                            "Pressione [E] para interagir com o bau!"
                        );
                    }

                    else if (esta_na_zona2) {
                        TEXTO(
                            al_map_rgb(255, 255, 255),
                            largura_tela / 2, PY(50),
                            ALLEGRO_ALIGN_CENTRE,
                            "Pressione [E] para interagir!"
                        );
                    }

                    else if (perto_da_base) {
                        TEXTO(
                            al_map_rgb(255, 255, 255),
                            largura_tela / 2, PY(50),
                            ALLEGRO_ALIGN_CENTRE,
                            "Pressione [CIMA] para subir a escada"
                        );
                    }

                    else if (perto_do_topo) {
                        TEXTO(
                            al_map_rgb(255, 255, 255),
                            largura_tela / 2, PY(50),
                            ALLEGRO_ALIGN_CENTRE,
                            "Pressione [BAIXO] para descer a escada"
                        );
                    }
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

                    const Quiz *quiz = &quizzes[quiz_atual];

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

                    // Linhas da pergunta
                    for (int i = 0; i < quiz->num_linhas; i++) {
                        TEXTO(
                            al_map_rgb(255, 255, 255),
                            (jx1 + jx2) / 2, jy1 + PY(60) + i * PY(22),
                            ALLEGRO_ALIGN_CENTRE,
                            quiz->pergunta[i]
                        );
                    }

                    // Alternativas
                    for (int i = 0; i < 4; i++) {

                        float oy = jy1 + PY(150) + i * PY(40);

                        ALLEGRO_COLOR cor_texto =
                            al_map_rgb(200, 200, 200);

                        if (i == opcao_selecionada) {

                            // Cor do destaque: azul normal, verde se acertou, vermelho se errou
                            ALLEGRO_COLOR cor_destaque = al_map_rgb(70, 70, 120);

                            if (frames_mensagem > 0) {
                                cor_destaque = ultima_resposta_correta
                                    ? al_map_rgb(40, 120, 60)
                                    : al_map_rgb(140, 40, 40);
                            }

                            al_draw_filled_rectangle(
                                jx1 + PX(30), oy - PY(8),
                                jx2 - PX(30), oy + PY(22),
                                cor_destaque
                            );

                            cor_texto =
                                al_map_rgb(255, 255, 255);
                        }

                        TEXTO(
                            cor_texto,
                            jx1 + PX(50), oy,
                            0,
                            quiz->alternativas[i]
                        );
                    }

                    // Mensagem de resposta (aparece por 2 segundos)
                    if (frames_mensagem > 0) {

                        const char *mensagem = ultima_resposta_correta
                            ? "Resposta correta!"
                            : "Resposta errada...";

                        ALLEGRO_COLOR cor_mensagem = ultima_resposta_correta
                            ? al_map_rgb(120, 255, 120)
                            : al_map_rgb(255, 90, 90);

                        // Texto com o dobro do tamanho normal
                        desenha_texto(
                            fonte,
                            cor_mensagem,
                            (jx1 + jx2) / 2, jy2 - PY(45),
                            ALLEGRO_ALIGN_CENTRE,
                            esc * 2,
                            mensagem
                        );
                    }

                    // Instrucoes (somem enquanto a mensagem esta na tela)
                    else {
                        TEXTO(
                            al_map_rgb(180, 180, 180),
                            (jx1 + jx2) / 2, jy2 - PY(35),
                            ALLEGRO_ALIGN_CENTRE,
                            "[CIMA/BAIXO] escolher  [ENTER] responder  [E/ESC] fechar"
                        );
                    }
                }

                // Remove o tremor para nao afetar os proximos desenhos
                al_identity_transform(&tremor);
                al_use_transform(&tremor);
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