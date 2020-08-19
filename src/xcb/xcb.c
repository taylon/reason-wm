#include <stdlib.h>

#include <caml/alloc.h>
#include <caml/bigarray.h>
#include <caml/callback.h>
#include <caml/memory.h>
#include <caml/mlvalues.h>
#include <xcb/xcb.h>

xcb_connection_t *conn;

/* void grab_mouse_buttons(xcb_window_t *window) { */
/*   xcb_grab_button(conn, 0, *window, */
/*                   XCB_EVENT_MASK_BUTTON_PRESS |
 * XCB_EVENT_MASK_BUTTON_RELEASE, */
/*                   XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, *window,
 * XCB_NONE, */
/*                   1 #<{(| left  button |)}>#, XCB_MOD_MASK_1); */
/*  */
/*   xcb_grab_button(conn, 0, *window, */
/*                   XCB_EVENT_MASK_BUTTON_PRESS |
 * XCB_EVENT_MASK_BUTTON_RELEASE, */
/*                   XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, *window,
 * XCB_NONE, */
/*                   2 #<{(| middle  button |)}>#, XCB_MOD_MASK_1); */
/*  */
/*   xcb_grab_button(conn, 0, *window, */
/*                   XCB_EVENT_MASK_BUTTON_PRESS |
 * XCB_EVENT_MASK_BUTTON_RELEASE, */
/*                   XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, *window,
 * XCB_NONE, */
/*                   3 #<{(| right  button |)}>#, XCB_MOD_MASK_1); */
/* } */
/*  */
/* // Substructure redirection is what is going to allow us to */
/* // receive the messages we need from X to become a Window Manager */
/* void setup_substructure_redirection(xcb_window_t *root_window) { */
/*   unsigned int mask[1] = {XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | // destroy
 * notify */
/*                           XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT}; // map
 * request */
/*  */
/*   xcb_void_cookie_t cookie = xcb_change_window_attributes_checked( */
/*       conn, *root_window, XCB_CW_EVENT_MASK, mask); */
/*  */
/*   xcb_flush(conn); */
/*  */
/*   xcb_generic_error_t *error = xcb_request_check(conn, cookie); */
/*   if (error != NULL) { */
/*     xcb_disconnect(conn); */
/*     free(error); */
/*   } */
/* } */
/*  */
/* void setup(void) { */
/*   conn = xcb_connect(NULL, NULL); */
/*   if (xcb_connection_has_error(conn)) { */
/*     xcb_disconnect(conn); */
/*  */
/*     printf("unable to initiate connection with X server\n"); */
/*     exit(EXIT_FAILURE); */
/*   } */
/*  */
/*   xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
 */
/*   xcb_window_t *root_window = &screen->root; */
/*  */
/*   screen_w = screen->width_in_pixels; */
/*   screen_h = screen->height_in_pixels; */
/*  */
/*   grab_mouse_buttons(root_window); */
/*   setup_substructure_redirection(root_window); */
/* } */
/*  */
/* void map_request(xcb_generic_event_t *generic_event) { */
/*   printf("map request\n"); */
/*  */
/*   xcb_map_request_event_t *event = (xcb_map_request_event_t *)generic_event;
 */
/*  */
/*   xcb_map_window(conn, event->window); */
/* } */
/*  */
/* #define XCB_CONFIG_MOVE XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y */
/* #define XCB_CONFIG_RESIZE XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT
 */
/* #define XCB_CONFIG_MOVE_RESIZE XCB_CONFIG_RESIZE | XCB_CONFIG_MOVE */
/*  */
/* void configure_request(xcb_generic_event_t *generic_event) { */
/*   printf("configure request\n"); */
/*  */
/*   xcb_configure_request_event_t *event = */
/*       (xcb_configure_request_event_t *)generic_event; */
/*  */
/*   unsigned int values[4] = {0, 0, screen_w, screen_h}; */
/*   xcb_configure_window(conn, event->window, XCB_CONFIG_MOVE_RESIZE, values);
 */
/* } */
/*  */
/* static void (*event_handlers[XCB_NO_OPERATION])(xcb_generic_event_t *) = { */
/*     [XCB_CONFIGURE_REQUEST] = configure_request, */
/*     [XCB_MAP_REQUEST] = map_request, */
/* }; */
/*  */
/* void run(void) { */
/*   xcb_generic_event_t *event; */
/*  */
/*   for (;;) { */
/*     if (xcb_connection_has_error(conn)) { */
/*       exit(EXIT_FAILURE); */
/*     } */
/*  */
/*     if ((event = xcb_wait_for_event(conn))) */
/*       if (event_handlers[event->response_type & ~0x80]) { */
/*         event_handlers[event->response_type & ~0x80](event); */
/*         xcb_flush(conn); */
/*       } */
/*  */
/*     free(event); */
/*   } */
/* } */

void open_window() {
  /* Open the connection to the X server */
  conn = xcb_connect(NULL, NULL);

  /* Get the first screen */
  const xcb_setup_t *setup = xcb_get_setup(conn);
  xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
  xcb_screen_t *screen = iter.data;

  /* Create the window */
  xcb_window_t window = xcb_generate_id(conn);
  xcb_create_window(conn,                          /* Connection          */
                    XCB_COPY_FROM_PARENT,          /* depth (same as root)*/
                    window,                        /* window Id           */
                    screen->root,                  /* parent window       */
                    0, 0,                          /* x, y                */
                    150, 150,                      /* width, height       */
                    10,                            /* border_width        */
                    XCB_WINDOW_CLASS_INPUT_OUTPUT, /* class               */
                    screen->root_visual,           /* visual              */
                    0, NULL);                      /* masks, not used yet */

  /* Map the window on the screen */
  xcb_map_window(conn, window);

  /* Make sure commands are sent before we pause so that the window gets shown
   */
  xcb_flush(conn);
}

CAMLprim value rexcb_open_window() {
  CAMLparam0();

  open_window();
  sleep(1);

  CAMLreturn(Val_unit);
}
