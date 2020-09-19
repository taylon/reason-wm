module Log = (val Timber.Log.withNamespace("Backend"));

switch (XCB.init()) {
| Ok(_) => Log.debug("Connection with X11 was established")
| Error(message) =>
  Log.errorf(m => m("Unable to establish connection with X11: %s", message))
/* exitWM(); */
};

module Display = {
  let screen = XCB.rootScreen();

  let height = screen.height;
  let width = screen.width;
};

module Window = {
  type position = {
    x: int,
    y: int,
  };

  type dimensions = {
    width: int,
    height: int,
  };

  type t = {
    id: int,
    dimensions,
    position,
    hidden: bool,
    app: string,
  };

  let windowFromXCB = (window: XCB.Window.t) => {
    id: window.id,
    hidden: false,

    // TODO: fallback to instanceName if className not found?
    app: window.className,

    position: {
      x: 0,
      y: 0,
    },

    dimensions: {
      width: Display.width,
      height: Display.height,
    },
  };

  let show = (window: XCB.Window.t) => {
    XCB.Window.show(window.id);

    windowFromXCB(window);
  };

  let close = XCB.Window.close;

  let resize = ({id, dimensions: {width, height}}: t) =>
    XCB.Window.resize(id, ~width, ~height);

  let move = ({id, position: {x, y}}: t) => XCB.Window.move(id, ~x, ~y);
};

let draw =
  List.iter(window => {
    Window.resize(window);
    Window.move(window);
  });

let eventHandler = (~windows, ~arrangeWindows, ~focusOn, ~event) =>
  switch (event) {
  | XCB.Event.MapRequest(window) =>
    Log.tracef(m => m("X11 Event - MapRequest for: %s", window.className));

    [Window.show(window), ...windows] |> arrangeWindows;

  | XCB.Event.DestroyNotify(windowID) =>
    Log.debugf(m => m("X11 Event - DestroyNotify for: %i", windowID));

    Window.close(windowID);

    List.filter((window: Window.t) => window.id != windowID, windows)
    |> arrangeWindows;

  | XCB.Event.KeyPress(modifiers, key, windowID) =>
    switch (modifiers) {
    | [XCB.Keyboard.Control, XCB.Keyboard.Mask_1] =>
      Log.tracef(m => m("closing window: %i", windowID));

      Window.close(windowID);

      List.filter((window: Window.t) => window.id != windowID, windows)
      |> arrangeWindows;

    | [] =>
      let focusOnWindowRunning = focusOn(windows);

      switch (key) {
      | 68 => focusOnWindowRunning("XTerm") // F2
      | 69 => focusOnWindowRunning(".") // F3
      | 70 => focusOnWindowRunning("kitty") // F4
      | 72 => focusOnWindowRunning("kitty") // leader
      | 96 => focusOnWindowRunning("XTerm") // file nav
      | _ => windows
      };

    | _ => windows
    }

  /* | Event.Unknown(id) => Log.tracef(m => m("X11 Event - Unknown: %i", id)) */
  | XCB.Event.Unknown(id) => windows
  };

let rec run = (~windows=[], arrangeWindows, focusOn) =>
  switch (XCB.Event.wait()) {
  | None => Log.trace("No event!!")
  | Some(event) =>
    let currentWindows =
      eventHandler(~windows, ~arrangeWindows, ~focusOn, ~event);

    draw(currentWindows);

    run(~windows=currentWindows, arrangeWindows, focusOn);
  };
