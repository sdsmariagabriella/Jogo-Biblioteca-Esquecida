#include <stdio.h>
#include <allegro5/allegro.h>

int main() {
    printf("A tentar iniciar o Allegro...\n");
    fflush(stdout);

    if (!al_init()) {
        printf("Falha ao inicializar o Allegro!\n");
        fflush(stdout);
        return -1;
    }

    printf("Allegro iniciado com sucesso! A abrir a janela...\n");
    fflush(stdout);

    ALLEGRO_DISPLAY* display = al_create_display(640, 480);
    if (!display) {
        printf("Falha ao criar a janela do Allegro!\n");
        fflush(stdout);
        return -1;
    }

    al_rest(3.0);
    al_destroy_display(display);

    return 0;
}