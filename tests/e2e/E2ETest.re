Timber.App.enable();
Timber.App.setLevel(Timber.Level.trace);

module Log = (val Timber.Log.withNamespace("Test"));

let runCommand = (cmd, args) =>
  Unix.create_process(cmd, args, Unix.stdin, Unix.stdout, Unix.stderr);

let displayID = ":1";

// --------
let xephyrPID =
  runCommand("Xephyr", [|"-ac", "-screen", "1700x1080", displayID|]);

// Just to make sure Xephyr is available before we do anything else
Unix.sleepf(0.5);
// ---------

// This is going to be used implcitly by the following commands to redirect
// everything to the same display used by Xephyr
Unix.putenv("DISPLAY", displayID);

let wmPID = runCommand(Sys.argv[1] ++ "/WM", [||]);
Unix.sleepf(1.5);

let kittyPID = runCommand("kitty", [||]);
let kitty2PID = runCommand("kitty", [||]);
let kitty3PID = runCommand("kitty", [||]);

Unix.sleep(1);

Unix.kill(wmPID, Sys.sigterm);
Unix.kill(kittyPID, Sys.sigterm);
Unix.kill(kitty2PID, Sys.sigterm);
Unix.kill(kitty3PID, Sys.sigterm);

Unix.sleepf(0.5);
Unix.kill(xephyrPID, Sys.sigterm);
