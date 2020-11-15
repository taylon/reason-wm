open Lwt_unix;

module Log = (val Timber.Log.withNamespace("Server"));

let socketFilePath = "/tmp/reason-wm-server.sock";
let address = ADDR_UNIX(socketFilePath);

let createClient = () => {
  let socketInfo = {
    let socket = socket(PF_UNIX, SOCK_STREAM, 0);

    let%lwt _ = Lwt_unix.connect(socket, address);

    let ic = Lwt_io.of_fd(~mode=Input, socket);
    let oc = Lwt_io.of_fd(~mode=Output, socket);

    Lwt.return((ic, oc));
  };

  let rec sendMessage = ((ic, oc)) => {
    /* let%lwt _ = Lwt_io.print("Enter input: "); */
    /* let%lwt line = Lwt_io.read_line(Lwt_io.stdin); */
    let%lwt _ = Lwt_io.write_line(oc, "Hello");
    let%lwt line = Lwt_io.read_line(ic);
    Lwt_io.printlf("Server sent: %s", line);
    /* let%lwt _ = Lwt_io.printlf("Server sent: %s", line); */
    /* sendMessage((ic, oc)); */
  };
  let%lwt _ = Lwt_io.printl("Connecting to server on socket");
  let%lwt info = socketInfo;
  sendMessage(info);
};

let createServer = () => {
  let echoHandler = (_, (ic, oc)) => {
    let rec echo = () => {
      switch%lwt (Lwt_io.read_line_opt(ic)) {
      | Some(s) =>
        let%lwt _ = Lwt_io.(printlf("Client sent: %s", s));
        let%lwt _ = Lwt_io.write_line(oc, s);
        echo();
      | None => Lwt.return_unit
      };
    };
    echo();
  };

  if (Sys.file_exists(socketFilePath)) {
    Sys.remove(socketFilePath);
  };

  let%lwt _server =
    Lwt_io.establish_server_with_client_address(
      address,
      echoHandler,
      ~no_close=true,
    );

  Log.debug("Server started!");

  Lwt.return_unit;
};

let client = {
  let%lwt _ = sleep(0.1);
  createClient();
};

/* let run = () => Lwt_main.run(Lwt.join([createServer(), client])); */
let run = () => Lwt_main.run(createServer());

// Stoped when thinking that maybe I should just use the simpler version
// using just recvfrom instead of above. I'm wondering if
// the fact that recvfrom blocks is a problem, I don't think so
// because I'll have to wrap the x11 backend with lwt anyway...
//-----------------------------------------

/* open Lwt_unix; */
/* module Log = (val Timber.Log.withNamespace("Server")); */
/* let address = ADDR_UNIX(socketFilePath); */
/* let fd = socket(PF_UNIX, SOCK_DGRAM, 0); */
/* let receiveMessage = () => { */
/*   let buff = Bytes.create(8192); */
/*   Log.debug("waiting"); */
/*   let%lwt (size, _) = recvfrom(fd, buff, 0, Bytes.length(buff), []); */
/*   Log.debugf(m => */
/*     m("received %s", String.sub(Bytes.to_string(buff), 0, size)) */
/*   ); */
/*   Lwt.return_unit; */
/* }; */
/* bind(fd, address); */
/* let run = () => Lwt_main.run(receiveMessage()); */
