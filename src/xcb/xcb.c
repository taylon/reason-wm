#include <stdio.h>
#include <stdlib.h>

/* #include <caml/alloc.h> */
/* #include <caml/bigarray.h> */
/* #include <caml/callback.h> */
/* #include <caml/memory.h> */
/* #include <caml/mlvalues.h> */
#include <xcb/xcb.h>
#include <xcb/xproto.h>

xcb_connection_t *xcb_connection() {
  static xcb_connection_t *conn;

  if (conn == NULL) {
    conn = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(conn)) {
      xcb_disconnect(conn);
    }
  }

  return conn;
}

xcb_screen_t *root_screen() {
  xcb_connection_t *conn = xcb_connection();

  return xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
}

xcb_window_t *root_window() {
  static xcb_window_t *root_window;

  if (root_window == NULL) {
    xcb_screen_t *screen = root_screen();
    root_window = &screen->root;
  }

  return root_window;
}

void grab_mouse_buttons() {
  xcb_connection_t *conn = xcb_connection();
  xcb_window_t *window = root_window();

  xcb_grab_button(conn, 0, *window,
                  XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
                  XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, *window, XCB_NONE,
                  1 /* left  button */, XCB_MOD_MASK_1);

  xcb_grab_button(conn, 0, *window,
                  XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
                  XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, *window, XCB_NONE,
                  2 /* middle  button */, XCB_MOD_MASK_1);

  xcb_grab_button(conn, 0, *window,
                  XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
                  XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, *window, XCB_NONE,
                  3 /* right  button */, XCB_MOD_MASK_1);
}

// Substructure redirection is makes us a Window Manager
xcb_generic_error_t *setup_substructure_redirection() {
  xcb_connection_t *conn = xcb_connection();
  xcb_window_t *window = root_window();

  // TODO: we can probably remove the notify mask here, not removing it now
  // because I can't test it atm
  unsigned int mask[1] = {XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | // destroy notify
                          XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT}; // map request

  xcb_void_cookie_t cookie = xcb_change_window_attributes_checked(
      conn, *window, XCB_CW_EVENT_MASK, mask);

  xcb_flush(conn);

  return xcb_request_check(conn, cookie);
  /* xcb_generic_error_t *error = xcb_request_check(conn, cookie); */
  /* if (error != NULL) { */
  /*   xcb_disconnect(conn); */
  /*   free(error); */
  /* } */
}

xcb_generic_error_t *init(void) {
  xcb_connection_t *conn = xcb_connection();

  grab_mouse_buttons();

  xcb_generic_error_t *error = setup_substructure_redirection();
  if (error != NULL) {
    xcb_disconnect(conn);
    // XXX: pay attention to this when making the ocaml wrapper
    /* free(error); */
  }

  return error;
}

void map_request(xcb_generic_event_t *generic_event) {
  xcb_connection_t *conn = xcb_connection();

  printf("map request\n");

  xcb_map_request_event_t *event = (xcb_map_request_event_t *)generic_event;

  xcb_map_window(conn, event->window);
}

void configure_request(xcb_generic_event_t *generic_event) {
  printf("configure request\n");

  xcb_connection_t *conn = xcb_connection();

  xcb_configure_request_event_t *event =
      (xcb_configure_request_event_t *)generic_event;

  xcb_screen_t *screen = root_screen();
  uint16_t screen_w = screen->width_in_pixels;
  uint16_t screen_h = screen->height_in_pixels;

  unsigned int values[4] = {0, 0, screen_w, screen_h};
  xcb_configure_window(conn, event->window,
                       XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT |
                           XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
                       values);
}

typedef void (*event_handler_t)(xcb_generic_event_t *);

void run_event_loop(event_handler_t event_handler) {
  xcb_connection_t *conn = xcb_connection();

  for (;;) {
    if (xcb_connection_has_error(conn)) {
      printf("connection error: we are currently not handling errors so "
             "check "
             "the xcb binding code!\n");
    }

    xcb_generic_event_t *event;
    if ((event = xcb_wait_for_event(conn))) {
      event_handler(event);

      xcb_flush(conn);
    }

    free(event);
  }
}

static void (*event_handlers[XCB_NO_OPERATION])(xcb_generic_event_t *) = {
    [XCB_CONFIGURE_REQUEST] = configure_request,
    [XCB_MAP_REQUEST] = map_request,
};

void handle_event(xcb_generic_event_t *event) {
  if (event_handlers[event->response_type & ~0x80]) {
    event_handlers[event->response_type & ~0x80](event);
  }
}

int main() {
  init();

  printf("Running!\n");
  run_event_loop(handle_event);
}

/* CAMLprim value rexcb_init() { */
/*   CAMLparam0(); */
/*  */
/*   init(); */
/*   sleep(1); */
/*  */
/*   CAMLreturn(Val_unit); */
/* } */
