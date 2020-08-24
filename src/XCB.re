external init: unit => unit = "rexcb_init";

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
