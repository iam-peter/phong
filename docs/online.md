# Playing over the internet

Games over the internet and the shared high scores of endless and squash go
through `phong-server`. A host opens a room there and gets a four letter
code, the others join with the code, and the server passes the messages
between them. The host runs the game like on the LAN. The server also keeps
the ten best scores of endless and squash.

This page is about where that server runs and how the game finds it.

## How the game finds the server

The server isn't built into the game. The game reads
[`online.json`](../online.json) in this repository:

```json
{ "server": "wss://phong-server.onrender.com" }
```

- **Moving the server** to another host: put the new address there.
- **Switching internet play off** for everybody: `{ "server": "" }`. The
  online parts of the lobby, the join screen and the results then don't
  show, instead of failing.

The game reads the file from
`https://raw.githubusercontent.com/iam-peter/phong/main/online.json`, so
a commit to `main` is all it takes, the file can be edited right on GitHub.
No build and no CI run is needed, and nobody needs new downloads. GitHub
keeps the file cached for up to five minutes. The game asks when it starts
and whenever a lobby or the join screen opens, and it keeps the last answer
for a start without a connection.

Every player can also, under Settings, Online:

- switch internet play off,
- name an own server, which goes before the published one, e.g. a server
  on the own machine or LAN for trying things out,
- see which server is in use and whether it answers.

Builds can change where the game asks, `-DPHONG_DIRECTORY_URL=<url>`, or
not ask at all with an empty one, and give it a server to start with,
`-DPHONG_SERVER_URL=wss://...`.

## Sleeping servers

Free plans often put a service to sleep when nobody used it for a while,
Render after about 15 minutes. The next connection wakes it, which takes
up to about a minute, and the game would look broken meanwhile. So the game
asks the server for a score list as soon as a lobby, the join screen or the
online settings open, and when endless or squash starts. That already
wakes it, before anybody wants a room. If the answer takes longer than two
seconds, these screens say so, with the seconds so far and how long the
server took the last time:

> Waking up the server, 12 s, last time it took 48 s

If there is no answer at all within two minutes they say that the server
doesn't answer.

## Where the server can run

Free offers change often, this is what was known when it was written,
check the current terms before relying on one:

| Host | Always on | Keeps the high scores | Runs `phong-server` as it is | Catch |
|---|---|---|---|---|
| [Render](https://render.com), free plan | no, sleeps after about 15 minutes | no, they start over with every restart | yes, `render.yaml` and `server/Dockerfile` | the waking up, see above |
| [Koyeb](https://www.koyeb.com), [Hugging Face Spaces](https://huggingface.co/spaces) (Docker) | no, they sleep too | no | yes, `server/Dockerfile` | like Render |
| [Oracle Cloud Always Free](https://www.oracle.com/cloud/free/) | yes | yes | yes, a virtual machine | a card for the sign-up, free machines are sometimes out of stock, TLS by hand |
| An own machine with a [Cloudflare Tunnel](https://developers.cloudflare.com/cloudflare-one/connections/connect-networks/) | while it is on | yes | yes | the machine has to stay on |
| [Cloudflare Workers with Durable Objects](https://developers.cloudflare.com/durable-objects/) | yes | yes | no, the server would need a rewrite in JavaScript | about 200 lines speaking the same messages |

Fly.io and Railway no longer have a free plan that lasts.

The browser version comes from an HTTPS page, so it can only reach the
server over TLS, `wss://`. Render, Koyeb, Hugging Face and Cloudflare add
the TLS, on a virtual machine a proxy like Caddy does it.

### Render

1. On [render.com](https://render.com): New, Blueprint, and this
   repository. Render reads [`render.yaml`](../render.yaml) and builds the
   service from `server/Dockerfile`, on the free plan.
2. When it is up, Render shows its address, e.g.
   `https://phong-server.onrender.com`. The game uses it as
   `wss://phong-server.onrender.com`.
3. Try it first without changing anything for others: Settings, Online,
   Own server, the `wss://` address. Open 2 Players, `Online open` should
   show a room code, maybe after the server woke up.
4. Then publish it for everybody in [`online.json`](../online.json).

The free plan has no disk that lasts, the high scores start over whenever
the service restarts or wakes up. A paid plan with a disk keeps them, run
the server with `--scores /data/scores.json` on it.

### A virtual machine, e.g. Oracle Cloud

The Linux download of the nightly, `Phong-server-linux-x64.tar.gz`, runs as
it is, the Qt it needs comes along. On an ARM machine, like the larger free
ones at Oracle, build it with the Qt of the distribution instead, see
[Server](../README.md#server). Then:

```
./phong-server --port 45460 --scores scores.json
```

and in front of it Caddy, which gets a certificate on its own, with a name
pointing to the machine, e.g. a free one from DuckDNS:

```
phong.example.com {
    reverse_proxy localhost:45460
}
```

The address for the game is then `wss://phong.example.com`.

### An own machine with a Cloudflare Tunnel

`cloudflared` connects the machine to Cloudflare, which gives it a public
`https://` name without opening any ports, and passes the WebSockets on to
`phong-server` on the machine. Run the server as above and a tunnel to
`http://localhost:45460`, the game then uses `wss://` and the tunnel's
name.

## Without CI

Nothing here needs the CI:

- `online.json` is a file in the repository, see above.
- `phong-server` builds with the desktop build, or on its own, see
  [Server](../README.md#server), and Render builds its own image.
- The game builds locally for the desktop and the browser, see
  [Build](../README.md#build). The browser build can be put on any static
  web host, it only needs `phong.html`, `phong.js`, `phong.wasm`,
  `qtloader.js` and `qtlogo.svg`.

The nightly only saves doing that by hand. GitHub doesn't count the
minutes of standard runners for public repositories, if Actions stopped
for lack of minutes the repository or its runners may be set up
otherwise, see Settings, Billing and plans.

## Trying it on the own machine

```
./build/desktop/server/phong-server --port 45460
```

and in the game Settings, Online, Own server `127.0.0.1:45460`. Two
instances of the game on the same machine can then play through it, one
opens a room, the other joins with the code. `--lag 100` holds every
message back for 100 ms, to see how the game feels with a slow connection.
