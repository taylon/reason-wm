#include <stdio.h>
#include <stdlib.h>

#include <caml/alloc.h>
#include <caml/memory.h>
#include <caml/mlvalues.h>
#include <caml/threads.h>

#include <X11/keysym.h>
#include <xcb/xcb.h>
#include <xcb/xcb_event.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xproto.h>

static xcb_connection_t *conn;
static xcb_screen_t *root_screen;
static xcb_window_t root_window;

typedef struct Key {
  xcb_mod_mask_t mod;
  xcb_keycode_t keycode;
} Key;

static Key keys[3] = {
    {.mod = XCB_MOD_MASK_CONTROL | XCB_MOD_MASK_1, .keycode = XK_q}};

void grab_keys(void) {
  xcb_key_symbols_t *keysyms = xcb_key_symbols_alloc(conn);

  for (int i = 0; i < sizeof(keys) / sizeof(Key); i++) {
    // We only use the first keysymbol, even if there are more.
    xcb_keycode_t *keycode =
        xcb_key_symbols_get_keycode(keysyms, keys[i].keycode);

    // Not doing error handling here because I'm pretty sure
    // that this way of grabbing keys won't for too long, when we get
    // to the point where we care about handling this error  the
    // method for grabbing keys will have changed
    if (keycode != NULL) {
      xcb_grab_key(conn, 1, root_window, keys[i].mod, *keycode,
                   XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    }

    free(keycode);
  }

  xcb_key_symbols_free(keysyms);
}

// By registering for substructure redirection we are basically telling
// the X server that we are going to be a window manager, which will make us
// receive maprequests and other events that only window managers can receive
xcb_generic_error_t *setup_event_mask(void) {
  uint32_t mask[1] = {XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |   // destroy notify
                      XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT}; // map request

  xcb_void_cookie_t cookie = xcb_change_window_attributes_checked(
      conn, root_window, XCB_CW_EVENT_MASK, mask);

  grab_keys();

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

  // TODO: need to return the X11 error code here somehow
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
#define Val_empty_list Val_int(0)

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
  CAMLlocal3(ret, extra_value, extra_value1);

  uint8_t event_response_type = XCB_EVENT_RESPONSE_TYPE(event);

  switch (event_response_type) {
  case XCB_MAP_REQUEST:;
    // cast generic event to the proper event
    xcb_map_request_event_t *map_event = (xcb_map_request_event_t *)event;

    // alloc MapRequest(window)
    ret = caml_alloc(1, 1);
    Store_field(ret, 0, Val_int(map_event->window));

    break;
  case XCB_KEY_PRESS:;
    xcb_key_press_event_t *key_press_event = (xcb_key_press_event_t *)event;

    // alloc list(Keyboard.modifier)
    switch (key_press_event->state) {
    case XCB_MOD_MASK_CONTROL | XCB_MOD_MASK_1:
      // [Mask_1]
      extra_value1 = caml_alloc(2, 0);
      Store_field(extra_value1, 0, Val_int(3)); // Mask_1 (Alt here)
      Store_field(extra_value1, 1, Val_int(0)); // []

      // [Control, Mask_1]
      extra_value = caml_alloc(2, 0);
      Store_field(extra_value, 0, Val_int(2));   // Control
      Store_field(extra_value, 1, extra_value1); // [Mask_1]

      break;
    default:
      extra_value = Val_empty_list;

      break;
    }

    // alloc KeyPress(list(Keyboard.modifier), Keyboard.keycode)
    ret = caml_alloc(2, 2);
    Store_field(ret, 0, extra_value); // list(Keyboard.modifier)
    Store_field(ret, 1, Val_int(key_press_event->detail)); // Keyboard.keycode

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

    xcb_disconnect(conn);
  } else {
    ret = Val_ok(Val_unit);
  }

  CAMLreturn(ret);
}
