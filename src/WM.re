Timber.App.enable();
Timber.App.setLevel(Timber.Level.trace);

module Log = (val Timber.Log.withNamespace("WM"));

Log.debug("Starting...");

let handleEvent = event =>
  switch (event) {
  | XCB.Event.MapRequest({windowID}) =>
    XCB.mapWindow(windowID);
    Log.tracef(m => m("X11 Event - MapRequest for: %i", windowID));

  | XCB.Event.Unknown(id) => Log.tracef(m => m("X11 Event - Unknown: %i", id))
  };

let runEventLoop = () => {
  let processingEvents = ref(true);

  while (processingEvents^) {
    switch (XCB.waitForEvent()) {
    | None => Log.debug("No event!!")
    | Some(event) => handleEvent(event)
    };
  };
};

XCB.init();
runEventLoop();
