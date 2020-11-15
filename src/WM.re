let () = Printexc.record_backtrace(true);

Timber.App.enable();
Timber.App.setLevel(Timber.Level.trace);

module Log = (val Timber.Log.withNamespace("WM"));

Log.debug("Starting...");

// Signal Handling ---------
let exitWM = () => {
  Log.info("Exiting...");

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

open Backend;
open Backend.Window;

let arrangeWindows = windows => {
  let numberOfWindows = List.length(windows);

  let windowWidth =
    numberOfWindows == 0 ? Display.width : Display.width / numberOfWindows;

  let xPosition = index => index == 0 ? 0 : windowWidth * index;

  let windowPerIndex = (index, window) => {
    ...window,

    position: {
      x: xPosition(index),
      y: 0,
    },

    dimensions: {
      width: windowWidth,
      height: Display.height,
    },
  };

  List.mapi(windowPerIndex, windows);
};

let focusOn = (allWindows, app) => {
  let findWindow = ListLabels.find_opt(allWindows);

  let browser = findWindow(window => window.app == "XTerm");
  let vim = findWindow(window => window.app == ".");
  let terminal = findWindow(window => window.app == "kitty");

  let windowToFocus = findWindow(window => window.app == app);

  let windows =
    switch (windowToFocus) {
    | Some(windowToFocus) =>
      switch (windowToFocus.app) {
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

Server.run();

Backend.run(arrangeWindows, focusOn);
