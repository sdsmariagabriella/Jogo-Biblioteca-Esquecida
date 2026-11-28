#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_image.h>

typedef enum
{
    ESTADO_MENU,
    ESTADO_HISTORIA,
    ESTADO_ENVELOPE,
    ESTADO_FLORESTA,
    ESTADO_JOGANDO,
    game_over,
    vitoria
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
#define BASE_ALTURA 600.0f

#define PX(v) ((v) * esc_x)
#define PY(v) ((v) * esc_y)
#define ES(v) ((v) * esc)

// Quantidade total de quizzes cadastrados no jogo (o indice 0 nao e usado)
#define TOTAL_QUIZZES 5

// Quantidade de enigmas resolvidos necessarios para vencer o jogo
#define ENIGMAS_PARA_VITORIA 4

// ---------------------------------------------------------------
// SPRITES DO PERSONAGEM
//
// Os 8 desenhos (64x64) ficam na pasta img/boneco/ com estes nomes:
//
//   boneco_de_frente.png                  -> parado / descendo a escada
//   boneco_de_costas.png                  -> subindo a escada
//   boneco_geral_direita_0001..0003.png   -> andando para a direita
//   boneco_andando_para_esquerda_1..3.png -> andando para a esquerda
//
// Posicoes de cada desenho dentro do vetor "sprites":
//   0 = frente, 1 = costas, 2..4 = direita, 5..7 = esquerda
// ---------------------------------------------------------------

#define TOTAL_SPRITES 8
#define SPRITE_FRENTE 0
#define SPRITE_COSTAS 1
#define SPRITE_DIREITA 2  // primeiro desenho de andar para a direita (2, 3, 4)
#define SPRITE_ESQUERDA 5 // primeiro desenho de andar para a esquerda (5, 6, 7)

// Quantos desenhos tem o ciclo de andar de cada lado
#define TOTAL_FRAMES_ANDAR 3

// Parte de baixo do quadro 64x64 que fica vazia embaixo dos pes
// (cerca de 12 pixels de 64)
#define MARGEM_INFERIOR_SPRITE (12.0f / 64.0f)

// Quantos ticks (1/60 s) cada desenho da animacao de andar fica na tela
#define TICKS_POR_FRAME_ANIM 8

// Nome do arquivo de cada desenho, na mesma ordem do vetor "sprites"
static const char *ARQUIVOS_SPRITES[TOTAL_SPRITES] = {
    "img/boneco/boneco_de_frente.png",
    "img/boneco/boneco_de_costas.png",
    "img/boneco/boneco_geral_direita_0001.png",
    "img/boneco/boneco_geral_direita_0002.png",
    "img/boneco/boneco_geral_direita_0003.png",
    "img/boneco/boneco_andando_para_esquerda_1.png",
    "img/boneco/boneco_andando_para_esquerda_2.png",
    "img/boneco/boneco_andando_para_esquerda_3.png"};

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
    const char *texto)
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
    desenha_texto(fonte, (cor), (x), (y), (flags), esc * 1.5f, (str))

// Desenha uma imagem esticada na tela inteira com transparencia
// (alfa vai de 0.0 = invisivel ate 1.0 = totalmente visivel).
// Os quatro canais recebem o mesmo valor porque o Allegro 5 usa
// alfa pre-multiplicado.
static void desenha_imagem_tela(
    ALLEGRO_BITMAP *img,
    float alfa,
    float largura,
    float altura)
{
    al_draw_tinted_scaled_bitmap(
        img,
        al_map_rgba_f(alfa, alfa, alfa, alfa),
        0, 0,
        al_get_bitmap_width(img),
        al_get_bitmap_height(img),
        0, 0,
        largura, altura,
        0);
}

// Escolhe qual desenho do personagem aparece neste momento
static ALLEGRO_BITMAP *escolhe_sprite(
    ALLEGRO_BITMAP *sprites[],
    bool andando,
    bool virado_esquerda,
    int frame_anim,
    int escada_estado)
{
    // Subindo a escada: de costas
    if (escada_estado == 1)
    {
        return sprites[SPRITE_COSTAS];
    }

    // Descendo a escada: de frente
    if (escada_estado == 2)
    {
        return sprites[SPRITE_FRENTE];
    }

    // Andando: ciclo de desenhos do lado para onde o personagem vai
    if (andando)
    {
        int primeiro = virado_esquerda ? SPRITE_ESQUERDA : SPRITE_DIREITA;
        return sprites[primeiro + frame_anim];
    }

    // Parado: de frente
    return sprites[SPRITE_FRENTE];
}

// Desenha o personagem (com fade opcional, usado na floresta).
//
// (x, y) e "tamanho" continuam sendo a caixa de colisao do personagem.
// O desenho (tam_sprite) pode ser bem maior que essa caixa: ele fica
// centralizado na caixa e com os pes na parte de baixo dela, assim a
// colisao, as zonas de interacao e a escada nao mudam.
static void desenha_personagem(
    ALLEGRO_BITMAP *sprite,
    float x, float y,
    float tamanho,
    float tam_sprite,
    float alfa)
{
    float dx = x + (tamanho - tam_sprite) / 2.0f;

    // Os desenhos tem uma margem vazia embaixo dos pes; descontamos essa
    // margem para os pes ficarem exatamente na base da caixa de colisao
    float dy = y + tamanho - tam_sprite * (1.0f - MARGEM_INFERIOR_SPRITE);

    al_draw_tinted_scaled_bitmap(
        sprite,
        al_map_rgba_f(alfa, alfa, alfa, alfa),
        0, 0,
        al_get_bitmap_width(sprite),
        al_get_bitmap_height(sprite),
        dx, dy,
        tam_sprite, tam_sprite,
        0);
}

// Avanca a animacao de andar (chamada uma vez por tick)
static void atualiza_animacao(bool andando, int *frame_anim, int *contador_anim)
{
    if (andando)
    {
        (*contador_anim)++;

        if (*contador_anim >= TICKS_POR_FRAME_ANIM)
        {
            *contador_anim = 0;
            *frame_anim = (*frame_anim + 1) % TOTAL_FRAMES_ANDAR;
        }
    }
    else
    {
        // Parou: volta ao primeiro desenho do ciclo
        *frame_anim = 0;
        *contador_anim = 0;
    }
}

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
        cor);
}

// Desenha o bau no andar de baixo da sala 1
static void desenha_bau(float x, float y, float largura, float altura, float esc)
{
    ALLEGRO_COLOR madeira_escura = al_map_rgb(95, 55, 25);
    ALLEGRO_COLOR madeira_clara = al_map_rgb(135, 80, 35);
    ALLEGRO_COLOR metal = al_map_rgb(215, 175, 55);
    ALLEGRO_COLOR contorno = al_map_rgb(40, 20, 10);

    float altura_tampa = altura * 0.38f;

    // Base do bau
    al_draw_filled_rectangle(
        x, y + altura_tampa,
        x + largura, y + altura,
        madeira_escura);

    // Tampa do bau
    al_draw_filled_rectangle(
        x, y,
        x + largura, y + altura_tampa,
        madeira_clara);

    // Faixas de metal nas laterais
    float largura_faixa = largura * 0.12f;
    al_draw_filled_rectangle(x, y, x + largura_faixa, y + altura, metal);
    al_draw_filled_rectangle(x + largura - largura_faixa, y, x + largura, y + altura, metal);

    // Contornos
    al_draw_rectangle(x, y, x + largura, y + altura, contorno, ES(2));
    al_draw_line(x, y + altura_tampa, x + largura, y + altura_tampa, contorno, ES(2));

    // Fechadura central
    float fech_larg = largura * 0.18f;
    float fech_alt = altura * 0.24f;
    float fech_x = x + (largura - fech_larg) / 2.0f;
    float fech_y = y + altura_tampa - fech_alt * 0.35f;

    al_draw_filled_rectangle(fech_x, fech_y, fech_x + fech_larg, fech_y + fech_alt, metal);
    al_draw_rectangle(fech_x, fech_y, fech_x + fech_larg, fech_y + fech_alt, contorno, ES(1.5f));
}

