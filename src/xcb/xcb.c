#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/keysym.h>
#include <xcb/xcb.h>
#include <xcb/xcb_event.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xproto.h>

#include <caml/alloc.h>
#include <caml/memory.h>
#include <caml/mlvalues.h>
#include <caml/threads.h>

static xcb_connection_t *conn;
static xcb_screen_t *root_screen;
static xcb_window_t root_window;

xcb_ewmh_connection_t *ewmh;

typedef struct Key {
  xcb_mod_mask_t mod;
  xcb_keysym_t keysym;
} Key;

static Key keys[6] = {
    {.mod = XCB_MOD_MASK_ANY, .keysym = XK_F2},
    {.mod = XCB_MOD_MASK_ANY, .keysym = XK_F3},
    {.mod = XCB_MOD_MASK_ANY, .keysym = XK_F4},
    {.mod = XCB_MOD_MASK_ANY, .keysym = XK_F6},
    {.mod = XCB_MOD_MASK_ANY, .keysym = XK_F12},
    {.mod = XCB_MOD_MASK_CONTROL | XCB_MOD_MASK_1, .keysym = XK_q}};

void grab_keys(void) {
  xcb_key_symbols_t *keysyms = xcb_key_symbols_alloc(conn);

  for (int i = 0; i < sizeof(keys) / sizeof(Key); i++) {
    // We only use the first keysymbol, even if there are more.
    xcb_keycode_t *keycode =
        xcb_key_symbols_get_keycode(keysyms, keys[i].keysym);

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

// TODO: Error handling
bool ewmh_init(void) {
  ewmh = calloc(1, sizeof(xcb_ewmh_connection_t));
  if (xcb_ewmh_init_atoms_replies(ewmh, xcb_ewmh_init_atoms(conn, ewmh),
                                  NULL) == 0) {
    return false;
  }

  xcb_ewmh_set_wm_pid(ewmh, root_window, getpid());
  xcb_ewmh_set_wm_name(ewmh, root_window, 2, "WM");

  // not sure if we need the following

  // took this from bspwm so need to figure out what exactly this means and if
  // it is really necessary
  /* xcb_ewmh_set_supporting_wm_check(ewmh, root_window, win); */
  /* xcb_ewmh_set_supporting_wm_check(ewmh, win, win); */

  // xcb_ewmh_set_supported(...)
}

bool ewmh_window_supports_protocol(xcb_window_t *window, xcb_atom_t atom) {
  xcb_icccm_get_wm_protocols_reply_t protocols;

  bool result = false;
  xcb_get_property_cookie_t cookie =
      xcb_icccm_get_wm_protocols(conn, *window, ewmh->WM_PROTOCOLS);

  if (xcb_icccm_get_wm_protocols_reply(conn, cookie, &protocols, NULL) != 1)
    return false;

  // Check if the client's protocols have the requested atom set
  for (uint32_t i = 0; i < protocols.atoms_len; i++)
    if (protocols.atoms[i] == atom)
      result = true;

  xcb_icccm_get_wm_protocols_reply_wipe(&protocols);

  return result;
}

// TODO: this will be reworked soon
// position on the array must match the position on the enum
static char *wm_atom_names[1] = {"WM_DELETE_WINDOW"};
enum { WM_DELETE_WINDOW, WM_ATOMS_COUNT };
static xcb_atom_t wm_atoms[WM_ATOMS_COUNT];

// TODO: report error when we cannot find one of the atoms
void init_atoms(void) {
  for (int i = 0; i < WM_ATOMS_COUNT; i++) {
    xcb_intern_atom_cookie_t cookie =
        xcb_intern_atom(conn, 0, strlen(wm_atom_names[i]), wm_atom_names[i]);

    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(conn, cookie, NULL);
    if (reply) {
      wm_atoms[i] = reply->atom;

      free(reply);
    }
  }
}

void send_event(xcb_window_t window, xcb_atom_t property, xcb_atom_t value) {
  xcb_client_message_event_t *event = calloc(32, 1);

  event->response_type = XCB_CLIENT_MESSAGE;
  event->window = window;
  event->type = property;
  event->format = 32;
  event->data.data32[0] = value;
  event->data.data32[1] = XCB_CURRENT_TIME;

  xcb_send_event(conn, false, window, XCB_EVENT_MASK_NO_EVENT, (char *)event);
  free(event);
}

void close_window(xcb_window_t window) {
  // if the window doesn't support WM_DELETE_WINDOW, then we just kill it
  if (!ewmh_window_supports_protocol(&window, wm_atoms[WM_DELETE_WINDOW])) {
    xcb_kill_client(conn, window);
  } else {
    send_event(window, ewmh->WM_PROTOCOLS, wm_atoms[WM_DELETE_WINDOW]);
  }

  xcb_flush(conn);
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

  ewmh_init();
  // TODO: handle error in init_atoms
  init_atoms();

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

// x in `ret = caml_alloc(y, x)` must map to the correct variant in
// XCB.Event.t
//
// https://caml.inria.fr/pub/docs/manual-ocaml/intfc.html#ss:c-block-allocation
CAMLprim value Val_xcb_event(xcb_generic_event_t *event) {
  CAMLparam0();
  CAMLlocal3(ret, extra_value, extra_value1);

  uint8_t event_response_type = XCB_EVENT_RESPONSE_TYPE(event);

  // TODO: We need to clean this all up, put the handling
  // of each case into it's own function otherwise handling
  // the CAMLlocals becames a mess
  switch (event_response_type) {
  case XCB_MAP_REQUEST:;
    xcb_map_request_event_t *map_event = (xcb_map_request_event_t *)event;

    // alloc Window.t
    extra_value = caml_alloc(3, 0);
    Store_field(extra_value, 0, Val_int(map_event->window)); // id

    xcb_icccm_get_wm_class_reply_t reply;
    if (xcb_icccm_get_wm_class_reply(
            conn, xcb_icccm_get_wm_class_unchecked(conn, map_event->window),
            &reply, NULL) == 1) {
      Store_field(extra_value, 1, caml_copy_string(reply.class_name));
      Store_field(extra_value, 2, caml_copy_string(reply.instance_name));

      xcb_icccm_get_wm_class_reply_wipe(&reply);
    }

    // alloc MapRequest(Window.t)
    ret = caml_alloc(1, 1);
    Store_field(ret, 0, extra_value); // Window.t

    break;

  case XCB_DESTROY_NOTIFY:;
    xcb_destroy_notify_event_t *destroy_event =
        (xcb_destroy_notify_event_t *)event;

    // alloc DestroyNotify(Window.id)
    ret = caml_alloc(1, 2);
    Store_field(ret, 0, Val_int(destroy_event->window));

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

    // alloc KeyPress(list(Keyboard.modifier), Keyboard.keycode, Window.id)
    ret = caml_alloc(3, 3);
    Store_field(ret, 0, extra_value); // list(Keyboard.modifier)
    Store_field(ret, 1, Val_int(key_press_event->detail)); // Keyboard.keycode

    // TODO: could child be NULL if there was no window open?
    Store_field(ret, 2, Val_int(key_press_event->child)); // Window.id

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

    // For now we do not forward these up
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

CAMLprim value rexcb_close_window(value window) {
  CAMLparam1(window);

  close_window(Int_val(window));

  CAMLreturn(Val_unit);
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
  xcb_flush(conn);

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

  xcb_ewmh_connection_wipe(ewmh);
  free(ewmh);

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
