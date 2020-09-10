Timber.App.enable();
Timber.App.setLevel(Timber.Level.trace);

module Log = (val Timber.Log.withNamespace("WM"));

Log.debug("Starting...");

// Signal Handling ---------
let exitWM = () => {
  Log.trace("Exiting the window manager...");

  XCB.disconnect();
  exit(0);
};

let exitSignalHandler = Sys.Signal_handle(signal => exitWM());
Sys.set_signal(Sys.sigint, exitSignalHandler);
Sys.set_signal(Sys.sigterm, exitSignalHandler);
Sys.set_signal(Sys.sighup, exitSignalHandler);
Sys.set_signal(Sys.sigquit, exitSignalHandler);
Sys.set_signal(Sys.sigabrt, exitSignalHandler);
// ---------

// We can't wait to get the API right, but it is not time yet! ---------
let openedWindows = ref([]);

let registerWindow = window => openedWindows := [window, ...openedWindows^];

let deregisterWindow = window =>
  openedWindows :=
    List.filter(openedWindow => openedWindow != window, openedWindows^);

// -------

// Window management stuff ---------
let reArrangeWindows = () => {
  open XCB;

  let rootScreen = XCB.rootScreen();

  let numberOfWindowsOpen = List.length(openedWindows^);
  let windowWidth =
    numberOfWindowsOpen == 0
      ? rootScreen.width : rootScreen.width / numberOfWindowsOpen;

  let xPosition = index => index == 0 ? 0 : windowWidth * index;

  List.iteri(
    (index, window) => {
      Window.resize(window, ~width=windowWidth, ~height=rootScreen.height);

      Window.move(window, ~x=xPosition(index), ~y=0);
    },
    openedWindows^,
  );
};

let arrangeForUltrawide = openedWindows => {
  open XCB;

  let rootScreen = XCB.rootScreen();

  let height = rootScreen.height;
  let width = numberOfWindows =>
    numberOfWindows == 0
      ? rootScreen.width : rootScreen.width / numberOfWindows;

  let resizeEqualy = windows =>
    List.iter(
      window =>
        Window.resize(window, ~width=width(List.length(windows)), ~height),
      windows,
    );

  let splitInThree = (primary, secondary, terciary) => {
    resizeEqualy([primary, secondary, terciary]);

    let xPosition = index => width(3) * index;
    let yPosition = 0;

    Window.move(secondary, ~x=0, ~y=0);
    Window.move(primary, ~x=xPosition(1), ~y=yPosition);
    Window.move(terciary, ~x=xPosition(2), ~y=yPosition);
  };

  let splitEqualy = reArrangeWindows;

  switch (openedWindows) {
  | [primary, secondary, terciary] =>
    splitInThree(primary, secondary, terciary);
    Log.debugf(m =>
      m("splitting in three: %i - %i - %i", primary, secondary, terciary)
    );

  | [primary, secondary] =>
    splitEqualy();
    Log.debugf(m => m("splitting in two: %i - %i", primary, secondary));

  | _ =>
    splitEqualy();
    Log.debugf(m => m("splitting equaly"));
  };
};

let focusOn = window => {
  let otherWindows =
    List.filter(openedWindow => openedWindow != window, openedWindows^);

  arrangeForUltrawide([window, ...otherWindows]);
};
// Dirty experiment ---------

// XCB ---------
let eventHandler = event =>
  XCB.(
    switch (event) {
    | Event.MapRequest(window) =>
      registerWindow(window);
      reArrangeWindows();

      Window.show(window);

      Log.tracef(m => m("X11 Event - MapRequest for: %i", window));

    | Event.DestroyNotify(window) =>
      Log.debugf(m => m("Destroy %i", window));

      deregisterWindow(window);
      reArrangeWindows();

    | Event.KeyPress(modifiers, key, window) =>
      switch (modifiers) {
      | [Keyboard.Control, Keyboard.Mask_1] =>
        Log.tracef(m => m("closing window: %i", window));
        Window.close(window);

      | [] =>
        switch (key) {
        | 72 => focusOn(4194326)
        | 96 => focusOn(8388621)
        | _ => Log.debug("Can't handle this key")
        }

      | _ => Log.debug("nem rolou")
      }

    /* | Event.Unknown(id) => Log.tracef(m => m("X11 Event - Unknown: %i", id)) */
    | Event.Unknown(id) => ()
    }
  );

XCB.runEventLoop(eventHandler, exitWM);
