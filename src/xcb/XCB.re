module Log = (val Timber.Log.withNamespace("XCB"));

external init: unit => result(unit, string) = "rexcb_init";
external disconnect: unit => unit = "rexcb_disconnect";

type screen = {
  width: int,
  height: int,
};

external rootScreen: unit => screen = "rexcb_root_screen";

module Window = {
  type t = int;

  external show: t => unit = "rexcb_show_window";

  external resize: (t, ~height: int, ~width: int) => unit =
    "rexcb_resize_window";

  external close: t => unit = "rexcb_close_window";

  external move: (t, ~x: int, ~y: int) => unit = "rexcb_move_window";
};

module Keyboard = {
  type modifier =
    | Shift // 0
    | Lock // 1
    | Control // 2
    | Mask_1 // 3
    | Mask_2 // 4
    | Mask_3 // 5
    | Mask_4 // 6
    | Mask_5; // 7

  type keycode = int;
};

module Event = {
  type t =
    | Unknown(int) // 0
    | MapRequest(Window.t) // 1
    | DestroyNotify(Window.t) // 2
    | KeyPress(list(Keyboard.modifier), Keyboard.keycode, Window.t); // 3

  external wait: unit => option(t) = "rexcb_wait_for_event";
};

let runEventLoop = (eventHandler, onExit) => {
  switch (init()) {
  | Ok(_) => Log.debug("Connection with X11 was established")
  | Error(message) =>
    Log.errorf(m =>
      m("Unable to establish connection with X11: %s", message)
    );

    onExit();
  };

  while (true) {
    switch (Event.wait()) {
    | None => Log.trace("No event!!")
    | Some(event) => eventHandler(event)
    };
  };
};
