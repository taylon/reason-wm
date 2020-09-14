Timber.App.enable();
Timber.App.setLevel(Timber.Level.trace);

module Log = (val Timber.Log.withNamespace("Test"));

let runCommand = (cmd, args) =>
  Unix.create_process(cmd, args, Unix.stdin, Unix.stdout, Unix.stderr);

let killApp = pid => Unix.kill(pid, Sys.sigterm);

let displayID = ":1";

// --------
let xephyrPID =
  runCommand("Xephyr", [|"-ac", "-screen", "1700x1080", displayID|]);

// Just to make sure Xephyr is available before we do anything else
Unix.sleepf(0.5);

let killXephyr = () => killApp(xephyrPID);
// ---------

// This is going to be used implcitly by all X Clients to redirect
// everything to the same display used by Xephyr
Unix.putenv("DISPLAY", displayID);

// ----------
let wmPID = runCommand(Sys.argv[1] ++ "/WM", [||]);
Unix.sleepf(1.5);

let killWM = () => killApp(wmPID);
// ----------

let testWith = (apps: list(string)) => {
  let pids = List.map(app => runCommand(app, [||]), apps);

  Unix.sleep(60);

  killWM();
  List.iter(killApp, pids);

  Unix.sleepf(0.5);
  killXephyr();
};

testWith(["pavucontrol", "kitty", "xterm"]);
// testWith(["pavucontrol"]);
// testWith(["epiphany"]);
