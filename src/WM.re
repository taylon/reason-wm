Timber.App.enable();
Timber.App.setLevel(Timber.Level.trace);

module Log = (val Timber.Log.withNamespace("WM"));

Log.debug("Starting...");

// ---------
let exitWM = () => {
  Log.trace("Exiting the window manager...");

  XCB.disconnect();
  exit(1);
};

let exitSignalHandler = Sys.Signal_handle(signal => exitWM());
Sys.set_signal(Sys.sigint, exitSignalHandler);
Sys.set_signal(Sys.sigterm, exitSignalHandler);
// ---------

let eventHandler = event =>
  switch (event) {
  | XCB.Event.MapRequest(window) =>
    XCB.Window.show(window);
    XCB.Window.resize(window, ~height=300, ~width=400);
    XCB.Window.move(window, ~x=400, ~y=300);
    Log.tracef(m => m("X11 Event - MapRequest for: %i", window));

  | XCB.Event.Unknown(id) => Log.tracef(m => m("X11 Event - Unknown: %i", id))
  };

XCB.init();
XCB.runEventLoop(eventHandler);
