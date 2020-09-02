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
// ---------

// Window management stuff ---------
let openedWindows = ref([]);
let registerWindow = window => openedWindows := [window, ...openedWindows^];

let reArrangeWindows = () => {
  open XCB;

  let rootScreen = XCB.rootScreen();

  let numberOfWindowsOpen = List.length(openedWindows^);
  let windowWidth = rootScreen.width / numberOfWindowsOpen;
  let xPosition = index => index == 0 ? 0 : windowWidth * index;

  List.iteri(
    (index, window) => {
      Window.resize(window, ~width=windowWidth, ~height=rootScreen.height);

      Window.move(window, ~x=xPosition(index), ~y=0);
    },
    openedWindows^,
  );
};
// ---------

// ---------
let eventHandler = event =>
  XCB.(
    switch (event) {
    | Event.MapRequest(window) =>
      registerWindow(window);
      reArrangeWindows();

      Window.show(window);

      Log.tracef(m => m("X11 Event - MapRequest for: %i", window));

    | Event.KeyPress(modifiers, key) =>
      switch (modifiers) {
      | [Keyboard.Control, Keyboard.Mask_1] =>
        Log.debug("programming is pure joy =D")
      | _ => Log.debug("nem rolou")
      }

    /* | Event.Unknown(id) => Log.tracef(m => m("X11 Event - Unknown: %i", id)) */
    | Event.Unknown(id) => ()
    }
  );

let start = () => {
  switch (XCB.init()) {
  | Ok(_) => Log.debug("Connection with X11 was established")
  | Error(message) =>
    Log.errorf(m =>
      m("Unable to establish connection with X11: %s", message)
    );

    exitWM();
  };

  XCB.runEventLoop(eventHandler);
};

start();
