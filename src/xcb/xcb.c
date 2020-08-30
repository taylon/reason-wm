#include <stdio.h>
#include <stdlib.h>

#include <caml/alloc.h>
#include <caml/memory.h>
#include <caml/mlvalues.h>
#include <caml/threads.h>

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xcb_event.h>

// TODO: get rid of these, they don't need to be global...
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

// TODO: Maybe consider declaring values in a different file?
#define Val_none Val_int(0)

static value Val_some(value v) {
  CAMLparam1(v);
  CAMLlocal1(some);

  some = caml_alloc(1, 0);
  Store_field(some, 0, v);

  CAMLreturn(some);
}

static value Val_ok(value v) {
  CAMLparam1(v);
  CAMLlocal1(ok);

  ok = caml_alloc(1, 0);
  Store_field(ok, 0, v);

  CAMLreturn(ok);
}

static value Val_error(char *const message) {
  CAMLparam0();
  CAMLlocal1(error);

  error = caml_alloc(1, 1);
  Store_field(error, 0, caml_copy_string(message));

  CAMLreturn(error);
}

// t in `ret = caml_alloc(n, t)` must map to the correct variant in
// XCB.Event.t
//
// https://caml.inria.fr/pub/docs/manual-ocaml/intfc.html#ss:c-block-allocation
CAMLprim value Val_xcb_event(xcb_generic_event_t * event) {
  CAMLparam0();
  CAMLlocal1(ret);

	uint8_t event_response_type = XCB_EVENT_RESPONSE_TYPE(event);
  switch (event_response_type) {
  case XCB_MAP_REQUEST:;
    // cast generic event to the proper event
    xcb_map_request_event_t *map_event =
        (xcb_map_request_event_t *)event;

    // alloc MapRequest(window)
    ret = caml_alloc(1, 1);
    Store_field(ret, 0, Val_int(map_event->window));

    break;
  default:
    // alloc Unknown(id)
    ret = caml_alloc(1, 0);
    Store_field(ret, 0, Val_int(event_response_type)); // id
  }

  CAMLreturn(ret);
}

CAMLprim value rexcb_root_screen(void) {
  CAMLparam0();
  CAMLlocal1(ret);

  xcb_screen_t *screen = root_screen();

  // alloc screen = { width: int, height: int }
  ret = caml_alloc(2, 0);
  Store_field(ret, 0, Val_int(screen->width_in_pixels));
  Store_field(ret, 1, Val_int(screen->height_in_pixels));

  CAMLreturn(ret);
}

CAMLprim value rexcb_resize_window(value window_id, value height, value width) {
  CAMLparam3(window_id, height, width);

  const uint32_t values[] = {Int_val(width), Int_val(height)};
  xcb_configure_window(conn, Int_val(window_id),
                       XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                       values);

  CAMLreturn(Val_unit);
}

CAMLprim value rexcb_move_window(value window_id, value x, value y) {
  CAMLparam3(window_id, x, y);

  const uint32_t values[] = {Int_val(x), Int_val(y)};
  xcb_configure_window(conn, Int_val(window_id),
                       XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);

  CAMLreturn(Val_unit);
}

CAMLprim value rexcb_wait_for_event(void) {
  CAMLparam0();
  CAMLlocal1(ret);

  caml_release_runtime_system();
  xcb_generic_event_t *event = xcb_wait_for_event(conn);
  caml_acquire_runtime_system();

  ret = Val_some(Val_xcb_event(event));

  free(event);

  CAMLreturn(ret);
}

// TODO: Respect override_redirect
CAMLprim value rexcb_show_window(value window_id) {
  CAMLparam1(window_id);

  xcb_map_window(conn, Int_val(window_id));
  xcb_flush(conn);

  CAMLreturn(Val_unit);
}

CAMLprim value rexcb_disconnect(void) {
  CAMLparam0();

  xcb_disconnect(conn);

  CAMLreturn(Val_unit);
}

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