int main(void)
{
    // Inicializa o Allegro
    if (!al_init())
    {
        printf("Erro ao iniciar o Allegro!\n");
        return 1;
    }

    // Inicializa teclado e mouse
    if (!al_install_keyboard())
    {
        printf("Erro no teclado!\n");
        return 1;
    }

    if (!al_install_mouse())
    {
        printf("Erro no mouse!\n");
        return 1;
    }

    // Inicializa os addons
    if (!al_init_primitives_addon())
    {
        printf("Erro nas primitivas!\n");
        return 1;
    }

    if (!al_init_image_addon())
    {
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

    if (!janela)
    {
        printf("Erro ao criar a janela!\n");
        return 1;
    }

    al_set_window_title(janela, "Biblioteca Esquecida");

    // Carrega a imagem da sala 1
    ALLEGRO_BITMAP *imagem_sala1 =
        al_load_bitmap("img/bibliotecaDestruida2.png");

    if (!imagem_sala1)
    {
        printf("Erro ao carregar a imagem da sala 1!\n");
        al_destroy_display(janela);
        return 1;
    }

    // Carrega a imagem da sala 1 depois do primeiro quiz acertado (salao sujo)
    ALLEGRO_BITMAP *imagem_sala1_suja =
        al_load_bitmap("img/bibliotecaSuja.png");

    if (!imagem_sala1_suja)
    {
        printf("Erro ao carregar a imagem da sala 1 (suja)!\n");
        al_destroy_bitmap(imagem_sala1);
        al_destroy_display(janela);
        return 1;
    }

    // Carrega a imagem da sala 2
    ALLEGRO_BITMAP *imagem_sala2 =
        al_load_bitmap("img/bibliotecaSala2.png");

    if (!imagem_sala2)
    {
        printf("Erro ao carregar a imagem da sala 2!\n");
        al_destroy_bitmap(imagem_sala1);
        al_destroy_display(janela);
        return 1;
    }

    // imagem da sala 3 (escritorio)
    ALLEGRO_BITMAP *imagem_sala3 =
        al_load_bitmap("img/bibliotecaEscritorio.png");
    if (!imagem_sala3)
    {
        printf("Erro ao carregar a imagem da sala 3!\n");
        al_destroy_bitmap(imagem_sala2);
        al_destroy_bitmap(imagem_sala1);
        al_destroy_display(janela);
        return 1;
    }

    // carregar imagem game over
    ALLEGRO_BITMAP *imagem_game_over =
        al_load_bitmap("img/bibliotecaRuinas.png");
    if (!imagem_game_over)
    {
        printf("Erro ao carregar a imagem de game over!\n");
        return 1;
    }

    // carregar imagem vitoria
    ALLEGRO_BITMAP *imagem_vitoria =
        al_load_bitmap("img/biblioteca.png");
    if (!imagem_vitoria)
    {
        printf("Erro ao carregar a imagem da biblioteca reconstruida!\n");
        return 1;
    }

    // Os 3 cenarios do salao da sala 1, do mais destruido ao restaurado:
    //   0 = bibliotecaDestruida2.png (comeco do jogo)
    //   1 = bibliotecaSuja.png       (depois do 1o quiz acertado)
    //   2 = biblioteca.png           (depois do 2o quiz acertado)
    ALLEGRO_BITMAP *fundo_sala1[3] = {
        imagem_sala1,
        imagem_sala1_suja,
        imagem_vitoria};

    // carregar imagens do envelope, da carta e da floresta
    ALLEGRO_BITMAP *imagem_envelope =
        al_load_bitmap("img/envelope.jpg");
    ALLEGRO_BITMAP *imagem_carta =
        al_load_bitmap("img/carta.png");
    ALLEGRO_BITMAP *imagem_floresta =
        al_load_bitmap("img/floresta.jpg");

    if (!imagem_envelope || !imagem_carta || !imagem_floresta)
    {
        printf("Erro ao carregar as imagens do envelope, da carta ou da floresta!\n");
        return 1;
    }

    // carregar os 8 desenhos do personagem (nomes em ARQUIVOS_SPRITES)
    ALLEGRO_BITMAP *sprites[TOTAL_SPRITES];

    for (int i = 0; i < TOTAL_SPRITES; i++)
    {
        sprites[i] = al_load_bitmap(ARQUIVOS_SPRITES[i]);

        if (!sprites[i])
        {
            printf("Erro ao carregar o desenho do personagem: %s\n", ARQUIVOS_SPRITES[i]);
            return 1;
        }
    }

    // Cria a fonte
    ALLEGRO_FONT *fonte = al_create_builtin_font();

    if (!fonte)
    {
        printf("Erro ao criar a fonte!\n");
        al_destroy_bitmap(imagem_sala2);
        al_destroy_bitmap(imagem_sala1);
        al_destroy_display(janela);
        return 1;
    }

    // Cria o timer
    ALLEGRO_TIMER *timer = al_create_timer(1.0 / FPS);

    if (!timer)
    {
        printf("Erro ao criar o timer!\n");
        al_destroy_font(fonte);
        al_destroy_bitmap(imagem_sala2);
        al_destroy_bitmap(imagem_sala1);
        al_destroy_display(janela);
        return 1;
    }

    // Cria a fila de eventos
    ALLEGRO_EVENT_QUEUE *fila = al_create_event_queue();

    if (!fila)
    {
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
        fila, al_get_keyboard_event_source());

    al_register_event_source(
        fila, al_get_mouse_event_source());

    al_register_event_source(
        fila, al_get_display_event_source(janela));

    al_register_event_source(
        fila, al_get_timer_event_source(timer));

    // ---------------- TELA INICIAL ----------------

    // Posicao do botao iniciar
    float botao_x1 = PX(280);
    float botao_y1 = PY(420);
    float botao_x2 = PX(520);
    float botao_y2 = PY(475);

    // ---------------- PERSONAGEM ----------------

    // Caixa de colisao do personagem (usada nas zonas, limites e escada)
    float tamanho = ES(50);

    // Tamanho do DESENHO do personagem na tela (proporcional ao cenario).
    // 0.38 = o quadro de 64x64 do desenho ocupa 38% da altura da tela
    // (a menina em si fica com cerca de 29% da altura, uns 3/5 da altura das portas).
    // Para aumentar ou diminuir o boneco, mude apenas este numero
    // (ex: 0.28 menor, 0.36 maior).
    const float tam_sprite = altura_tela * 0.38f;

    float velo = PX(5);

    // Posicao fixa no eixo y (o personagem so se move no eixo x)
    // Andar de baixo (chao)
    const float Y_INICIAL = PY(530);

    // Andar de cima (varanda da sala 1). Se o personagem ficar acima ou
    // abaixo do piso da varanda, ajuste apenas este valor.
    const float Y_CIMA = PY(160);

    float x = PX(100);
    float y = Y_INICIAL;

    // Controle da animacao do personagem
    bool andando = false;         // true enquanto uma seta esquerda/direita esta apertada
    bool virado_esquerda = false; // para qual lado o personagem esta olhando
    int frame_anim = 0;           // qual desenho do ciclo de andar esta na tela
    int contador_anim = 0;        // conta ticks para trocar de desenho

    // ---------------- ESCADA (SALA 1) ----------------

    // Onde o personagem consegue subir a escada (parte de baixo)
    const float escada_base_x = PX(165);
    const float escada_base_largura = PX(90);

    // Limite direito da varanda (onde a escada chega la em cima)
    const float x_max_cima = PX(281) - tamanho;

    // Perto do topo da escada o personagem consegue descer
    const float x_topo_escada_min = x_max_cima - PX(60);

    // Ponto onde o personagem aparece ao descer a escada
    const float x_pe_escada = PX(190);

    // Duracao da animacao de subir/descer (3/4 de segundo)
    const int duracao_escada = (FPS * 3) / 4;

    bool andar_cima = false; // true = personagem esta na varanda
    int escada_estado = 0;   // 0 = parado, 1 = subindo, 2 = descendo
    float progresso_escada = 0;
    float x_origem_escada = 0, y_origem_escada = 0;
    float x_destino_escada = 0, y_destino_escada = 0;

    bool na_base_escada = false;
    bool na_topo_escada = false;

    // ---------------- SALAS ----------------

    int sala_atual = 1;
    float margem_entrada = PX(10);
    int vidas = 3;

    // Restauracao do salao da sala 1 (0, 1 ou 2, veja "fundo_sala1").
    // A troca de cenario acontece com um fade de 1 segundo:
    // "estagio_anterior" fica por baixo e "estagio_salao" aparece por cima.
    int estagio_salao = 0;
    int estagio_anterior = 0;
    float alfa_estagio = 1.0f; // 1.0 = troca concluida

    // Zona de interacao do bau alinhada ao chao do andar de baixo
    float zona_x = PX(700);
    float zona_y = PY(495);
    float zona_largura = PX(75);
    float zona_altura = PY(55);

    bool esta_na_zona = false;
    // Zona de interacao da Sala 2 (onde estao os livros sobre a mesa do centro)
    float zona2_x = PX(300);
    float zona2_y = PY(450);
    float zona2_largura = PX(230);
    float zona2_altura = PY(80);

    bool esta_na_zona_sala2 = false;
    // Zona de interacao sala 2 do terceiro enigma (vaso de planta)
    float zona3_x = PX(0);
    float zona3_y = PY(490);
    float zona3_largura = PX(55);
    float zona3_altura = PY(80);
    bool esta_na_zona_sala2_3 = false;

    // zona de interação da luz na Sala 3 (luminaria verde sobre a mesa do escritorio)
    float zona4_x = PX(400);
    float zona4_y = PY(420);
    float zona4_largura = PX(120);
    float zona4_altura = PY(100);

    bool esta_na_zona_sala3_luz = false;

    // zona de interacao da estante, na varanda da sala 1 (quiz do Titanic)
    float zona5_x = PX(60);
    float zona5_y = PY(110);
    float zona5_largura = PX(100);
    float zona5_altura = PY(100);

    bool esta_na_zona_estante = false;

    // ---------------- QUIZ ----------------

    bool quiz_aberto = false;

    int opcao_selecionada = 0;
    int opcao_respondida = -1;
    int quiz_atual = 1;

    // Indice da alternativa correta de cada quiz (0 = A, 1 = B, 2 = C, 3 = D)
    // A posicao 0 nao e usada: o quiz 1 esta na posicao 1, e assim por diante
    const int respostas_certas[TOTAL_QUIZZES + 1] = {0, 1, 1, 1, 1, 2};

    // Quais quizzes ja foram respondidos corretamente
    bool quiz_resolvido[TOTAL_QUIZZES + 1] = {false};

    // Flags de bloqueio de passagem entre salas
    bool passagem_sala2_bloqueada = false;
    bool passagem_sala3_bloqueada = false;

    // Temporizador para exibir "parte restaurada!" ao interagir novamente
    int frames_restaurada = 0;

    const char *alternativas[4] = {
        "A) Um grande terremoto",
        "B) A erupcao do Monte Vesuvio",
        "C) Uma grande inundacao",
        "D) Um incendio na cidade"};

    const char *alternativas2[4] = {
        "A) Corrida Armamentista",
        "B) Corrida Espacial",
        "C) Revolucao Industrial",
        "D) Guerra dos Cem Anos"};

    const char *alternativas3[4] = {
        "A) A queda do Imperio Romano",
        "B) Assassinato de Francisco Ferdinando",
        "C) A Revolucao Francesa",
        "D) A Independencia dos Estados Unidos"};

    const char *alternativas4[4] = {
        "A) Os moradores descobriram uma passagem secreta.",
        "B) Os soldados gregos estavam escondidos dentro do cavalo.",
        "C) A cidade foi destruída por um terremoto.",
        "D) O exército construiu uma muralha ao redor da cidade."};

    const char *alternativas5[4] = {
        "A) O naufragio do Lusitania",
        "B) O naufragio do encouracado Bismarck",
        "C) O naufragio do RMS Titanic",
        "D) O naufragio do Andrea Doria"};

    // ---------------- MENSAGEM DE RESPOSTA E TREMOR ----------------

    // Duracao da mensagem "Resposta correta!" / "Resposta errada..." (2 segundos)
    const int duracao_mensagem = 2 * FPS;

    // Duracao do tremor de tela ao errar (meio segundo)
    const int duracao_tremor = FPS / 2;

    int frames_mensagem = 0; // frames restantes da mensagem (0 = sem mensagem)
    int frames_tremor = 0;   // frames restantes do tremor (0 = sem tremor)
    bool ultima_resposta_correta = false;

    // ---------------- ENVELOPE / CARTA / FLORESTA ----------------

    float alfa_envelope = 0.0f;
    float alfa_carta = 0.0f;
    bool carta_abrindo = false; // true depois do clique no envelope
    float alfa_floresta = 0.0f;

    // Velocidade dos fades: 1 segundo para ir de 0 a 1
    const float passo_fade = 1.0f / FPS;

    // Altura do personagem no caminho de terra da floresta.
    // Se ele ficar acima ou abaixo do caminho, ajuste o 0.76f.
    const float Y_FLORESTA = altura_tela * 0.76f;

    // A porta fica no lado direito da floresta: a partir desta posicao
    // o jogador pode interagir. Se a mensagem aparecer cedo ou tarde
    // demais, ajuste o 0.86f.
    float porta_x = largura_tela * 0.86f;
    bool esta_na_porta = false;

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

    while (rodando)
    {

        ALLEGRO_EVENT evento;
        al_wait_for_event(fila, &evento);

        // Fechar a janela
        if (evento.type == ALLEGRO_EVENT_DISPLAY_CLOSE)
        {
            rodando = false;
        }

        // ---------------- EVENTOS DO TECLADO ----------------

        else if (evento.type == ALLEGRO_EVENT_KEY_DOWN)
        {

            int tecla = evento.keyboard.keycode;

            // Na tela inicial
            if (estado_atual == ESTADO_MENU)
            {

                if (tecla == ALLEGRO_KEY_ENTER)
                {
                    estado_atual = ESTADO_HISTORIA;
                    caracteres_visiveis = 0; // restar o texto
                    contador_frames = 0;     // resetar o contador de frames
                }

                if (tecla == ALLEGRO_KEY_ESCAPE)
                {
                    rodando = false;
                }
            }

            // Na tela da historia
            else if (estado_atual == ESTADO_HISTORIA)
            {

                if (tecla == ALLEGRO_KEY_ENTER || tecla == ALLEGRO_KEY_SPACE)
                {
                    estado_atual = ESTADO_ENVELOPE;
                    alfa_envelope = 0.0f;
                    alfa_carta = 0.0f;
                    carta_abrindo = false;
                }

                if (tecla == ALLEGRO_KEY_ESCAPE)
                {
                    rodando = false;
                }
            }

            // Na tela do envelope e da carta
            else if (estado_atual == ESTADO_ENVELOPE)
            {

                if (tecla == ALLEGRO_KEY_ESCAPE)
                {
                    rodando = false;
                }

                else if (tecla == ALLEGRO_KEY_ENTER || tecla == ALLEGRO_KEY_SPACE)
                {
                    // Primeiro clique: abre o envelope (a carta aparece com fade)
                    if (!carta_abrindo && alfa_envelope >= 1.0f)
                    {
                        carta_abrindo = true;
                    }

                    // Segundo clique: vai para a floresta
                    else if (carta_abrindo && alfa_carta >= 1.0f)
                    {
                        estado_atual = ESTADO_FLORESTA;
                        x = PX(40);
                        y = Y_FLORESTA;
                        alfa_floresta = 0.0f;
                        esta_na_porta = false;
                        virado_esquerda = false;
                    }
                }
            }

            // Na floresta
            else if (estado_atual == ESTADO_FLORESTA)
            {

                if (tecla == ALLEGRO_KEY_ESCAPE)
                {
                    rodando = false;
                }

                else if (
                    tecla == ALLEGRO_KEY_E &&
                    esta_na_porta &&
                    alfa_floresta >= 1.0f)
                {
                    // Destranca a porta e entra na biblioteca
                    estado_atual = ESTADO_JOGANDO;
                    sala_atual = 1;
                    x = PX(100);
                    y = Y_INICIAL;
                    andar_cima = false;
                    escada_estado = 0;
                    virado_esquerda = false;
                }
            }

            // Tela de game over ou vitoria
            else if (estado_atual == game_over || estado_atual == vitoria)
            {

                if (tecla == ALLEGRO_KEY_ENTER)
                {
                    vidas = 3;
                    sala_atual = 1;
                    x = PX(100);
                    y = Y_INICIAL;
                    virado_esquerda = false;

                    // Volta para o andar de baixo e zera os quizzes resolvidos
                    andar_cima = false;
                    escada_estado = 0;
                    for (int i = 0; i <= TOTAL_QUIZZES; i++)
                    {
                        quiz_resolvido[i] = false;
                    }

                    quiz_aberto = false;
                    opcao_selecionada = 0;
                    opcao_respondida = -1;
                    frames_mensagem = 0;
                    frames_tremor = 0;
                    frames_restaurada = 0;
                    estado_atual = ESTADO_MENU;
                }

                if (tecla == ALLEGRO_KEY_ESCAPE)
                {
                    rodando = false;
                }
            }

            // Durante o jogo
            else
            {

                if (quiz_aberto)
                {

                    // Enquanto a mensagem de resposta aparece, as teclas sao ignoradas
                    if (frames_mensagem > 0)
                    {
                        // nada acontece
                    }

                    else if (tecla == ALLEGRO_KEY_UP)
                    {
                        opcao_selecionada =
                            (opcao_selecionada + 3) % 4;
                    }

                    else if (tecla == ALLEGRO_KEY_DOWN)
                    {
                        opcao_selecionada =
                            (opcao_selecionada + 1) % 4;
                    }

                    else if (tecla == ALLEGRO_KEY_ENTER && opcao_respondida == -1)
                    {
                        opcao_respondida = opcao_selecionada;

                        // Inicia a mensagem de 2 segundos
                        frames_mensagem = duracao_mensagem;

                        if (opcao_respondida == respostas_certas[quiz_atual])
                        {
                            ultima_resposta_correta = true;
                            quiz_resolvido[quiz_atual] = true;
                            printf("Resposta correta!\n");
                        }
                        else
                        {
                            ultima_resposta_correta = false;
                            vidas--;
                            frames_tremor = duracao_tremor;
                            printf("Voce perdeu uma vida!\n");
                        }
                    }

                    else if (
                        tecla == ALLEGRO_KEY_E ||
                        tecla == ALLEGRO_KEY_ESCAPE)
                    {
                        quiz_aberto = false;
                    }
                }

                else
                {

                    if (tecla == ALLEGRO_KEY_ESCAPE)
                    {
                        rodando = false;
                    }

                    // Enquanto o personagem sobe ou desce a escada, nada mais funciona
                    else if (escada_estado == 0)
                    {

                        // Sobe a escada
                        if (tecla == ALLEGRO_KEY_UP && na_base_escada)
                        {
                            escada_estado = 1;
                            progresso_escada = 0;
                            x_origem_escada = x;
                            y_origem_escada = y;
                            x_destino_escada = x_max_cima;
                            y_destino_escada = Y_CIMA;
                        }

                        // Desce a escada
                        else if (tecla == ALLEGRO_KEY_DOWN && na_topo_escada)
                        {
                            escada_estado = 2;
                            progresso_escada = 0;
                            x_origem_escada = x;
                            y_origem_escada = y;
                            x_destino_escada = x_pe_escada;
                            y_destino_escada = Y_INICIAL;
                        }

                        else if (tecla == ALLEGRO_KEY_E)
                        {
                            int quiz_alvo = 0;

                            if (sala_atual == 1 && esta_na_zona)
                            {
                                quiz_alvo = 1;
                            }
                            else if (sala_atual == 1 && esta_na_zona_estante)
                            {
                                quiz_alvo = 5;
                            }
                            else if (sala_atual == 2 && esta_na_zona_sala2)
                            {
                                quiz_alvo = 2;
                            }
                            else if (sala_atual == 2 && esta_na_zona_sala2_3)
                            {
                                quiz_alvo = 3;
                            }
                            else if (sala_atual == 3 && esta_na_zona_sala3_luz)
                            {
                                quiz_alvo = 4;
                            }

                            if (quiz_alvo > 0)
                            {
                                // Se o quiz ja foi acertado, exibe "parte restaurada!"
                                if (quiz_resolvido[quiz_alvo])
                                {
                                    frames_restaurada = duracao_mensagem;
                                }
                                else
                                {
                                    quiz_atual = quiz_alvo;
                                    quiz_aberto = true;
                                    opcao_selecionada = 0;
                                    opcao_respondida = -1;
                                    frames_restaurada = 0;
                                }
                            }
                        }
                    }
                }
            }
        }

        // ---------------- EVENTOS DO MOUSE ----------------

        else if (evento.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN)
        {

            if (estado_atual == ESTADO_MENU)
            {
                // Verifica se clicou no botao iniciar
                if (
                    evento.mouse.button == 1 &&
                    evento.mouse.x >= botao_x1 &&
                    evento.mouse.x <= botao_x2 &&
                    evento.mouse.y >= botao_y1 &&
                    evento.mouse.y <= botao_y2)
                {
                    estado_atual = ESTADO_HISTORIA;
                    caracteres_visiveis = 0; // restar o texto
                    contador_frames = 0;     // resetar o contador de frames
                }
            }

            else if (estado_atual == ESTADO_HISTORIA)
            {
                if (evento.mouse.button == 1)
                {
                    estado_atual = ESTADO_ENVELOPE;
                    alfa_envelope = 0.0f;
                    alfa_carta = 0.0f;
                    carta_abrindo = false;
                }
            }

            else if (estado_atual == ESTADO_ENVELOPE)
            {
                if (evento.mouse.button == 1)
                {
                    // Primeiro clique: abre o envelope (a carta aparece com fade)
                    if (!carta_abrindo && alfa_envelope >= 1.0f)
                    {
                        carta_abrindo = true;
                    }

                    // Segundo clique: vai para a floresta
                    else if (carta_abrindo && alfa_carta >= 1.0f)
                    {
                        estado_atual = ESTADO_FLORESTA;
                        x = PX(40);
                        y = Y_FLORESTA;
                        alfa_floresta = 0.0f;
                        esta_na_porta = false;
                        virado_esquerda = false;
                    }
                }
            }
        }

        // ---------------- ATUALIZACAO DO JOGO ----------------

        if (evento.type == ALLEGRO_EVENT_TIMER)
        {

            // Tela inicial
            if (estado_atual == ESTADO_MENU)
            {

                // Fundo da tela inicial
                al_draw_scaled_bitmap(
                    imagem_sala1,
                    0, 0,
                    al_get_bitmap_width(imagem_sala1),
                    al_get_bitmap_height(imagem_sala1),
                    0, 0,
                    largura_tela,
                    altura_tela,
                    0);

                // Escurece o fundo
                al_draw_filled_rectangle(
                    0, 0,
                    largura_tela, altura_tela,
                    al_map_rgba(0, 0, 0, 190));

                // Titulo
                TEXTO(
                    al_map_rgb(219, 198, 167),
                    largura_tela / 2, PY(100),
                    ALLEGRO_ALIGN_CENTRE,
                    "BIBLIOTECA ESQUECIDA");

                // Subtitulo
                TEXTO(
                    al_map_rgb(220, 220, 220),
                    largura_tela / 2, PY(155),
                    ALLEGRO_ALIGN_CENTRE,
                    "Uma aventura entre livros e misterios");

                // Texto de explicacao
                TEXTO(
                    al_map_rgb(255, 255, 255),
                    largura_tela / 2, PY(230),
                    ALLEGRO_ALIGN_CENTRE,
                    "Explore a biblioteca esquecida");

                TEXTO(
                    al_map_rgb(255, 255, 255),
                    largura_tela / 2, PY(260),
                    ALLEGRO_ALIGN_CENTRE,
                    "e descubra os misterios escondidos.");

                TEXTO(
                    al_map_rgb(255, 255, 255),
                    largura_tela / 2, PY(310),
                    ALLEGRO_ALIGN_CENTRE,
                    "Movimente-se com as SETAS do teclado.");

                TEXTO(
                    al_map_rgb(255, 255, 255),
                    largura_tela / 2, PY(340),
                    ALLEGRO_ALIGN_CENTRE,
                    "Pressione E para interagir com objetos.");

                TEXTO(
                    al_map_rgb(255, 255, 255),
                    largura_tela / 2, PY(370),
                    ALLEGRO_ALIGN_CENTRE,
                    "Responda ao quiz usando as setas e ENTER.");

                // Botao iniciar
                al_draw_filled_rectangle(
                    botao_x1, botao_y1,
                    botao_x2, botao_y2,
                    al_map_rgb(100, 65, 35));

                al_draw_rectangle(
                    botao_x1, botao_y1,
                    botao_x2, botao_y2,
                    al_map_rgb(255, 220, 150),
                    ES(3));

                // Texto centralizado verticalmente no botao
                TEXTO(
                    al_map_rgb(255, 255, 255),
                    largura_tela / 2,
                    botao_y1 + ((botao_y2 - botao_y1) - ES(8)) / 2,
                    ALLEGRO_ALIGN_CENTRE,
                    "INICIAR JOGO");

                TEXTO(
                    al_map_rgb(200, 200, 200),
                    largura_tela / 2, PY(520),
                    ALLEGRO_ALIGN_CENTRE,
                    "Pressione ENTER ou clique para comecar");
            }

            // Tela preta da historia
            else if (estado_atual == ESTADO_HISTORIA)
            {

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
                if (contador_frames >= velocidade_texto)
                {
                    if (caracteres_visiveis < total_linhas)
                    {
                        caracteres_visiveis++;
                    }
                    contador_frames = 0;
                }

                // Desenha cada linha visivel uma embaixo da outra
                float y_inicial = PY(120);
                float espacamento_linha = PY(30);

                for (int i = 0; i < caracteres_visiveis && i < total_linhas; i++)
                {
                    TEXTO(
                        al_map_rgb(230, 230, 230),
                        largura_tela / 2,
                        y_inicial + (i * espacamento_linha),
                        ALLEGRO_ALIGN_CENTRE,
                        linhas_historia[i]);
                }

                // Aviso para continuar no rodape
                TEXTO(
                    al_map_rgb(180, 180, 180),
                    largura_tela / 2, PY(550),
                    ALLEGRO_ALIGN_CENTRE,
                    "Pressione ENTER, ESPACO ou clique para continuar");
            }

            // ---------------- ENVELOPE E CARTA ----------------
            else if (estado_atual == ESTADO_ENVELOPE)
            {
                // O envelope aparece com fade in
                if (alfa_envelope < 1.0f)
                {
                    alfa_envelope += passo_fade;
                    if (alfa_envelope > 1.0f)
                    {
                        alfa_envelope = 1.0f;
                    }
                }

                // Depois do clique, a carta aparece por cima com fade in
                if (carta_abrindo && alfa_carta < 1.0f)
                {
                    alfa_carta += passo_fade;
                    if (alfa_carta > 1.0f)
                    {
                        alfa_carta = 1.0f;
                    }
                }

                al_clear_to_color(al_map_rgb(0, 0, 0));

                desenha_imagem_tela(imagem_envelope, alfa_envelope, largura_tela, altura_tela);

                if (carta_abrindo)
                {
                    desenha_imagem_tela(imagem_carta, alfa_carta, largura_tela, altura_tela);
                }

                if (!carta_abrindo && alfa_envelope >= 1.0f)
                {
                    TEXTO(
                        al_map_rgb(255, 255, 255),
                        largura_tela / 2, PY(560),
                        ALLEGRO_ALIGN_CENTRE,
                        "Clique para abrir o envelope");
                }
                else if (alfa_carta >= 1.0f)
                {
                    TEXTO(
                        al_map_rgb(255, 255, 255),
                        largura_tela / 2, PY(560),
                        ALLEGRO_ALIGN_CENTRE,
                        "Clique para continuar");
                }
            }

            // ---------------- FLORESTA ----------------
            else if (estado_atual == ESTADO_FLORESTA)
            {
                // A floresta aparece com fade in
                if (alfa_floresta < 1.0f)
                {
                    alfa_floresta += passo_fade;
                    if (alfa_floresta > 1.0f)
                    {
                        alfa_floresta = 1.0f;
                    }
                }

                ALLEGRO_KEYBOARD_STATE key_state;
                al_get_keyboard_state(&key_state);

                andando = false;

                if (al_key_down(&key_state, ALLEGRO_KEY_LEFT))
                {
                    x -= velo;
                    virado_esquerda = true;
                    andando = true;

                    if (x < 0)
                    {
                        x = 0;
                    }
                }

                if (al_key_down(&key_state, ALLEGRO_KEY_RIGHT))
                {
                    x += velo;
                    virado_esquerda = false;
                    andando = true;

                    if (x > largura_tela - tamanho)
                    {
                        x = largura_tela - tamanho;
                    }
                }

                atualiza_animacao(andando, &frame_anim, &contador_anim);

                // Esta perto da porta?
                esta_na_porta = (x + tamanho > porta_x);

                al_clear_to_color(al_map_rgb(0, 0, 0));

                desenha_imagem_tela(imagem_floresta, alfa_floresta, largura_tela, altura_tela);

                // Personagem (com o mesmo fade da cena)
                desenha_personagem(
                    escolhe_sprite(sprites, andando, virado_esquerda, frame_anim, 0),
                    x, y,
                    tamanho,
                    tam_sprite,
                    alfa_floresta);

                if (alfa_floresta >= 1.0f)
                {
                    if (esta_na_porta)
                    {
                        TEXTO(
                            al_map_rgb(255, 255, 255),
                            largura_tela / 2, PY(50),
                            ALLEGRO_ALIGN_CENTRE,
                            "Pressione [E] para destrancar a porta e entrar");
                    }
                    else
                    {
                        TEXTO(
                            al_map_rgb(200, 200, 200),
                            largura_tela / 2, PY(50),
                            ALLEGRO_ALIGN_CENTRE,
                            "Caminhe com as SETAS ate a porta da biblioteca");
                    }
                }
            }

            // ---------------- TELA DE GAME OVER ----------------
            else if (estado_atual == game_over)
            {

                // Desenha a imagem de fundo
                al_draw_scaled_bitmap(
                    imagem_game_over,
                    0, 0,
                    al_get_bitmap_width(imagem_game_over),
                    al_get_bitmap_height(imagem_game_over),
                    0, 0,
                    largura_tela,
                    altura_tela,
                    0);

                // Escurece a imagem para destacar os textos
                al_draw_filled_rectangle(
                    0, 0,
                    largura_tela, altura_tela,
                    al_map_rgba(0, 0, 0, 150));

                // Coracoes no topo (2 cheios e o ultimo quebrado)
                ALLEGRO_COLOR vermelho = al_map_rgb(230, 50, 80);
                float inicio_x = largura_tela / 2 - ES(54);
                float topo_y = PY(130);

                for (int i = 0; i < 2; i++)
                {
                    float cx = inicio_x + (i * ES(40));
                    al_draw_filled_circle(cx + ES(8), topo_y + ES(8), ES(8), vermelho);
                    al_draw_filled_circle(cx + ES(20), topo_y + ES(8), ES(8), vermelho);
                    al_draw_filled_triangle(
                        cx + ES(1), topo_y + ES(11),
                        cx + ES(27), topo_y + ES(11),
                        cx + ES(14), topo_y + ES(28),
                        vermelho);
                }

                // Coracao quebrado ao lado
                float qx = inicio_x + (2 * ES(40));
                al_draw_filled_circle(qx + ES(7), topo_y + ES(8), ES(7), vermelho);
                al_draw_filled_triangle(
                    qx + ES(1), topo_y + ES(11),
                    qx + ES(12), topo_y + ES(11),
                    qx + ES(11), topo_y + ES(27),
                    vermelho);
                al_draw_filled_circle(qx + ES(21), topo_y + ES(8), ES(7), vermelho);
                al_draw_filled_triangle(
                    qx + ES(16), topo_y + ES(11),
                    qx + ES(27), topo_y + ES(11),
                    qx + ES(17), topo_y + ES(27),
                    vermelho);

                // Titulo GAME OVER / VOCE MORREU
                TEXTO(
                    al_map_rgb(255, 210, 40),
                    largura_tela / 2, PY(210),
                    ALLEGRO_ALIGN_CENTRE,
                    "VOCE MORREU!");

                // Instrucao para voltar ao inicio
                TEXTO(
                    al_map_rgb(255, 255, 255),
                    largura_tela / 2, PY(310),
                    ALLEGRO_ALIGN_CENTRE,
                    "VOLTAR AO INICIO?");

                TEXTO(
                    al_map_rgb(200, 200, 200),
                    largura_tela / 2, PY(355),
                    ALLEGRO_ALIGN_CENTRE,
                    "Pressione ENTER para voltar ao inicio");
            }

            else if (estado_atual == vitoria)
            {
                al_draw_scaled_bitmap(
                    imagem_vitoria,
                    0, 0,
                    al_get_bitmap_width(imagem_vitoria),
                    al_get_bitmap_height(imagem_vitoria),
                    0, 0,
                    largura_tela, altura_tela,
                    0);

                // Escurece levemente para destacar o texto de vitoria
                al_draw_filled_rectangle(
                    0, 0,
                    largura_tela, altura_tela,
                    al_map_rgba(0, 0, 0, 120));

                TEXTO(
                    al_map_rgb(255, 220, 80),
                    largura_tela / 2, PY(180),
                    ALLEGRO_ALIGN_CENTRE,
                    "VOCE VENCEU!");

                TEXTO(
                    al_map_rgb(255, 255, 255),
                    largura_tela / 2, PY(240),
                    ALLEGRO_ALIGN_CENTRE,
                    "Os 4 enigmas foram resolvidos corretamente!");

                TEXTO(
                    al_map_rgb(255, 255, 255),
                    largura_tela / 2, PY(275),
                    ALLEGRO_ALIGN_CENTRE,
                    "A biblioteca foi reconstruida!");

                TEXTO(
                    al_map_rgb(200, 200, 200),
                    largura_tela / 2, PY(355),
                    ALLEGRO_ALIGN_CENTRE,
                    "Pressione ENTER para voltar ao inicio");
            }

            // Jogo
            else
            {
                if (frames_restaurada > 0)
                {
                    frames_restaurada--;
                }

                // Contagem da mensagem de resposta (2 segundos)
                if (frames_mensagem > 0)
                {
                    frames_mensagem--;

                    // Quando a mensagem termina, o quiz fecha junto
                    if (frames_mensagem == 0)
                    {
                        quiz_aberto = false;

                        // Se acabaram as vidas, vai para o game over
                        if (vidas <= 0)
                        {
                            estado_atual = game_over;
                        }
                        else
                        {
                            // Conta quantos enigmas foram resolvidos corretamente
                            int enigmas_resolvidos = 0;

                            for (int i = 1; i <= TOTAL_QUIZZES; i++)
                            {
                                if (quiz_resolvido[i])
                                {
                                    enigmas_resolvidos++;
                                }
                            }

                            // Ao resolver os 4 enigmas corretamente, exibe a tela de vitoria
                            if (enigmas_resolvidos >= ENIGMAS_PARA_VITORIA)
                            {
                                estado_atual = vitoria;
                            }
                        }
                    }
                }

                // Calcula o tremor de tela (diminui aos poucos ate parar)
                float tremor_x = 0;
                float tremor_y = 0;

                if (frames_tremor > 0)
                {
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

                // Verifica desbloqueios das salas
                bool sala1_desbloqueou_sala2 = quiz_resolvido[1] || quiz_resolvido[5];
                bool sala2_desbloqueou_sala3 = quiz_resolvido[2] || quiz_resolvido[3];

                // A cada tick comeca considerando que o personagem esta parado
                andando = false;

                // Movimento do personagem
                if (!quiz_aberto)
                {

                    // Animacao de subir/descer a escada
                    if (escada_estado != 0)
                    {
                        progresso_escada += 1.0f / duracao_escada;

                        if (progresso_escada > 1.0f)
                        {
                            progresso_escada = 1.0f;
                        }

                        // O personagem anda em diagonal entre o ponto de origem e o destino
                        x = x_origem_escada + (x_destino_escada - x_origem_escada) * progresso_escada;
                        y = y_origem_escada + (y_destino_escada - y_origem_escada) * progresso_escada;

                        // Chegou ao fim da escada
                        if (progresso_escada >= 1.0f)
                        {
                            andar_cima = (escada_estado == 1);
                            escada_estado = 0;
                        }
                    }

                    // Movimento normal (somente no eixo x)
                    else
                    {

                        ALLEGRO_KEYBOARD_STATE key_state;
                        al_get_keyboard_state(&key_state);

                        // Na varanda o limite da direita e o topo da escada
                        float limite_direito = andar_cima ? x_max_cima : largura_tela - tamanho;

                        if (al_key_down(&key_state, ALLEGRO_KEY_LEFT))
                        {
                            x -= velo;
                            virado_esquerda = true;
                            andando = true;

                            if (x < 0)
                            {
                                x = 0;
                            }
                        }

                        if (al_key_down(&key_state, ALLEGRO_KEY_RIGHT))
                        {
                            x += velo;
                            virado_esquerda = false;
                            andando = true;

                            if (x > limite_direito)
                            {
                                x = limite_direito;
                            }
                        }

                        // Garante que o personagem fique sempre na altura do andar atual
                        y = andar_cima ? Y_CIMA : Y_INICIAL;

                        // Troca de sala (so acontece no andar de baixo)
                        if (!andar_cima)
                        {
                            // Sala 1 para Sala 2 (bloqueada ate resolver um enigma da Sala 1)
                            if (
                                sala_atual == 1 &&
                                x + tamanho >= largura_tela)
                            {
                                if (sala1_desbloqueou_sala2)
                                {
                                    sala_atual = 2;
                                    x = margem_entrada;
                                }
                                else
                                {
                                    x = largura_tela - tamanho;
                                }
                            }

                            else if (
                                sala_atual == 2 &&
                                x <= 0)
                            {
                                sala_atual = 1;
                                x = largura_tela - tamanho - margem_entrada;
                            }
                            // Sala 2 para Sala 3 (bloqueada ate resolver um enigma da Sala 2)
                            else if (sala_atual == 2 && x + tamanho >= largura_tela)
                            {
                                if (sala2_desbloqueou_sala3)
                                {
                                    sala_atual = 3;
                                    x = margem_entrada;
                                }
                                else
                                {
                                    x = largura_tela - tamanho;
                                }
                            }

                            // Sala 3 para Sala 2
                            else if (sala_atual == 3 && x <= 0)
                            {
                                sala_atual = 2;
                                x = largura_tela - tamanho - margem_entrada;
                            }
                        }
                    }
                }

                // Avanca a animacao de andar do personagem
                atualiza_animacao(andando, &frame_anim, &contador_anim);

                // Verifica se o personagem esta na borda bloqueada da Sala 1 ou Sala 2
                passagem_sala2_bloqueada = (sala_atual == 1 &&
                                            !andar_cima &&
                                            !sala1_desbloqueou_sala2 &&
                                            x + tamanho >= largura_tela - PX(5));

                passagem_sala3_bloqueada = (sala_atual == 2 &&
                                            !andar_cima &&
                                            !sala2_desbloqueou_sala3 &&
                                            x + tamanho >= largura_tela - PX(5));

                // Verifica se o personagem esta na escada
                na_base_escada = (sala_atual == 1 &&
                                  !andar_cima &&
                                  escada_estado == 0 &&
                                  x < escada_base_x + escada_base_largura &&
                                  x + tamanho > escada_base_x);

                na_topo_escada = (sala_atual == 1 &&
                                  andar_cima &&
                                  escada_estado == 0 &&
                                  x >= x_topo_escada_min);

                // Verifica colisao com o bau no andar de baixo
                esta_na_zona = (sala_atual == 1 &&
                                !andar_cima &&
                                escada_estado == 0 &&
                                x < zona_x + zona_largura &&
                                x + tamanho > zona_x);

                // Verifica colisao com a estante da varanda (so la em cima)
                esta_na_zona_estante = (sala_atual == 1 &&
                                        andar_cima &&
                                        escada_estado == 0 &&
                                        x < zona5_x + zona5_largura &&
                                        x + tamanho > zona5_x);

                // Verifica colisao com a zona da Sala 2
                esta_na_zona_sala2 = (sala_atual == 2 &&
                                      !andar_cima &&
                                      x < zona2_x + zona2_largura &&
                                      x + tamanho > zona2_x);
                esta_na_zona_sala2_3 = (sala_atual == 2 &&
                                        !andar_cima &&
                                        x < zona3_x + zona3_largura &&
                                        x + tamanho > zona3_x);
                esta_na_zona_sala3_luz = (sala_atual == 3 &&
                                          !andar_cima &&
                                          x < zona4_x + zona4_largura &&
                                          x + tamanho > zona4_x);

                // Desenha o fundo da sala 1 restaurado apos acertar o primeiro quiz.
                if (sala_atual == 1)
                {
                    ALLEGRO_BITMAP *fundo_atual = quiz_resolvido[1]
                        ? imagem_vitoria : imagem_sala1;
                    desenha_imagem_tela(fundo_atual, 1.0f, largura_tela, altura_tela);
                }

                else if (sala_atual == 2)
                {
                    al_draw_scaled_bitmap(
                        imagem_sala2,
                        0, 0,
                        al_get_bitmap_width(imagem_sala2),
                        al_get_bitmap_height(imagem_sala2),
                        0, 0,
                        largura_tela,
                        altura_tela,
                        0);
                }
                else if (sala_atual == 3)
                {
                    al_draw_scaled_bitmap(
                        imagem_sala3,
                        0, 0,
                        al_get_bitmap_width(imagem_sala3),
                        al_get_bitmap_height(imagem_sala3),
                        0, 0,
                        largura_tela,
                        altura_tela,
                        0);
                }

                // Desenha o bau no andar de baixo da sala 1 (sem quadrados azuis)
                if (sala_atual == 1)
                {
                    desenha_bau(zona_x, zona_y, zona_largura, zona_altura, esc);
                }

                // Desenha o personagem
                desenha_personagem(
                    escolhe_sprite(sprites, andando, virado_esquerda, frame_anim, escada_estado),
                    x, y,
                    tamanho,
                    tam_sprite,
                    1.0f);

                // Mensagens no topo da tela
                if (!quiz_aberto)
                {
                    // Se tentou avancar para a Sala 2 antes de resolver um enigma da Sala 1
                    if (passagem_sala2_bloqueada)
                    {
                        float ax1 = PX(120);
                        float ay1 = PY(90);
                        float ax2 = PX(680);
                        float ay2 = PY(155);

                        al_draw_filled_rectangle(
                            ax1, ay1, ax2, ay2,
                            al_map_rgba(60, 15, 15, 220));
                        al_draw_rectangle(
                            ax1, ay1, ax2, ay2,
                            al_map_rgb(255, 180, 60),
                            ES(2));

                        TEXTO(
                            al_map_rgb(255, 210, 80),
                            largura_tela / 2, ay1 + PY(12),
                            ALLEGRO_ALIGN_CENTRE,
                            "ALERTA: Passagem para a Sala 2 bloqueada!");
                        TEXTO(
                            al_map_rgb(255, 255, 255),
                            largura_tela / 2, ay1 + PY(36),
                            ALLEGRO_ALIGN_CENTRE,
                            "Resolva o enigma do bau ou da estante antes de avancar.");
                    }
                    else if (passagem_sala3_bloqueada)
                    {
                        float ax1 = PX(120);
                        float ay1 = PY(90);
                        float ax2 = PX(680);
                        float ay2 = PY(155);

                        al_draw_filled_rectangle(
                            ax1, ay1, ax2, ay2,
                            al_map_rgba(60, 15, 15, 220));
                        al_draw_rectangle(
                            ax1, ay1, ax2, ay2,
                            al_map_rgb(255, 180, 60),
                            ES(2));

                        TEXTO(
                            al_map_rgb(255, 210, 80),
                            largura_tela / 2, ay1 + PY(12),
                            ALLEGRO_ALIGN_CENTRE,
                            "ALERTA: Passagem para a Sala 3 bloqueada!");
                        TEXTO(
                            al_map_rgb(255, 255, 255),
                            largura_tela / 2, ay1 + PY(36),
                            ALLEGRO_ALIGN_CENTRE,
                            "Resolva um enigma da Sala 2 antes de avancar.");
                    }

                    // Mensagem de "parte restaurada!" ou dica de interacao
                    if (frames_restaurada > 0)
                    {
                        TEXTO(
                            al_map_rgb(120, 255, 120),
                            largura_tela / 2, PY(50),
                            ALLEGRO_ALIGN_CENTRE,
                            "parte restaurada!");
                    }
                    else if (esta_na_zona)
                    {
                        TEXTO(
                            al_map_rgb(255, 255, 255),
                            largura_tela / 2, PY(50),
                            ALLEGRO_ALIGN_CENTRE,
                            "aperte E para interagir com o bau");
                    }
                    else if (esta_na_zona_estante)
                    {
                        TEXTO(
                            al_map_rgb(255, 255, 255),
                            largura_tela / 2, PY(50),
                            ALLEGRO_ALIGN_CENTRE,
                            "aperte E para interagir com a estante");
                    }
                    else if (na_base_escada)
                    {
                        TEXTO(
                            al_map_rgb(255, 255, 255),
                            largura_tela / 2, PY(50),
                            ALLEGRO_ALIGN_CENTRE,
                            "Pressione [CIMA] para subir a escada!");
                    }
                    else if (na_topo_escada)
                    {
                        TEXTO(
                            al_map_rgb(255, 255, 255),
                            largura_tela / 2, PY(50),
                            ALLEGRO_ALIGN_CENTRE,
                            "Pressione [BAIXO] para descer a escada!");
                    }
                    else if (esta_na_zona_sala2)
                    {
                        TEXTO(
                            al_map_rgb(255, 255, 255),
                            largura_tela / 2, PY(50),
                            ALLEGRO_ALIGN_CENTRE,
                            "Pressione [E] para interagir com o livro!");
                    }
                    else if (esta_na_zona_sala2_3)
                    {
                        TEXTO(
                            al_map_rgb(255, 255, 255),
                            largura_tela / 2, PY(50),
                            ALLEGRO_ALIGN_CENTRE,
                            "Pressione [E] para examinar o vaso!");
                    }
                    else if (esta_na_zona_sala3_luz)
                    {
                        TEXTO(
                            al_map_rgb(255, 255, 255),
                            largura_tela / 2, PY(50),
                            ALLEGRO_ALIGN_CENTRE,
                            "Pressione [E] para interagir com a luz!");
                    }
                }

                // Mostra a sala atual
                char texto_sala[32];
                snprintf(texto_sala, sizeof(texto_sala), "Sala %d", sala_atual);

                TEXTO(
                    al_map_rgb(255, 255, 255),
                    PX(10), PY(10),
                    0,
                    texto_sala);

                // Desenha os coracoes de vida (canto superior direito)
                for (int i = 0; i < vidas; i++)
                {
                    float hx = PX(700) + (i * ES(25));
                    float hy = PY(15);

                    desenha_coracao(hx, hy, esc, al_map_rgb(255, 0, 0));
                }

                // ---------------- TELA DO QUIZ ----------------

                if (quiz_aberto == 1)
                {

                    float jx1 = PX(60);
                    float jy1 = PY(60);
                    float jx2 = PX(740);
                    float jy2 = PY(540);

                    // Escurece o fundo
                    al_draw_filled_rectangle(
                        0, 0,
                        largura_tela, altura_tela,
                        al_map_rgba(0, 0, 0, 150));

                    // Painel do quiz
                    al_draw_filled_rectangle(
                        jx1, jy1, jx2, jy2,
                        al_map_rgb(40, 40, 60));

                    al_draw_rectangle(
                        jx1, jy1, jx2, jy2,
                        al_map_rgb(255, 255, 255),
                        ES(3));

                    // Titulo
                    TEXTO(
                        al_map_rgb(255, 220, 80),
                        (jx1 + jx2) / 2, jy1 + PY(20),
                        ALLEGRO_ALIGN_CENTRE,
                        "QUIZ");

                    // Perguntas
                    if (quiz_atual == 1)
                    {
                        TEXTO(al_map_rgb(255, 255, 255),
                              (jx1 + jx2) / 2, jy1 + PY(60),
                              ALLEGRO_ALIGN_CENTRE,
                              "Em uma manhã, o sol simplesmente desapareceu");

                        TEXTO(al_map_rgb(255, 255, 255),
                              (jx1 + jx2) / 2, jy1 + PY(82),
                              ALLEGRO_ALIGN_CENTRE,
                              "e uma chuva de pedras e cinzas cobriu a cidade,");

                        TEXTO(al_map_rgb(255, 255, 255),
                              (jx1 + jx2) / 2, jy1 + PY(104),
                              ALLEGRO_ALIGN_CENTRE,
                              "congelando a vida dos moradores por quase dois mil anos.");

                        TEXTO(al_map_rgb(255, 255, 255),
                              (jx1 + jx2) / 2, jy1 + PY(126),
                              ALLEGRO_ALIGN_CENTRE,
                              "O que causou isso?");
                    }
                    else if (quiz_atual == 2)
                    {
                        TEXTO(al_map_rgb(255, 255, 255),
                              (jx1 + jx2) / 2, jy1 + PY(60),
                              ALLEGRO_ALIGN_CENTRE,
                              "Dois imperios superpoderosos investiram");

                        TEXTO(al_map_rgb(255, 255, 255),
                              (jx1 + jx2) / 2, jy1 + PY(82),
                              ALLEGRO_ALIGN_CENTRE,
                              "bilhoes de dolares e reuniram os melhores cientistas");

                        TEXTO(al_map_rgb(255, 255, 255),
                              (jx1 + jx2) / 2, jy1 + PY(104),
                              ALLEGRO_ALIGN_CENTRE,
                              "para chegar a um lugar onde ninguem jamais esteve.");

                        TEXTO(al_map_rgb(255, 255, 255),
                              (jx1 + jx2) / 2, jy1 + PY(126),
                              ALLEGRO_ALIGN_CENTRE,
                              "Que disputa foi essa?");
                    }
                    else if (quiz_atual == 3)
                    {
                        TEXTO(al_map_rgb(255, 255, 255),
                              (jx1 + jx2) / 2, jy1 + PY(60),
                              ALLEGRO_ALIGN_CENTRE,
                              "Em 1914, um acontecimento envolvendo o herdeiro");

                        TEXTO(al_map_rgb(255, 255, 255),
                              (jx1 + jx2) / 2, jy1 + PY(82),
                              ALLEGRO_ALIGN_CENTRE,
                              "do Imperio Austro-Hungaro contribuiu para o inicio");

                        TEXTO(al_map_rgb(255, 255, 255),
                              (jx1 + jx2) / 2, jy1 + PY(104),
                              ALLEGRO_ALIGN_CENTRE,
                              "de um grande conflito na Europa.");

                        TEXTO(al_map_rgb(255, 255, 255),
                              (jx1 + jx2) / 2, jy1 + PY(126),
                              ALLEGRO_ALIGN_CENTRE,
                              "O que mudou o rumo da historia mundial?");
                    }
                    else if (quiz_atual == 4)
                    {
                        TEXTO(al_map_rgb(255, 255, 255),
                              (jx1 + jx2) / 2, jy1 + PY(60),
                              ALLEGRO_ALIGN_CENTRE,
                              "Um exercito fingiu desistir de uma guerra de dez anos");

                        TEXTO(al_map_rgb(255, 255, 255),
                              (jx1 + jx2) / 2, jy1 + PY(82),
                              ALLEGRO_ALIGN_CENTRE,
                              "e deixou um grande presente na porta da cidade inimiga.");

                        TEXTO(al_map_rgb(255, 255, 255),
                              (jx1 + jx2) / 2, jy1 + PY(104),
                              ALLEGRO_ALIGN_CENTRE,
                              "Os moradores aceitaram o presente e comemoraram,");

                        TEXTO(al_map_rgb(255, 255, 255),
                              (jx1 + jx2) / 2, jy1 + PY(126),
                              ALLEGRO_ALIGN_CENTRE,
                              "mas naquela noite a cidade foi destruida.");

                        TEXTO(al_map_rgb(255, 255, 255),
                              (jx1 + jx2) / 2, jy1 + PY(148),
                              ALLEGRO_ALIGN_CENTRE,
                              "O que aconteceu?");
                    }
                    else if (quiz_atual == 5)
                    {
                        TEXTO(al_map_rgb(255, 255, 255),
                              (jx1 + jx2) / 2, jy1 + PY(60),
                              ALLEGRO_ALIGN_CENTRE,
                              "A maior e mais luxuosa embarcacao de sua epoca,");

                        TEXTO(al_map_rgb(255, 255, 255),
                              (jx1 + jx2) / 2, jy1 + PY(82),
                              ALLEGRO_ALIGN_CENTRE,
                              "considerada praticamente inquebravel, afundou");

                        TEXTO(al_map_rgb(255, 255, 255),
                              (jx1 + jx2) / 2, jy1 + PY(104),
                              ALLEGRO_ALIGN_CENTRE,
                              "em sua viagem inaugural, matando mais de 1.500");

                        TEXTO(al_map_rgb(255, 255, 255),
                              (jx1 + jx2) / 2, jy1 + PY(126),
                              ALLEGRO_ALIGN_CENTRE,
                              "pessoas em aguas congelantes, mesmo apos avisos");

                        TEXTO(al_map_rgb(255, 255, 255),
                              (jx1 + jx2) / 2, jy1 + PY(148),
                              ALLEGRO_ALIGN_CENTRE,
                              "de perigo na rota. O que aconteceu?");
                    }

                    // Alternativas
                    const char **alternativas_atuais = alternativas;

                    if (quiz_atual == 1)
                        alternativas_atuais = alternativas;
                    else if (quiz_atual == 2)
                        alternativas_atuais = alternativas2;
                    else if (quiz_atual == 3)
                        alternativas_atuais = alternativas3;
                    else if (quiz_atual == 4)
                        alternativas_atuais = alternativas4;
                    else if (quiz_atual == 5)
                        alternativas_atuais = alternativas5;

                    for (int i = 0; i < 4; i++)
                    {

                        float oy = jy1 + PY(180) + i * PY(45);

                        ALLEGRO_COLOR cor_texto =
                            al_map_rgb(200, 200, 200);

                        if (i == opcao_selecionada)
                        {

                            // Cor do destaque: azul normal, verde se acertou, vermelho se errou
                            ALLEGRO_COLOR cor_destaque = al_map_rgb(70, 70, 120);

                            if (frames_mensagem > 0)
                            {
                                cor_destaque = ultima_resposta_correta
                                                   ? al_map_rgb(40, 120, 60)
                                                   : al_map_rgb(140, 40, 40);
                            }

                            al_draw_filled_rectangle(
                                jx1 + PX(30), oy - PY(8),
                                jx2 - PX(30), oy + PY(22),
                                cor_destaque);

                            cor_texto =
                                al_map_rgb(255, 255, 255);
                        }

                        TEXTO(
                            cor_texto,
                            jx1 + PX(50), oy,
                            0,
                            alternativas_atuais[i]);
                    }

                    // Mensagem de resposta (aparece por 2 segundos)
                    if (frames_mensagem > 0)
                    {

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
                            mensagem);
                    }

                    // Instrucoes (somem enquanto a mensagem esta na tela)
                    else
                    {
                        TEXTO(
                            al_map_rgb(180, 180, 180),
                            (jx1 + jx2) / 2, jy2 - PY(35),
                            ALLEGRO_ALIGN_CENTRE,
                            "[CIMA/BAIXO] escolher  [ENTER] responder  [E/ESC] fechar");
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

    for (int i = 0; i < TOTAL_SPRITES; i++)
    {
        al_destroy_bitmap(sprites[i]);
    }

    al_destroy_font(fonte);
    al_destroy_bitmap(imagem_floresta);
    al_destroy_bitmap(imagem_carta);
    al_destroy_bitmap(imagem_envelope);
    al_destroy_bitmap(imagem_sala2);
    al_destroy_bitmap(imagem_sala1);
    al_destroy_bitmap(imagem_sala3);
    al_destroy_bitmap(imagem_game_over);
    al_destroy_bitmap(imagem_vitoria);
    al_destroy_event_queue(fila);
    al_destroy_timer(timer);
    al_destroy_display(janela);

    return 0;
}
