#include <stdio.h>
#include <stdlib.h>

#include <caml/memory.h>
#include <caml/threads.h>

#include <xcb/xcb.h>
#include <xcb/xproto.h>

static xcb_connection_t *conn;
static xcb_window_t *root_window;

xcb_screen_t *root_screen(void) {
  return xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
}

void grab_mouse_buttons(void) {
  xcb_grab_button(conn, 0, *root_window,
                  XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
                  XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, *root_window,
                  XCB_NONE, 1 /* left  button */, XCB_MOD_MASK_1);

  xcb_grab_button(conn, 0, *root_window,
                  XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
                  XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, *root_window,
                  XCB_NONE, 2 /* middle  button */, XCB_MOD_MASK_1);

  xcb_grab_button(conn, 0, *root_window,
                  XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
                  XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, *root_window,
                  XCB_NONE, 3 /* right  button */, XCB_MOD_MASK_1);
}

// Substructure redirection is makes us a Window Manager
xcb_generic_error_t *setup_substructure_redirection(void) {
  // TODO: we can probably remove the notify mask here, not removing it now
  // because I can't test it atm
  unsigned int mask[1] = {XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | // destroy notify
                          XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT}; // map request

  xcb_void_cookie_t cookie = xcb_change_window_attributes_checked(
      conn, *root_window, XCB_CW_EVENT_MASK, mask);

  xcb_flush(conn);

  return xcb_request_check(conn, cookie);
  /* xcb_generic_error_t *error = xcb_request_check(conn, cookie); */
  /* if (error != NULL) { */
  /*   xcb_disconnect(conn); */
  /*   free(error); */
  /* } */
}

CAMLprim value Val_xcb_event(xcb_generic_event_t *event) {
  CAMLparam0();
  CAMLlocal1(ret);

  switch (event->response_type) {
  case XCB_CONFIGURE_REQUEST:
    ret = Val_int(0);
    break;
  case XCB_MAP_REQUEST:
    ret = Val_int(1);
    break;
  default:
    ret = Val_int(2);
  }

  CAMLreturn(ret);
}

CAMLprim value rexcb_wait_for_event(void) {
  CAMLparam0();
  CAMLlocal1(ret);

  caml_release_runtime_system();
  xcb_generic_event_t *event = xcb_wait_for_event(conn);
  caml_acquire_runtime_system();

  ret = Val_xcb_event(event);

  CAMLreturn(ret);
}

/* typedef void (*event_handler_t)(xcb_generic_event_t *); */

/* void run_event_loop(event_handler_t event_handler) { */
/*   for (;;) { */
/*     if (xcb_connection_has_error(conn)) { */
/*       printf("connection error: we are currently not handling errors so " */
/*              "check " */
/*              "the xcb binding code!\n"); */
/*     } */
/*  */
/*     xcb_generic_event_t *event; */
/*     if ((event = xcb_wait_for_event(conn))) { */
/*       event_handler(event); */
/*  */
/*       xcb_flush(conn); */
/*     } */
/*  */
/*     free(event); */
/*   } */
/* } */

xcb_generic_error_t *init(void) {
  conn = xcb_connect(NULL, NULL);
  if (xcb_connection_has_error(conn)) {
    xcb_disconnect(conn);
  }

  xcb_screen_t *screen = root_screen();
  root_window = &screen->root;

  // conn and root_window need to be initialized before
  // anything else happens!
  grab_mouse_buttons();

  xcb_generic_error_t *error = setup_substructure_redirection();
  if (error != NULL) {
    xcb_disconnect(conn);
    // XXX: pay attention to this when working on the ocaml wrapper
    /* free(error); */
  }

  return error;
}

CAMLprim value rexcb_init(void) {
  CAMLparam0();

  init();

  CAMLreturn(Val_unit);
}

void map_request(xcb_generic_event_t *generic_event) {
  printf("map request\n");

  xcb_map_request_event_t *event = (xcb_map_request_event_t *)generic_event;

  xcb_map_window(conn, event->window);
}

void configure_request(xcb_generic_event_t *generic_event) {
  printf("configure request\n");

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

/* static void (*event_handlers[XCB_NO_OPERATION])(xcb_generic_event_t *) = { */
/*     [XCB_CONFIGURE_REQUEST] = configure_request, */
/*     [XCB_MAP_REQUEST] = map_request, */
/* }; */
/*  */
/* void handle_event(xcb_generic_event_t *event) { */
/*   if (event_handlers[event->response_type & ~0x80]) { */
/*     event_handlers[event->response_type & ~0x80](event); */
/*   } */
/* } */

/* int main() { */
/*   init(); */
/*  */
/*   printf("Running!\n"); */
/*   run_event_loop(handle_event); */
/* } */

/* CAMLprim value rexcb_init() { */
/*   CAMLparam0(); */
/*  */
/*   init(); */
/*   sleep(1); */
/*  */
/*   CAMLreturn(Val_unit); */
/* } */
