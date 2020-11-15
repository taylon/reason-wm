open Lwt_unix;

module Log = (val Timber.Log.withNamespace("Client"));

let socketFilePath = "/tmp/reason-wm-server.sock";
let address = ADDR_UNIX(socketFilePath);

let socketInfo = {
  let socket = socket(PF_UNIX, SOCK_STREAM, 0);

  let%lwt _ = Lwt_unix.connect(socket, address);

  let ic = Lwt_io.of_fd(~mode=Input, socket);
  let oc = Lwt_io.of_fd(~mode=Output, socket);

  Lwt.return((ic, oc));
};

let create = () => {
  let rec sendMessage = ((ic, oc)) => {
    /* let%lwt _ = Lwt_io.print("Enter input: "); */
    /* let%lwt line = Lwt_io.read_line(Lwt_io.stdin); */
    let%lwt _ = Lwt_io.write_line(oc, "Hello");
    let%lwt line = Lwt_io.read_line(ic);
    let%lwt _ = Lwt_io.printlf("Server sent: %s", line);
    /* let%lwt _ = Lwt_io.printlf("Server sent: %s", line); */

    sendMessage((ic, oc));
  };

  let%lwt _ = Lwt_io.printl("Connecting to server on socket");
  let%lwt info = socketInfo;

  sendMessage(info);
};

Lwt_main.run(create());

/* open Unix; */

/* module Log = (val Timber.Log.withNamespace("Client")); */

/* let address = ADDR_UNIX("/tmp/reason-wm-server.sock"); */

/* let sock = { */
/*   let fd = socket(PF_UNIX, SOCK_STREAM, 0); */
/*   connect(fd, address); */

/*   fd; */
/* }; */

/* let sendMessage = message => { */
/*   let bytes = Bytes.of_string(message); */

/*   Log.debugf(m => m("sending %s", message)); */

/*   send(sock, bytes, 0, Bytes.length(bytes), []); */
/* }; */

/* sendMessage("Hi!\n"); */
