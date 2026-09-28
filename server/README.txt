phong-server
============

Rooms for P(H)ONG over the internet and the shared high scores of
endless and squash. The game's lobby opens a room with "Online open",
the others join it with its code, set the server under Settings, Online.

    ./phong-server --port 45460 --scores scores.json

--port    the port to listen on, 45460 by default
--scores  the file keeping the high scores, without it they're lost
          when the server stops
--lag     holds every message back for that many milliseconds, to try
          the game with a slow connection

The Qt libraries it needs are in lib/. From the system it takes glib and
Kerberos, on Debian and Ubuntu libglib2.0-0 and libgssapi-krb5-2, which
nearly every installation has.

It speaks plain WebSockets, ws://. The browser version on an HTTPS page
can only reach a server over TLS, wss://, so a public server goes behind
a proxy that adds it, e.g. Caddy:

    phong.example.com {
        reverse_proxy localhost:45460
    }

and the players set wss://phong.example.com as the server.
