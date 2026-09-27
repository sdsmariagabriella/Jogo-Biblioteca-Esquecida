#include <stdio.h>
#include <allegro5/allegro.h>

int main(void)
{
    if (!al_init())
    {
        printf("Erro ao iniciar o Allegro!\n");
        return 1;
    }

    printf("Allegro funcionando!\n");

    return 0;
}