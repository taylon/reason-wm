Timber.App.enable();
Timber.App.setLevel(Timber.Level.debug);

module Log = (val Timber.Log.withNamespace("WM"));

Log.debug("Starting...");

XCB.init();
let event = XCB.waitForEvent();

let handleEvent = event =>
  switch (event) {
  | XCB.Event.MapRequest(data) =>
    Log.debugf(m => m("Window ID: %i", data.window))
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

runEventLoop();
