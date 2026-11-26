#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <stdbool.h>

typedef struct {
    float x, y, w, h;
} Objeto;

static bool ponto_dentro(Objeto* o, float px, float py) {
    return px >= o->x && px <= o->x + o->w &&
        py >= o->y && py <= o->y + o->h;
}

int main(void) {
    al_init();
    al_init_primitives_addon();
    al_install_mouse();

    ALLEGRO_DISPLAY* display = al_create_display(800, 600);
    ALLEGRO_EVENT_QUEUE* fila = al_create_event_queue();
    al_register_event_source(fila, al_get_display_event_source(display));
    al_register_event_source(fila, al_get_mouse_event_source());

    Objeto caixa = { 100, 100, 80, 80 };
    bool arrastando = false;
    float offset_x = 0, offset_y = 0;
    bool rodando = true;

    while (rodando) {
        ALLEGRO_EVENT ev;
        al_wait_for_event(fila, &ev);

        switch (ev.type) {
        case ALLEGRO_EVENT_DISPLAY_CLOSE:
            rodando = false;
            break;

        case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
            if (ev.mouse.button == 1 && ponto_dentro(&caixa, ev.mouse.x, ev.mouse.y)) {
                arrastando = true;
                offset_x = ev.mouse.x - caixa.x;
                offset_y = ev.mouse.y - caixa.y;
            }
            break;

        case ALLEGRO_EVENT_MOUSE_BUTTON_UP:
            if (ev.mouse.button == 1)
                arrastando = false;
            break;

        case ALLEGRO_EVENT_MOUSE_AXES:
            if (arrastando) {
                caixa.x = ev.mouse.x - offset_x;
                caixa.y = ev.mouse.y - offset_y;
            }
            break;
        }

        if (al_is_event_queue_empty(fila)) {
            al_clear_to_color(al_map_rgb(30, 30, 30));
            al_draw_filled_rectangle(caixa.x, caixa.y,
                caixa.x + caixa.w, caixa.y + caixa.h,
                arrastando ? al_map_rgb(255, 180, 0)
                : al_map_rgb(0, 160, 255));
            al_flip_display();
        }
    }

    al_destroy_event_queue(fila);
    al_destroy_display(display);
    return 0;
}