Timber.App.enable();
Timber.App.setLevel(Timber.Level.debug);

module Log = (val Timber.Log.withNamespace("WM"));

Log.debug("Starting...");

XCB.openWindow();
