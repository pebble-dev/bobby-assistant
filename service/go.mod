module github.com/pebble-dev/bobby-assistant/service

go 1.23.0

toolchain go1.23.3

require (
	github.com/google/uuid v1.6.0
	github.com/honeycombio/beeline-go v1.18.0
	github.com/joho/godotenv v1.5.1
	github.com/redis/go-redis/v9 v9.7.1
	github.com/yuin/gopher-lua v1.1.1
	golang.org/x/exp v0.0.0-20250228200357-dead58393ab7
	golang.org/x/text v0.22.0
	google.golang.org/api v0.223.0
	google.golang.org/genai v0.4.0
	nhooyr.io/websocket v1.8.10
)

require (
	cloud.google.com/go v0.118.3 // indirect
	cloud.google.com/go/compute/metadata v0.6.0 // indirect
	github.com/cespare/xxhash/v2 v2.3.0 // indirect
	github.com/dgryski/go-rendezvous v0.0.0-20200823014737-9f7001d12a5f // indirect
	github.com/facebookgo/clock v0.0.0-20150410010913-600d898af40a // indirect
	github.com/facebookgo/limitgroup v0.0.0-20150612190941-6abd8d71ec01 // indirect
	github.com/facebookgo/muster v0.0.0-20150708232844-fd3d7953fd52 // indirect
	github.com/felixge/httpsnoop v1.0.4 // indirect
	github.com/google/go-cmp v0.7.0 // indirect
	github.com/gorilla/websocket v1.5.3 // indirect
	github.com/honeycombio/libhoney-go v1.25.0 // indirect
	github.com/klauspost/compress v1.18.0 // indirect
	github.com/umahmood/haversine v0.0.0-20151105152445-808ab04add26 // indirect
	github.com/vmihailenco/msgpack/v5 v5.4.1 // indirect
	github.com/vmihailenco/tagparser/v2 v2.0.0 // indirect
	golang.org/x/oauth2 v0.27.0 // indirect
	golang.org/x/sys v0.30.0 // indirect
	google.golang.org/grpc v1.70.0 // indirect
	gopkg.in/alexcesaro/statsd.v2 v2.0.0 // indirect
)

// In v1.8.11, nhooyr.io/websocket made a change that appears to break cleanly closing websockets, so we can't use it.
exclude nhooyr.io/websocket v1.8.11

// In v1.8.12, nhooyr.io/websocket moved to github.com/coder/websocket (which still hasn't fixed the closing issue)
exclude nhooyr.io/websocket v1.8.12
