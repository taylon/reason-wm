module Log = (val Timber.Log.withNamespace("XCB"));

external init: unit => unit = "rexcb_init";
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

  external move: (t, ~x: int, ~y: int) => unit = "rexcb_move_window";
};

module Event = {
  type t =
    | Unknown(int) // 0
    | MapRequest(Window.t); // 1
};

external waitForEvent: unit => option(Event.t) = "rexcb_wait_for_event";

let runEventLoop = eventHandler => {
  init();

  while (true) {
    switch (waitForEvent()) {
    | None => Log.debug("No event!!")
    | Some(event) => eventHandler(event)
    };
  };
};
