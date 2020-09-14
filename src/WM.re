let () = Printexc.record_backtrace(true);

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

let arrangeWindows = windows => {
  open XCB;
  open XCB.Window;

  let rootScreen = XCB.rootScreen();

  let numberOfWindows = List.length(windows);
  let windowWidth =
    numberOfWindows == 0
      ? rootScreen.width : rootScreen.width / numberOfWindows;

  let xPosition = index => index == 0 ? 0 : windowWidth * index;

  List.iteri(
    (index, window) => {
      Window.resize(window.id, ~width=windowWidth, ~height=rootScreen.height);
      Window.move(window.id, ~x=xPosition(index), ~y=0);
    },
    windows,
  );

  openedWindows := windows;
};

let openWindow = window => {
  arrangeWindows([window, ...openedWindows^]);
  XCB.Window.show(window.id);
};

let closeWindow = id =>
  openedWindows^
  |> List.filter((openedWindow: XCB.Window.t) => openedWindow.id != id)
  |> arrangeWindows;

let findWindow = className =>
  List.find_opt(
    (openedWindow: XCB.Window.t) => openedWindow.className == className,
    openedWindows^,
  );

// -------

let focusOn = className => {
  open XCB.Window;

  Log.tracef(m => m("focusing on %s", className));

  let browser = findWindow("XTerm");
  let vim = findWindow(".");
  let terminal = findWindow("kitty");

  let windows =
    switch (findWindow(className)) {
    | Some(windowToFocus) =>
      switch (windowToFocus.className) {
      | "kitty" => [browser, terminal, vim]
      | "XTerm" => [vim, browser, terminal]
      | "." => [browser, vim, terminal]
      | _ => [Some(windowToFocus), browser, vim, terminal]
      }
    | None => [browser, vim, terminal]
    };

  arrangeWindows(
    windows |> List.filter(Option.is_some) |> List.map(Option.get),
  );
};

// XCB ---------
let eventHandler = event =>
  XCB.(
    switch (event) {
    | Event.MapRequest(window) =>
      Log.tracef(m => m("X11 Event - MapRequest for: %s", window.className));

      openWindow(window);

    | Event.DestroyNotify(id) =>
      Log.debugf(m => m("X11 Event - DestroyNotify for: %i", id));

      closeWindow(id);

    | Event.KeyPress(modifiers, key, windowID) =>
      switch (modifiers) {
      | [Keyboard.Control, Keyboard.Mask_1] =>
        Log.tracef(m => m("closing window: %i", windowID));

        Window.close(windowID);

      | [] =>
        switch (key) {
        | 68 => focusOn("XTerm") // F2
        | 69 => focusOn(".") // F3
        | 70 => focusOn("kitty") // F4
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
