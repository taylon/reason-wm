external init: unit => unit = "rexcb_init";

type window = int;

module Event = {
  type mapRequest = {window};

  type t =
    | MapRequest(mapRequest);
};

// TODO: replace option with result once we know what we doing
external waitForEvent: unit => option(Event.t) = "rexcb_wait_for_event";
