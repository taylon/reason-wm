module Log = (val Timber.Log.withNamespace("XCB"));

external init: unit => unit = "rexcb_init";
external disconnect: unit => unit = "rexcb_disconnect";

type windowID = int;

external mapWindow: windowID => unit = "rexcb_map_window";

module Event = {
  type mapRequest = {windowID};

  type t =
    | Unknown(int) // 0
    | MapRequest(mapRequest); // 1
};

// TODO: replace option with result once we know what we doing
external waitForEvent: unit => option(Event.t) = "rexcb_wait_for_event";

let runEventLoop = eventHandler =>
  while (true) {
    switch (waitForEvent()) {
    | None => Log.debug("No event!!")
    | Some(event) => eventHandler(event)
    };
  };
