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
let openedWindows: ref(list(XCB.Window.t)) = ref([]);

let registerWindow = window => openedWindows := [window, ...openedWindows^];

let deregisterWindow = windowID =>
  openedWindows :=
    List.filter(
      (openedWindow: XCB.Window.t) => openedWindow.id != windowID,
      openedWindows^,
    );

// -------

// Window management stuff ---------
let reArrangeWindows = () => {
  open XCB;
  open XCB.Window;

  let rootScreen = XCB.rootScreen();

  let numberOfWindowsOpen = List.length(openedWindows^);
  let windowWidth =
    numberOfWindowsOpen == 0
      ? rootScreen.width : rootScreen.width / numberOfWindowsOpen;

  let xPosition = index => index == 0 ? 0 : windowWidth * index;

  List.iteri(
    (index, window) => {
      Window.resize(window.id, ~width=windowWidth, ~height=rootScreen.height);

      Window.move(window.id, ~x=xPosition(index), ~y=0);
    },
    openedWindows^,
  );
};

let arrangeForUltrawide = openedWindows => {
  open XCB;
  open XCB.Window;

  let rootScreen = XCB.rootScreen();

  let height = rootScreen.height;
  let width = numberOfWindows =>
    numberOfWindows == 0
      ? rootScreen.width : rootScreen.width / numberOfWindows;

  let resizeEqualy = windows =>
    List.iter(
      window =>
        Window.resize(
          window.id,
          ~width=width(List.length(windows)),
          ~height,
        ),
      windows,
    );

  let splitInThree = (primary, secondary, terciary) => {
    resizeEqualy([primary, secondary, terciary]);

    let xPosition = index => width(3) * index;
    let yPosition = 0;

    Window.move(secondary.id, ~x=0, ~y=0);
    Window.move(primary.id, ~x=xPosition(1), ~y=yPosition);
    Window.move(terciary.id, ~x=xPosition(2), ~y=yPosition);
  };

  let splitEqualy = reArrangeWindows;

  switch (openedWindows) {
  | [primary, secondary, terciary] =>
    splitInThree(primary, secondary, terciary)

  | _ =>
    splitEqualy();
    Log.debugf(m => m("splitting equaly"));
  };
};

type application =
  | Browser
  | Vim
  | Terminal;

let focusOn = className => {
  open XCB.Window;

  let otherWindows =
    List.filter(
      openedWindow => openedWindow.className != className,
      openedWindows^,
    );

  let windowToFocus =
    List.find(
      openedWindow => openedWindow.className == className,
      openedWindows^,
    );

  arrangeForUltrawide([windowToFocus, ...otherWindows]);
};
// ---------

// XCB ---------
let eventHandler = event =>
  XCB.(
    switch (event) {
    | Event.MapRequest(window) =>
      registerWindow(window);
      reArrangeWindows();

      Window.show(window.id);

      Log.tracef(m => m("X11 Event - MapRequest for: %s", window.className));

    | Event.DestroyNotify(windowID) =>
      Log.debugf(m => m("Destroy %i", windowID));

      deregisterWindow(windowID);
      reArrangeWindows();

    | Event.KeyPress(modifiers, key, windowID) =>
      switch (modifiers) {
      | [Keyboard.Control, Keyboard.Mask_1] =>
        Log.tracef(m => m("closing window: %i", windowID));
        Window.close(windowID);

      | [] =>
        switch (key) {
        | 72 => focusOn("kitty") // leader
        | 96 => focusOn("XTerm") // file nav
        | _ => Log.debug("Can't handle this key")
        }

      | _ => Log.debug("nem rolou")
      }

    /* | Event.Unknown(id) => Log.tracef(m => m("X11 Event - Unknown: %i", id)) */
    | Event.Unknown(id) => ()
    }
  );

XCB.runEventLoop(eventHandler, exitWM);
