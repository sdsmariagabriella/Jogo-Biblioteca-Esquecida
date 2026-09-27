#include <stdio.h>
#include <stdbool.h>

#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro.h>

int main(void){
    //inicializar o Allegro
    if(!al_init()){
        printf("Erro ao iniciar o Allegro!\n");
        return 1;
    }
    //inicializar o teclado 
    if(!al_install_keyboard()){
        printf("erro no teclado");
        return 1;
    }
    //iniciar  as primitivas (formas geometricas)
    if(!al_init_primitives_addon()){
        printf("erro nas primitivas");
        return 1;
    }

    //Criar uma janela de 800 * 600 pixels
    ALLEGRO_DISPLAY *janela = al_create_display(800, 600);

    if (!janela){
        printf("Erro na janela\n");
        return 1;
    }

    //definir o tutulo da janela 
    al_set_window_title(janela,("Jogo de teste"));

    //Criar uma fila de eventos
    ALLEGRO_EVENT_QUEUE *fila = al_create_event_queue();

    //registrar a fila de eventos para o teclado
    al_register_event_source(fila, al_get_keyboard_event_source());
    al_register_event_source(fila, al_get_display_event_source(janela))
;

    //posiçao do objeto (personagem)
    float x = 375;
    float y = 275;

    //velocidade do objeto (personagem)
    float velo = 5;
    bool rodando = true;

    while(rodando){
        ALLEGRO_EVENT evento;

        al_wait_for_event(fila, &evento);

        //Se apertar umas da teclas 
        if(evento.type == ALLEGRO_EVENT_KEY_DOWN){

            //ESQUEURDA
            if (evento.keyboard.keycode == ALLEGRO_KEY_LEFT){
                x -= velo;
            }

            //direita
            if (evento.keyboard.keycode == ALLEGRO_KEY_RIGHT){
                x += velo;
            }

            // ESC fecha o jogo
            if (evento.keyboard.keycode == ALLEGRO_KEY_ESCAPE)
            {
                rodando = false;
            }
        }

        //Se apertar o botao de fechar a janela
        if (evento.type == ALLEGRO_EVENT_DISPLAY_CLOSE){
           rodando = false;
        }

        //limpar a tela
        al_clear_to_color(al_map_rgb(0,0,0));

        //desenhar no quadro
        al_draw_filled_rectangle(
            x,y,
            x + 50, y +50,
            al_map_rgb(200,200,0)
        );

        //atualizar a tela
        al_flip_display();

       
    }
     //liberar a memoria do evento
    al_destroy_event_queue(fila);
    al_destroy_display(janela);


    //manter a janela aberta
    //al_rest(5.0);

    //fechar a janela
    //al_destroy_display(janela);
    return 0;
}