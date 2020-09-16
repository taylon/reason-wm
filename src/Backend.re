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

  // --------
  let currentlyOpened: ref(list(t)) = ref([]);
  let show = (window: XCB.Window.t) => {
    currentlyOpened := [windowFromXCB(window), ...currentlyOpened^];

    XCB.Window.show(window.id);
  };

  let close = id => {
    XCB.Window.close(id);

    currentlyOpened :=
      List.filter(window => window.id != id, currentlyOpened^);
  };
  // --------

  let find = predicate => List.find_opt(predicate, currentlyOpened^);

  let resize = ({id, dimensions: {width, height}}: t) =>
    XCB.Window.resize(id, ~width, ~height);

  let move = ({id, position: {x, y}}: t) => XCB.Window.move(id, ~x, ~y);
};

let draw = windows =>
  List.iter(
    window => {
      Window.resize(window);
      Window.move(window);
    },
    windows,
  );

let eventHandler = (arrangeWindows, focusOn, event) =>
  switch (event) {
  | XCB.Event.MapRequest(window) =>
    Log.tracef(m => m("X11 Event - MapRequest for: %s", window.className));

    Window.show(window);

    draw(arrangeWindows(Window.currentlyOpened^));

  | XCB.Event.DestroyNotify(id) =>
    Log.debugf(m => m("X11 Event - DestroyNotify for: %i", id));

    Window.close(id);

  | XCB.Event.KeyPress(modifiers, key, windowID) =>
    switch (modifiers) {
    | [XCB.Keyboard.Control, XCB.Keyboard.Mask_1] =>
      Log.tracef(m => m("closing window: %i", windowID));

      Window.close(windowID);

      draw(arrangeWindows(Window.currentlyOpened^));

    | [] =>
      let newArrangement =
        switch (key) {
        | 68 => focusOn("XTerm") // F2
        | 69 => focusOn(".") // F3
        | 70 => focusOn("kitty") // F4
        | 72 => focusOn("kitty") // leader
        | 96 => focusOn("XTerm") // file nav
        | _ => arrangeWindows(Window.currentlyOpened^)
        };

      draw(newArrangement);

    | _ => Log.debug("nem rolou")
    }

  /* | Event.Unknown(id) => Log.tracef(m => m("X11 Event - Unknown: %i", id)) */
  | XCB.Event.Unknown(id) => ()
  };

let runEventLoop = (arrangeWindows, focusOn) =>
  XCB.runEventLoop(eventHandler(arrangeWindows, focusOn));
