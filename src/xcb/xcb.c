#include <stdio.h>
#include <stdlib.h>

#include <caml/alloc.h>
#include <caml/memory.h>
#include <caml/mlvalues.h>
#include <caml/threads.h>

#include <xcb/xcb.h>
#include <xcb/xcb_event.h>
#include <xcb/xproto.h>

static xcb_connection_t *conn;
static xcb_screen_t *root_screen;
static xcb_window_t root_window;

void grab_mouse_buttons(void) {
  xcb_grab_button(conn, 0, root_window,
                  XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
                  XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, root_window,
                  XCB_NONE, 1 /* left  button */, XCB_MOD_MASK_1);

  xcb_grab_button(conn, 0, root_window,
                  XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
                  XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, root_window,
                  XCB_NONE, 2 /* middle  button */, XCB_MOD_MASK_1);

  xcb_grab_button(conn, 0, root_window,
                  XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
                  XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, root_window,
                  XCB_NONE, 3 /* right  button */, XCB_MOD_MASK_1);
}

// By registering for substructure redirection we are basically telling
// the X server that we are going to be a window manager, which will make us
// receive maprequests and other events that only window managers can receive
xcb_generic_error_t *setup_event_mask(void) {
  uint32_t mask[1] = {XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |   // destroy notify
                      XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT}; // map request

  xcb_void_cookie_t cookie = xcb_change_window_attributes_checked(
      conn, root_window, XCB_CW_EVENT_MASK, mask);

  return xcb_request_check(conn, cookie);
}

char *check_connection(xcb_connection_t *connection) {
  int error = xcb_connection_has_error(connection);

  if (error != 0) {
    switch (error) {
    case XCB_CONN_ERROR:
      return "socket, pipe or other stream errors";
    case XCB_CONN_CLOSED_EXT_NOTSUPPORTED:
      return "extension not supported";
    case XCB_CONN_CLOSED_MEM_INSUFFICIENT:
      return "insufficient memmory";
    case XCB_CONN_CLOSED_REQ_LEN_EXCEED:
      return "the request length that the server accepts was exceeded";
    case XCB_CONN_CLOSED_PARSE_ERR:
      return "unable to parse display string";
    case XCB_CONN_CLOSED_INVALID_SCREEN:
      return "the server does not have a screen matching the display";
    case XCB_CONN_CLOSED_FDPASSING_FAILED:
      return "some FD passing operation failed";
    default:
      return "unknown error";
    }
  } else {
    return NULL;
  }
}

// init connects to X, sets the root_screen and root_window globals, and then it
// performs other setup procedures. It returns an error message or NULL if there
// was no error
char *init(void) {
  conn = xcb_connect(NULL, NULL);

  char *connection_error = check_connection(conn);
  if (connection_error != NULL) {
    return connection_error;
  }

  root_screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
  root_window = root_screen->root;

  grab_mouse_buttons();

  xcb_generic_error_t *error = setup_event_mask();
  if (error != NULL) {
    free(error);

    return "unable to setup event mask, another window manager might be "
           "running";
  }

  return NULL;
}

// TODO: Maybe consider moving the OCaml stuff to a different file?
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
CAMLprim value Val_xcb_event(xcb_generic_event_t *event) {
  CAMLparam0();
  CAMLlocal1(ret);

  uint8_t event_response_type = XCB_EVENT_RESPONSE_TYPE(event);
  switch (event_response_type) {
  case XCB_MAP_REQUEST:;
    // cast generic event to the proper event
    xcb_map_request_event_t *map_event = (xcb_map_request_event_t *)event;

    // alloc MapRequest(window)
    ret = caml_alloc(1, 1);
    Store_field(ret, 0, Val_int(map_event->window));

    break;
  case XCB_CONFIGURE_REQUEST:;
    xcb_configure_request_event_t *configure_event =
        (xcb_configure_request_event_t *)event;

    // So far from what we could see, some applications (xterm as an example)
    // trigger CONFIGURE_REQUEST and then only trigger MAP_REQUEST immediately
    // if we call xcb_configure_window and xcb_flush here. If we don't call
    // those the MAP_REQUEST take forever to trigger, regardless of
    // whether or not we call xcb_flush
    const uint32_t values[2] = {configure_event->width,
                                configure_event->height};
    xcb_configure_window(conn, configure_event->window,
                         XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                         values);
    xcb_flush(conn);

    // alloc Unknown(id)
    ret = caml_alloc(1, 0);
    Store_field(ret, 0, Val_int(event_response_type)); // id
    break;
  default:
    // alloc Unknown(id)
    ret = caml_alloc(1, 0);
    Store_field(ret, 0, Val_int(event_response_type)); // id
  }

  CAMLreturn(ret);
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

CAMLprim value rexcb_root_screen(void) {
  CAMLparam0();
  CAMLlocal1(ret);

  // alloc screen = { width: int, height: int }
  ret = caml_alloc(2, 0);
  Store_field(ret, 0, Val_int(root_screen->width_in_pixels));
  Store_field(ret, 1, Val_int(root_screen->height_in_pixels));

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

CAMLprim value rexcb_init(void) {
  CAMLparam0();
  CAMLlocal1(ret);

  char *error_message = init();
  if (error_message != NULL) {
    ret = Val_error(error_message);

    free(error_message);
    xcb_disconnect(conn);
  } else {
    ret = Val_ok(Val_unit);
  }

  CAMLreturn(ret);
}
