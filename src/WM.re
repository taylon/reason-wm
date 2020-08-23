Timber.App.enable();
Timber.App.setLevel(Timber.Level.debug);

module Log = (val Timber.Log.withNamespace("WM"));

Log.debug("Starting...");

XCB.init();
let event = XCB.waitForEvent();

Log.debugf(m => m("Event: %i", event));
