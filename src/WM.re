Timber.App.enable();
Timber.App.setLevel(Timber.Level.trace);

module Log = (val Timber.Log.withNamespace("WM"));

Log.debug("Starting...");

let exitWM = () => {
  Log.trace("Exiting the window manager...");

  XCB.disconnect();
  exit(1);
};

let exitSignalHandler = Sys.Signal_handle(signal => exitWM());

Sys.set_signal(Sys.sigint, exitSignalHandler);
Sys.set_signal(Sys.sigterm, exitSignalHandler);

let eventHandler = event =>
  switch (event) {
  | XCB.Event.MapRequest({windowID}) =>
    XCB.mapWindow(windowID);
    Log.tracef(m => m("X11 Event - MapRequest for: %i", windowID));

  | XCB.Event.Unknown(id) => Log.tracef(m => m("X11 Event - Unknown: %i", id))
  };

XCB.init();
XCB.runEventLoop(eventHandler);
